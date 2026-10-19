"""One game table: seats, tokens, lobby, and the asyncio loop that drives the engine.

Every seat has a bot. A human's prompt that times out is answered by that seat's bot (the seat stays human);
a dropped connection makes the seat AWAY and the bot plays every prompt until the token holder reconnects.
"""
import asyncio
import logging
import os
import random

from . import proto, rules
from .bots import HeuristicBot
from .proto import AWAY, BOT, EMPTY, HUMAN

_TAKEOVER = object()        # resolves a pending answer when the human goes away
log = logging.getLogger("synack")


class Conn:
    """Outgoing side of one TCP connection."""

    def __init__(self, writer):
        self.writer = writer

    @property
    def alive(self):
        return not self.writer.is_closing()

    def send(self, data):
        if self.alive:
            self.writer.write(data)

    def close(self):
        if self.alive:
            self.writer.close()


class Seat:
    def __init__(self):
        self.kind = EMPTY
        self.name = ""
        self.token = None
        self.conn = None
        self.bot = None

    @property
    def online(self):
        return self.kind == HUMAN and self.conn is not None and self.conn.alive


class Table:
    def __init__(self, tid, theme, timeout=60.0, bot_delay=1.0, seed=None, think=None):
        self.tid = tid
        self.theme = theme
        self.timeout = timeout
        self.bot_delay = bot_delay
        self.think = think or {}    # decision kind -> (mean, jitter): a bot's thinking time
        self.rng = random.Random(seed)
        self.seats = [Seat() for _ in range(proto.SEATS)]
        self.host = None            # Seat object
        self.game = None
        self.lobby = True
        self.task = None
        self.waiting = None         # (Seat, Future, deadline) while a human prompt is open
        self.timed_out = set()      # seats whose last prompt was answered by their bot
        self.on_game_over = None    # optional callback(table), for tests

    # -------------------------------------------------------------- connections

    def join(self, conn, token, name):
        """Returns the Seat, or a proto fault code."""
        if any(token):
            seat = next((s for s in self.seats if s.token == token), None)
            if seat is None:
                return proto.F_TOKEN
            if seat.conn is not None and seat.conn is not conn:
                seat.conn.close()
            seat.conn = conn
            seat.kind = HUMAN
            self._announce(seat)
            self.resync(seat)
            return seat
        if not self.lobby:
            return proto.F_FULL
        # after a game, bot and away seats stay on show until someone needs the chair
        seat = (next((s for s in self.seats if s.kind == EMPTY), None)
                or next((s for s in self.seats if s.kind in (BOT, AWAY)), None))
        if seat is None:
            return proto.F_FULL
        seat.kind = HUMAN
        seat.name = name.upper()[:8] or "COWBOY"
        seat.token = self._new_token()
        seat.conn = conn
        seat.bot = HeuristicBot(self.rng.random())
        if self.host is None:
            self.host = seat
        self._announce(seat)
        self.resync(seat)
        return seat

    def disconnect(self, seat, conn):
        if seat.conn is not conn:
            return                  # an old connection replaced by a reconnect
        seat.conn = None
        if self.lobby and self.game is None:
            self._free(seat)                # left before any game: nothing to come back to
            return
        seat.kind = AWAY                    # mid-game, or after one: keep the seat and token
        self._announce(seat)
        self._release(seat)
        if self.lobby and seat is self.host:
            self.host = next((s for s in self.seats if s.online), None)
            if self.host:
                self._send(self.host, proto.lobby_prompt())

    def receive(self, seat, t, payload):
        """One client frame (type already validated, payload complete)."""
        k = t.decode()
        if k == "R":
            self.resync(seat)
        elif k == "Q":
            conn = seat.conn
            seat.conn = None
            if self.lobby:
                self._free(seat)
            else:
                seat.kind = BOT
                self._announce(seat)
                self._release(seat)
            if conn:
                conn.close()
        elif k == "S":
            self._start(seat, payload[0])
        elif k in "PFKDB" and (self.lobby or self.game is None):
            self._reject(seat, self.theme.error("nodecision"))
        elif k in "PFKDB":
            w = self.waiting
            if w and w[0] is seat and self.game.pending.kind == k and not w[1].done():
                w[1].set_result(proto.to_action(k, payload))
            elif seat in self.timed_out:
                self._reject(seat, self.theme.error("late"))
            elif w and w[0] is seat:
                self._reject(seat, self.theme.error("nodecision"))    # wrong kind of answer
            else:
                self._reject(seat, self.theme.error("notyours"))
        # 'H' after the first frame is ignored

    # -------------------------------------------------------------- lobby

    def _start(self, seat, count):
        humans = [s for s in self.seats if s.kind == HUMAN]
        r = self.theme.rules
        if (not self.lobby or seat is not self.host
                or not max(len(humans), r["minplayers"]) <= count <= min(r["maxplayers"], proto.SEATS)):
            self._reject(seat, self.theme.error("nodecision") if seat is not self.host or not self.lobby
                         else self.theme.error("seats", lo=max(len(humans), r["minplayers"]), hi=r["maxplayers"]))
            if self.lobby and seat is self.host:
                self._send(seat, proto.lobby_prompt())
            return
        names = [s.name for s in humans]
        free = [b for b in self.theme.bots if b not in names]
        self.rng.shuffle(free)
        bots = []
        for i in range(count - len(humans)):
            b = Seat()
            b.kind, b.name, b.bot = BOT, free[i % len(free)], HeuristicBot(self.rng.random())
            bots.append(b)
        self.seats = humans + bots + [Seat() for _ in range(proto.SEATS - count)]
        self.game = rules.Game([s.name for s in self.seats[:count]], seed=self.rng.random(), theme=self.theme)
        events = self.game.start()
        self.lobby = False
        for s in self.seats:
            if s.online:
                self.resync(s)
        self._broadcast_logs(events)
        self.task = asyncio.get_running_loop().create_task(self._drive())

    def _free(self, seat):
        seat.kind, seat.name, seat.token, seat.conn = EMPTY, "", None, None
        self._announce(seat)
        if seat is self.host:
            self.host = next((s for s in self.seats if s.kind == HUMAN), None)
            if self.host and self.lobby:
                self._send(self.host, proto.lobby_prompt())

    # -------------------------------------------------------------- game loop

    async def _drive(self):
        g = self.game
        loop = asyncio.get_running_loop()
        while not g.over:
            d = g.pending
            seat = self.seats[d.seat]
            action = _TAKEOVER
            if seat.online:
                fut = loop.create_future()
                self.waiting = (seat, fut, loop.time() + self.timeout)
                self.timed_out.discard(seat)
                self._send(seat, proto.prompt(d, self.timeout))
                try:
                    action = await asyncio.wait_for(asyncio.shield(fut), self.timeout)
                except TimeoutError:
                    self.timed_out.add(seat)
                finally:
                    self.waiting = None
            else:
                await asyncio.sleep(self._bot_wait(d.kind))
            by_bot = action is _TAKEOVER
            if by_bot:
                action = seat.bot.decide(g, d)
            try:
                events = g.apply(d.seat, action)
            except rules.RuleError as ex:
                if by_bot:
                    raise                                           # a bot bug, not a client error
                self._reject(seat, str(ex))                         # the same prompt goes out again
                continue
            self._broadcast(events)
        self._game_over()

    def _bot_wait(self, kind):
        """Seconds a bot waits before deciding: bot_delay, or a thinking time for `kind`."""
        if kind not in self.think:
            return self.bot_delay
        mean, jitter = self.think[kind]
        return max(0.0, self.rng.triangular(mean - jitter, mean + jitter, mean))

    def _game_over(self):
        self.lobby = True
        self.task = None
        if self.host is None or not self.host.online:
            self.host = next((s for s in self.seats if s.online), None)
        if self.host:
            self._send(self.host, proto.lobby_prompt())
        if self.on_game_over:
            self.on_game_over(self)

    def _release(self, seat):
        """The human behind `seat` is gone: let the bot answer any open prompt."""
        w = self.waiting
        if w and w[0] is seat and not w[1].done():
            w[1].set_result(_TAKEOVER)

    # -------------------------------------------------------------- output

    def resync(self, seat):
        g = self.game
        idx = self.seats.index(seat)
        out = [proto.welcome(idx, seat.token), proto.area(self.theme.game["region"])]
        out += [proto.seat_info(i, s.kind, s.name) for i, s in enumerate(self.seats)]
        out.append(proto.snapshot(g, None if self.lobby or g is None else g.turn))
        if g is not None and idx < g.n:
            out.append(proto.hand(tuple((t, g.status(t)) for t in g.hands[idx])))
        if not self.lobby:
            out.append(proto.frame("t", bytes([g.turn])))
            w = self.waiting
            if w and w[0] is seat:
                remaining = max(1, w[2] - asyncio.get_running_loop().time())
                out.append(proto.prompt(g.pending, remaining))
        else:
            if g is not None and g.over:
                out.append(proto.event(rules.GameOver(g.winners)))      # result of the last game
            if seat is self.host:
                out.append(proto.lobby_prompt())
        self._send(seat, b"".join(out))

    def _broadcast(self, events):
        public = []
        for ev in events:
            if isinstance(ev, rules.Hand):
                self._send(self.seats[ev.seat], proto.hand(ev.tiles))
            else:
                public.append(proto.event(ev))
        data = b"".join(public)
        for s in self.seats:
            self._send(s, data)

    def _broadcast_logs(self, events):
        data = b"".join(proto.event(e) for e in events if isinstance(e, rules.Log))
        for s in self.seats:
            self._send(s, data)

    def _announce(self, seat):
        data = proto.seat_info(self.seats.index(seat), seat.kind, seat.name)
        for s in self.seats:
            self._send(s, data)

    def _reject(self, seat, reason):
        """Tell one player why their answer was refused: a private log line, then fault 4."""
        log.info("table %d: rejected %s: %s", self.tid, seat.name, reason)
        self._send(seat, proto.log_line(reason) + proto.fault(proto.F_ILLEGAL))

    @staticmethod
    def _send(seat, data):
        if data and seat.online:
            seat.conn.send(data)

    def _new_token(self):
        while True:
            t = os.urandom(4)
            if any(t) and all(s.token != t for s in self.seats):
                return t
