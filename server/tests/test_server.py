"""End-to-end: real TCP server, protocol clients, bots.  Run from server/:  python3 -m unittest -v"""
import argparse
import asyncio
import json
import random
import socket
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import testclient
from acquire import lobby, proto
from serve import Server

HOST = "127.0.0.1"


class ServerTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.server = Server(timeout=5.0, bot_delay=0, seed=42, tables=(0, 7))   # 7: tables stay apart
        self.srv = await self.server.serve(HOST, 0)
        self.port = self.srv.sockets[0].getsockname()[1]

    async def asyncTearDown(self):
        self.srv.close()
        self.srv.close_clients()
        await self.srv.wait_closed()

    def client(self, name, **kw):
        kw.setdefault("auto", True)
        kw.setdefault("quiet", True)
        kw.setdefault("rng", random.Random(name))
        return testclient.run(HOST, self.port, kw.pop("table", 0), name, **kw)

    async def raw(self, table=0, token=bytes(4), name="RAW", ver=proto.VERSION):
        reader, writer = await asyncio.open_connection(HOST, self.port)
        writer.write(b"H" + bytes([ver, table]) + token + proto.handle(name))
        return reader, writer

    async def test_two_humans_two_bots_full_game(self):
        host = asyncio.create_task(self.client("ALICE", start=4, need_humans=2))
        await asyncio.sleep(0.05)
        guest = asyncio.create_task(self.client("BOB"))
        a, b = await asyncio.wait_for(asyncio.gather(host, guest), 30)
        self.assertIsNotNone(a.winners)
        self.assertEqual(a.winners, b.winners)
        self.assertEqual(a.board, b.board)                  # both clients tracked the same Grid
        self.assertEqual(a.cash, b.cash)
        self.assertEqual([k for k, _ in a.seats[:4]], [1, 1, 3, 3])
        self.assertEqual(a.faults + b.faults, [])
        self.assertTrue(any("TOP NETRUNNER" in line for line in a.logs))

    async def test_server_logs_the_game_log(self):
        with self.assertLogs("synack", "INFO") as cm:
            c = await asyncio.wait_for(self.client("SOLO", start=3), 30)
        prefix = "INFO:synack:table 0: "
        lines = [m[len(prefix):] for m in cm.output if m.startswith(prefix) and "rejected" not in m]
        self.assertEqual(lines, c.logs)                     # every public line, the bots' and the openings too
        self.assertEqual(sum("SPIKES" in line for line in lines[:3]), 3)

    async def test_reconnect_after_game_shows_result(self):
        c = await asyncio.wait_for(self.client("SOLO", start=2), 30)
        again = testclient.Client()
        r, w = await self.raw(token=c.token, name="SOLO")
        while again.winners is None:
            data = await asyncio.wait_for(r.read(4096), 5)
            self.assertTrue(data, f"connection closed; faults {again.faults}")
            again.feed(data)
        self.assertEqual(again.winners, c.winners)
        w.close()

    async def test_silent_human_is_covered_by_bot(self):
        self.server.timeout = 0.05
        t = self.server.table(0)
        t.timeout = 0.05
        reader, writer = await self.raw(name="AFK")
        c = testclient.Client()
        while c.prompt is None:
            c.feed(await reader.read(4096))
        self.assertEqual(c.prompt[0], "S")
        writer.write(b"S\x03")                             # start, then never answer again
        while c.winners is None:
            data = await asyncio.wait_for(reader.read(4096), 30)
            self.assertTrue(data)
            c.feed(data)
        self.assertEqual(c.seats[c.seat][0], proto.AWAY)    # on autopilot, and everyone can see it
        self.assertEqual(t.seats[c.seat].kind, proto.HUMAN) # but the seat is still theirs
        writer.close()

    async def test_autopilot_and_back(self):
        from acquire import table as tmod
        old, tmod.AFK_MIN = tmod.AFK_MIN, 0.25
        self.addCleanup(setattr, tmod, "AFK_MIN", old)
        self.server.timeout, self.server.bot_delay = 1.0, 0.01
        reader, writer = await self.raw(name="NAPPER")
        c = testclient.Client(rng=random.Random(1))

        async def next_prompt():
            c.prompt = None
            while c.prompt is None or c.prompt[0] == "S":
                c.feed(await asyncio.wait_for(reader.read(4096), 5))
            return c.prompt[1]                              # its clock, whole seconds

        while c.prompt is None:
            c.feed(await reader.read(4096))
        writer.write(b"S\x02")
        self.assertEqual(await next_prompt(), 1)            # the full clock; let it run out
        self.assertEqual(await next_prompt(), 1)            # twice
        self.assertEqual(await next_prompt(), 0)            # now the bot's clock
        self.assertEqual(c.seats[c.seat][0], proto.AWAY)
        self.assertIn("[?] NAPPER ON AUTOPILOT", c.logs)
        writer.write(c.auto_answer())                       # back at the keyboard
        self.assertEqual(await next_prompt(), 1)
        self.assertEqual(c.seats[c.seat][0], proto.HUMAN)
        self.assertIn("[*] NAPPER JACKS BACK IN", c.logs)
        writer.close()

    async def test_drop_and_reconnect(self):
        self.server.bot_delay = 0.01                        # bots take their time while we're away
        reader, writer = await self.raw(name="FLAKY")
        c = testclient.Client()
        while c.prompt is None:
            c.feed(await reader.read(4096))
        token, seat = c.token, c.seat
        writer.write(b"S\x02")
        while c.turn is None:
            c.feed(await reader.read(4096))
        writer.close()                                      # drop mid-game; the bot plays the seat
        await asyncio.sleep(0.05)
        t = self.server.table(0)
        self.assertEqual(t.seats[seat].kind, proto.AWAY)
        self.assertFalse(t.lobby)
        back = await asyncio.wait_for(self.client("FLAKY", token=token), 30)
        self.assertEqual(back.seat, seat)
        self.assertEqual(back.token, token)
        self.assertIsNotNone(back.winners)

    async def start_and_drop(self, name):
        """Start a 2-seat game as the only human, then drop the connection. Returns the token."""
        reader, writer = await self.raw(name=name)
        c = testclient.Client()
        while c.prompt is None:
            c.feed(await reader.read(4096))
        writer.write(b"S\x02")
        while c.turn is None:
            c.feed(await reader.read(4096))
        writer.close()
        await asyncio.sleep(0.05)
        return c.token

    async def test_game_pauses_with_no_players(self):
        with self.assertLogs("synack", "INFO") as cm:
            token = await self.start_and_drop("ALONE")
            g = self.server.table(0).game
            board, pending = list(g.board), g.pending
            await asyncio.sleep(0.2)                        # bots with no delay would finish the game
            self.assertEqual((list(g.board), g.pending), (board, pending))
            back = await asyncio.wait_for(self.client("ALONE", token=token), 30)
        self.assertIsNotNone(back.winners)                  # picked up where it stopped
        self.assertTrue(any("paused, no players" in m for m in cm.output))
        self.assertTrue(any("resumed" in m for m in cm.output))

    async def test_reclaim_away_seat_by_handle(self):
        token = await self.start_and_drop("COMEBACK")       # and the token is lost, as when the Atari is switched off
        seat = self.server.table(0).seats[0]
        back = await asyncio.wait_for(self.client("comeback"), 30)   # no token, same handle
        self.assertEqual(back.seat, 0)
        self.assertEqual(back.token, token)                 # the client now holds the seat's token again
        self.assertIsNotNone(back.winners)
        self.assertIs(self.server.table(0).seats[0], seat)

    async def test_handle_only_reclaims_away_seats(self):
        self.server.bot_delay = 0.01
        host = asyncio.create_task(self.client("HOST", start=2))
        await asyncio.sleep(0.2)                            # the game is on, HOST is connected
        r, w = await self.raw(name="HOST")                  # a second login can't take the bot seat
        self.assertEqual(await asyncio.wait_for(r.readexactly(2), 5), proto.fault(proto.F_HANDLE))
        w.close()
        await asyncio.wait_for(host, 30)

    async def test_handle_taken_in_the_lobby(self):
        ra, wa = await self.raw(name="FRED")
        a = testclient.Client()
        while a.prompt is None:
            a.feed(await asyncio.wait_for(ra.read(4096), 5))
        r, w = await self.raw(name="fred")                  # a second FRED before any game
        self.assertEqual(await asyncio.wait_for(r.readexactly(2), 5), proto.fault(proto.F_HANDLE))
        w.write(proto.hello(0, bytes(4), "FRANK"))          # still connected: another handle gets in
        b = testclient.Client()
        while b.seat is None:
            b.feed(await asyncio.wait_for(r.read(4096), 5))
        self.assertEqual(b.seat, 1)
        w.close()
        wa.close()

    async def test_handle_of_a_bot_at_the_table_is_taken(self):
        self.server.bot_delay = 0.01
        host = asyncio.create_task(self.client("HOST", start=3))
        await asyncio.sleep(0.2)                            # the game is on, two bots
        bot = next(s.name for s in self.server.table(0).seats if s.kind == proto.BOT)
        r, w = await self.raw(name=bot)                     # jacking in under a bot's name
        self.assertEqual(await asyncio.wait_for(r.readexactly(2), 5), proto.fault(proto.F_HANDLE))
        w.close()
        await asyncio.wait_for(host, 30)

    async def test_log_off_hands_the_seat_to_a_bot(self):
        self.server.timeout, self.server.bot_delay = 0.2, 0.01
        ra, wa = await self.raw(name="ALICE")
        a = testclient.Client()

        async def until(done):
            while not done():
                a.feed(await asyncio.wait_for(ra.read(4096), 5))

        await until(lambda: a.prompt is not None)
        rb, wb = await self.raw(name="BOB")
        b = testclient.Client()
        while b.seat is None:
            b.feed(await asyncio.wait_for(rb.read(4096), 5))
        await until(lambda: a.seats[1] == (proto.HUMAN, "BOB"))
        wa.write(b"S\x02")                                  # two players, no bots
        while b.turn is None:
            b.feed(await asyncio.wait_for(rb.read(4096), 5))
        wb.write(b"Q")                                      # BOB logs off
        while await asyncio.wait_for(rb.read(4096), 5):     # and is hung up on
            pass
        wb.close()
        await until(lambda: a.seats[1][0] == proto.BOT)
        bot = a.seats[1][1]
        self.assertIn(bot, self.server.theme.bots)          # a bot's name, not BOB's
        self.assertIn(f"[-] {bot} TAKES OVER FROM BOB", a.logs)
        r, w = await self.raw(token=b.token, name="BOB")    # the old token is no good
        self.assertEqual(await asyncio.wait_for(r.readexactly(2), 5), proto.fault(proto.F_TOKEN))
        w.close()
        r, w = await self.raw(name="BOB")                   # but BOB can jack in again, for the bot seat
        c = testclient.Client()
        while c.seat is None:
            c.feed(await asyncio.wait_for(r.read(4096), 5))
        self.assertEqual(c.seat, 1)
        w.close()
        wa.close()

    async def test_everyone_on_autopilot_drops_the_game(self):
        from acquire import table as tmod
        old, tmod.AFK_MIN = tmod.AFK_MIN, 0.02
        self.addCleanup(setattr, tmod, "AFK_MIN", old)
        self.server.timeout, self.server.bot_delay, self.server.afk, self.server.abandon = 0.05, 0.01, 1, 0.5
        reader, writer = await self.raw(name="DOZER")
        c = testclient.Client()
        while c.prompt is None:
            c.feed(await reader.read(4096))
        writer.write(b"S\x03")                             # start, then never answer
        with self.assertLogs("synack", "INFO") as cm:
            while not any("NOBODY PLAYING" in line for line in c.logs):
                c.feed(await asyncio.wait_for(reader.read(4096), 10))
        t = self.server.table(0)
        self.assertTrue(t.lobby)
        self.assertIsNone(t.game)
        self.assertTrue(any("every player on autopilot" in m for m in cm.output))
        self.assertIsNone(c.winners)                        # no result for a dropped game
        self.assertEqual(c.prompt[0], "S")                  # still connected, and host of the new lobby
        self.assertEqual(c.seats[c.seat], (proto.AWAY, "DOZER"))     # still on autopilot until a key is pressed
        self.assertEqual([k for k, _ in c.seats].count(proto.EMPTY), proto.SEATS - 1)
        writer.close()

    async def test_abandoned_game_frees_the_table(self):
        self.server.abandon = 0.1
        with self.assertLogs("synack", "INFO") as cm:
            token = await self.start_and_drop("GONE")
            await asyncio.sleep(0.3)
        t = self.server.table(0)
        self.assertTrue(t.lobby)
        self.assertIsNone(t.game)
        self.assertTrue(any("abandoned" in m for m in cm.output))
        r, w = await self.raw(token=token, name="GONE")     # the old seat is gone
        self.assertEqual(await asyncio.wait_for(r.read(10), 5), proto.fault(proto.F_TOKEN))
        w.close()
        c = await asyncio.wait_for(self.client("NEXT", start=2), 30)   # and the table takes a new game
        self.assertIsNotNone(c.winners)

    @unittest.skipUnless(hasattr(socket, "TCP_USER_TIMEOUT"), "Linux only")
    async def test_link_timeout_set(self):
        r, w = await self.raw(name="WIRED")
        await asyncio.wait_for(r.read(10), 5)               # joined
        sock = self.server.table(0).seats[0].conn.writer.get_extra_info("socket")
        self.assertEqual(sock.getsockopt(socket.IPPROTO_TCP, socket.TCP_USER_TIMEOUT), 60000)
        w.close()

    async def lobby_client(self, name):
        reader, writer = await self.raw(name=name)
        c = testclient.Client()

        async def until(cond):
            while not cond():
                c.feed(await asyncio.wait_for(reader.read(4096), 5))
        return c, until, writer

    async def test_idle_host_passes_on(self):
        self.server.host_wait = 1.0
        a, a_until, aw = await self.lobby_client("SLEEPY")
        await a_until(lambda: a.prompt is not None)
        self.assertEqual(a.prompt[:2], ("S", 0))            # alone: no clock
        a.prompt = None
        b, b_until, bw = await self.lobby_client("KEEN")
        await a_until(lambda: a.prompt is not None)
        self.assertEqual(a.prompt[:2], ("S", 1))            # someone is waiting: the host's clock runs
        await b_until(lambda: "[*] SLEEPY HAS 1 S TO START" in b.logs)   # and they're told who and how long
        with self.assertLogs("synack", "INFO") as cm:
            await b_until(lambda: b.prompt is not None)     # it ran out: KEEN is host now
        self.assertEqual(b.prompt[:2], ("S", 1))
        self.assertTrue(any("host passes to KEEN" in m for m in cm.output))
        await b_until(lambda: "[*] KEEN IS THE HOST NOW" in b.logs)     # the new host is told too
        await a_until(lambda: "[*] KEEN HAS 1 S TO START" in a.logs)
        self.assertLess(a.logs.index("[*] KEEN IS THE HOST NOW"), a.logs.index("[*] KEEN HAS 1 S TO START"))
        t = self.server.table(0)
        self.assertIs(t.host, t.seats[b.seat])
        bw.write(b"S\x02")                                 # and can start
        await b_until(lambda: b.turn is not None)
        self.assertFalse(t.lobby)
        aw.close()
        bw.close()

    async def test_late_arrival_hears_the_countdown(self):
        self.server.host_wait = 5.0
        a, a_until, aw = await self.lobby_client("FIRST")
        await a_until(lambda: a.prompt is not None)
        b, b_until, bw = await self.lobby_client("SECOND")
        await b_until(lambda: any("FIRST HAS" in line for line in b.logs))
        c, c_until, cw = await self.lobby_client("THIRD")
        await c_until(lambda: any(line.startswith("[*] FIRST HAS ") for line in c.logs))
        self.assertEqual(sum("FIRST HAS" in line for line in b.logs), 1)   # no repeat for those already told
        for w in (aw, bw, cw):
            w.close()

    async def test_autopilot_lasts_and_absentees_lose_the_host_role(self):
        from acquire import table as tmod
        old, tmod.AFK_MIN = tmod.AFK_MIN, 0.02
        self.addCleanup(setattr, tmod, "AFK_MIN", old)
        sv = self.server
        sv.timeout, sv.bot_delay, sv.afk, sv.abandon, sv.host_wait = 0.05, 0.01, 1, 0.5, 30.0
        a, a_until, aw = await self.lobby_client("AWAY1")
        await a_until(lambda: a.prompt is not None)
        b, b_until, bw = await self.lobby_client("AWAY2")
        await b_until(lambda: b.seat is not None)
        aw.write(b"S\x03")                                 # then neither answers again
        await a_until(lambda: any("NOBODY PLAYING" in line for line in a.logs))
        t = sv.table(0)
        ids = {s.name: s for s in t.seats if s.online}
        self.assertTrue(t._autopilot(ids["AWAY1"]) and t._autopilot(ids["AWAY2"]))   # still, in the lobby
        c, c_until, cw = await self.lobby_client("ALERT")
        with self.assertLogs("synack", "INFO") as cm:
            await c_until(lambda: c.prompt is not None and c.prompt[0] == "S")   # host at once, no 30 s wait
        self.assertTrue(any("host passes to ALERT" in m for m in cm.output))
        await a_until(lambda: "[*] ALERT HAS 30 S TO START" in a.logs)   # the old host is told, and resynced:
        self.assertIsNone(a.prompt)                         # no stale countdown left on their screen
        cw.write(b"S\x03")
        await c_until(lambda: c.turn is not None)
        shown = {name: kind for kind, name in c.seats if name}
        self.assertEqual(shown, {"AWAY1": proto.AWAY, "AWAY2": proto.AWAY, "ALERT": proto.HUMAN})
        for w in (aw, bw, cw):
            w.close()

    async def test_host_clock_stops_when_alone_again(self):
        self.server.host_wait = 5.0
        a, a_until, aw = await self.lobby_client("STAYS")
        await a_until(lambda: a.prompt is not None)
        b, b_until, bw = await self.lobby_client("LEAVES")
        a.prompt = None
        await a_until(lambda: a.prompt is not None and a.prompt[1] > 0)
        a.prompt = None
        bw.close()                                          # nobody waiting any more
        await a_until(lambda: a.prompt is not None)
        self.assertEqual(a.prompt[:2], ("S", 0))
        self.assertIsNone(self.server.table(0).host_timer)
        aw.close()

    async def test_faults(self):
        r, w = await self.raw(ver=99)
        self.assertEqual(await r.read(10), proto.fault(proto.F_VERSION))
        w.close()
        r, w = await self.raw(token=b"\x01\x02\x03\x04")
        self.assertEqual(await r.read(10), proto.fault(proto.F_TOKEN))
        w.close()
        # an answer when no prompt is open
        r, w = await self.raw(name="EAGER")
        c = testclient.Client()
        while c.prompt is None:
            c.feed(await r.read(4096))
        w.write(b"P\x05")
        while not c.faults:
            c.feed(await r.read(4096))
        self.assertEqual(c.faults, [proto.F_ILLEGAL])
        self.assertIn("NO DECISION", c.logs[-1])          # the reason arrives first
        w.close()

    async def test_illegal_move_gets_reason_and_same_prompt(self):
        r, w = await self.raw(name="CLUMSY")
        c = testclient.Client()
        while c.prompt is None:
            c.feed(await r.read(4096))
        w.write(b"S\x02")
        while c.prompt is None or c.prompt[0] != "P":
            c.prompt = None if c.prompt and c.prompt[0] == "S" else c.prompt
            c.feed(await asyncio.wait_for(r.read(4096), 5))
        kit = {n & 0x7F for n in c.hand if n != 0xFF}
        bad = next(n for n in range(108) if n not in kit and c.board[n] == 0)
        c.prompt = None
        w.write(b"P" + bytes([bad]))
        while c.prompt is None:
            c.feed(await asyncio.wait_for(r.read(4096), 5))
        self.assertEqual(c.faults, [proto.F_ILLEGAL])
        self.assertIn("IS NOT IN YOUR KIT", c.logs[-1])
        self.assertEqual(c.prompt[0], "P")                  # asked again
        w.close()

    async def test_table_full_once_started(self):
        self.server.bot_delay = 0.01
        self.server.jack_in = False
        host = asyncio.create_task(self.client("HOST", start=2, table=7))
        await asyncio.sleep(0.05)
        r, w = await self.raw(table=7, name="LATE")
        self.assertEqual(await asyncio.wait_for(r.readexactly(2), 5), proto.fault(proto.F_FULL))
        w.close()
        await asyncio.wait_for(host, 30)

    async def test_newcomer_jacks_in_for_a_bot(self):
        self.server.bot_delay = 0.01
        host = asyncio.create_task(self.client("HOST", start=3))
        await asyncio.sleep(0.2)                            # the game is on, two bots playing
        t = self.server.table(0)
        g = t.game
        self.assertIs(t._bot_seat(), t.seats[min((1, 2), key=g.net_worth)])  # the weakest
        bots = {i: t.seats[i].name for i in (1, 2)}         # which is weakest by the time LATE gets in may change
        late = await asyncio.wait_for(self.client("LATE"), 30)
        await asyncio.wait_for(host, 30)
        self.assertIn(late.seat, bots)
        self.assertEqual(late.seats[late.seat], (proto.HUMAN, "LATE"))
        i = late.logs.index(f"[+] LATE JACKS IN FOR {bots[late.seat]}")
        self.assertEqual(late.logs[i - 1], "[*] GAME IN PROGRESS: TAKING OVER BOT")
        self.assertIsNotNone(late.winners)                  # played it out from the bot's seat
        self.assertEqual(g.names[late.seat], "LATE")
        self.assertEqual(t.seats[late.seat].kind, proto.HUMAN)

    async def test_jack_in_leaves_away_seats_alone(self):
        ra, wa = await self.raw(name="ANN")
        rb, wb = await self.raw(name="BEN")
        a = testclient.Client()
        while a.prompt is None:
            a.feed(await ra.read(4096))
        wa.write(b"S\x02")                                  # two humans, no bots
        while a.turn is None:
            a.feed(await ra.read(4096))
        wb.close()                                          # BEN's seat is away, kept for BEN
        await asyncio.sleep(0.05)
        r, w = await self.raw(name="CAT")
        self.assertEqual(await asyncio.wait_for(r.readexactly(2), 5), proto.fault(proto.F_FULL))
        w.close()
        wa.close()

    async def test_lobby_players(self):
        """The game lobby's PLAYERS column: humans here of every seat in the lobby, human seats of the seats playing
        in a game (full without jack-in)."""
        t = self.server.table(0)
        self.assertEqual(t.players(), (0, 6))
        ra, wa = await self.raw(name="ANN")
        a = testclient.Client()
        while a.prompt is None:
            a.feed(await ra.read(4096))
        self.assertEqual(t.players(), (1, 6))
        wa.write(b"S\x03")                                  # ANN and two bots
        while a.turn is None:
            a.feed(await ra.read(4096))
        self.assertEqual(t.players(), (1, 3))
        t.jack_in = False
        self.assertEqual(t.players(), (3, 3))
        t.jack_in = True
        wa.close()
        await asyncio.sleep(0.05)
        self.assertEqual(t.players(), (1, 3))               # away: kept for ANN

    async def watch(self, table=0):
        """A client at its title screen: connected, watching the table's status."""
        reader, writer = await asyncio.open_connection(HOST, self.port)
        writer.write(proto.info(table))
        c = testclient.Client()

        async def news():
            c.status = None
            while c.status is None:
                c.feed(await asyncio.wait_for(reader.read(4096), 5))
            return c.status
        return reader, writer, c, news

    async def test_title_screen_status(self):
        self.server.bot_delay = 0.01
        r, w, c, news = await self.watch()
        self.assertEqual(await news(), (proto.ST_LOBBY, "LOBBY OPEN: 0 WAITING"))
        self.assertEqual(c.region, "NEO-KANSAI-1")                # the title screen shows it before joining
        ra, wa = await self.raw(name="ANN")
        self.assertEqual(await news(), (proto.ST_LOBBY, "LOBBY OPEN: 1 WAITING"))
        a = testclient.Client()
        while a.prompt is None:
            a.feed(await ra.read(4096))
        wa.write(b"S\x02")                                  # ANN and a bot
        self.assertEqual(await news(), (proto.ST_JACKIN, "IN PROGRESS: TAKE OVER A BOT"))
        w.write(proto.hello(0, bytes(4), "BOB"))            # joins on the same connection
        while c.seat is None:
            c.feed(await asyncio.wait_for(r.read(4096), 5))
        self.assertEqual(c.seat, 1)
        self.assertEqual(c.seats[1], (proto.HUMAN, "BOB"))
        w.close()
        wa.close()

    async def test_title_screen_gets_keepalives(self):
        self.server.keepalive = 0.05
        r, w, c, news = await self.watch()
        await news()                                        # the region and the status
        data = await asyncio.wait_for(r.read(4096), 5)      # then nothing changes, but the link is kept checked
        self.assertEqual(data[:1], proto.keepalive())
        w.close()

    async def test_host_hears_who_is_at_the_door(self):
        ra, wa = await self.raw(name="ANN")
        a = testclient.Client()

        async def until(done):
            while not done():
                a.feed(await asyncio.wait_for(ra.read(4096), 5))

        await until(lambda: a.prompt is not None)           # the host's `S`
        door = lambda: [line for line in a.logs if "AT THE DOOR" in line]
        r1, w1, c1, news1 = await self.watch()
        await until(lambda: door() == ["[*] 1 AT THE DOOR"])
        r2, w2, c2, news2 = await self.watch()
        await until(lambda: len(door()) == 2)
        self.assertEqual(door()[-1], "[*] 2 AT THE DOOR")
        w1.write(proto.hello(0, bytes(4), "BOB"))           # BOB comes in: a seat row, and one fewer at the door
        await until(lambda: a.seats[1] == (proto.HUMAN, "BOB") and len(door()) == 3)
        self.assertEqual(door()[-1], "[*] 1 AT THE DOOR")
        w2.close()                                          # the other one goes away
        await until(lambda: len(door()) == 4)
        self.assertEqual(door()[-1], "[*] 0 AT THE DOOR")
        w1.close()
        wa.close()

    async def test_too_late_for_the_last_bot_seat(self):
        self.server.bot_delay = 0.01
        host = asyncio.create_task(self.client("HOST", start=2))
        await asyncio.sleep(0.2)                            # the game is on, one bot seat
        r1, w1, c1, news1 = await self.watch()
        r2, w2, c2, news2 = await self.watch()
        self.assertEqual((await news1())[0], proto.ST_JACKIN)
        self.assertEqual((await news2())[0], proto.ST_JACKIN)
        w1.write(proto.hello(0, bytes(4), "FIRST"))
        self.assertEqual(await news2(), (proto.ST_FULL, "TABLE FULL"))   # pushed as soon as FIRST got it
        c2.status = None
        w2.write(proto.hello(0, bytes(4), "SECOND"))        # pressed Return before seeing that
        while c2.status is None:
            c2.feed(await asyncio.wait_for(r2.read(4096), 5))
        self.assertEqual(c2.faults, [proto.F_FULL])
        self.assertEqual(c2.status[0], proto.ST_FULL)       # still connected, back to watching
        self.assertIsNone(c2.seat)
        w1.close()
        w2.close()
        await asyncio.wait_for(host, 30)

class RegionTest(unittest.IsolatedAsyncioTestCase):
    async def test_region_option_reaches_the_client(self):
        server = Server(timeout=5.0, bot_delay=0, seed=42, region="NEO-LAGOS-3")
        srv = await server.serve(HOST, 0)
        try:
            port = srv.sockets[0].getsockname()[1]
            c = await asyncio.wait_for(testclient.run(HOST, port, 0, "ALICE", auto=True, quiet=True, start=2,
                                                      rng=random.Random(1)), 30)
            self.assertEqual(c.region, "NEO-LAGOS-3")
            self.assertTrue(any("NEO-LAGOS-3" in line for line in c.logs))   # the null-route line
        finally:
            srv.close()
            srv.close_clients()
            await srv.wait_closed()

    def test_region_names(self):
        from serve import region_name
        self.assertEqual(region_name("neo-lagos-3"), "NEO-LAGOS-3")
        for bad in ("", "THIRTEEN-CHAR", "NEO\\KANSAI", "NEO~1"):
            with self.assertRaises(argparse.ArgumentTypeError):
                region_name(bad)


class ThinkTimeTest(unittest.TestCase):
    def test_bot_wait_by_decision_kind(self):
        server = Server(bot_delay=0.5, seed=1, think={"P": (20.0, 10.0), "B": (12.0, 5.0)})
        table = server.table(0)
        exploit = [table._bot_wait("P") for _ in range(2000)]
        buy = [table._bot_wait("B") for _ in range(2000)]
        self.assertTrue(all(10.0 <= t <= 30.0 for t in exploit))
        self.assertTrue(all(7.0 <= t <= 17.0 for t in buy))
        near = sum(1 for t in exploit if 15.0 <= t <= 25.0) / len(exploit)
        self.assertLess(abs(near - 0.5), 0.05)    # even: half within half the jitter
        for kind in "FKD":                         # the small choices stay quick
            self.assertEqual(table._bot_wait(kind), 0.5)
        self.assertEqual(Server(bot_delay=0.5).table(0)._bot_wait("P"), 0.5)


class LobbyTest(unittest.IsolatedAsyncioTestCase):
    async def test_listing_follows_the_table(self):
        posts = []

        class FakeLobby(BaseHTTPRequestHandler):
            def do_POST(self):
                posts.append((self.path, json.loads(self.rfile.read(int(self.headers["Content-Length"])))))
                self.send_response(200)
                self.end_headers()

            def do_DELETE(self):
                posts.append((self.command, json.loads(self.rfile.read(int(self.headers["Content-Length"])))))
                self.send_response(204)
                self.end_headers()

            def log_message(self, *args):
                pass

        http = ThreadingHTTPServer((HOST, 0), FakeLobby)
        threading.Thread(target=http.serve_forever, daemon=True).start()
        server = Server(timeout=5.0, bot_delay=0, seed=42, region="NEO-LAGOS-3")
        srv = await server.serve(HOST, 0)

        async def listed(**want):
            for _ in range(100):
                if posts and all(posts[-1][1][k] == v for k, v in want.items()):
                    return posts[-1]
                await asyncio.sleep(0.05)
            self.fail(f"no post with {want}: {posts[-1:]}")

        try:
            server.list_in(lobby.Lobby(f"http://{HOST}:{http.server_port}",
                                       lobby.entry("SYN-ACK WIRE", "NEO-LAGOS-3", "eu", "TCP://wire.example.com:6502/",
                                                   "TNFS://tnfs.example.com/synack.atr", 11)))
            path, e = await listed(curplayers=0)
            self.assertEqual(path, "/server")
            self.assertEqual(e, {"game": "SYN-ACK WIRE", "appkey": 11, "server": "NEO-LAGOS-3", "region": "eu",
                                 "serverurl": "TCP://wire.example.com:6502/", "status": "online",
                                 "maxplayers": 6, "curplayers": 0,
                                 "clients": [{"platform": "atari", "url": "TNFS://tnfs.example.com/synack.atr"}]})
            r, w = await asyncio.open_connection(HOST, srv.sockets[0].getsockname()[1])
            w.write(proto.hello(0, bytes(4), "ANN"))
            await listed(curplayers=1, maxplayers=6)
            await server.lobby.close()
            self.assertEqual(posts[-1], ("DELETE", {"serverurl": "TCP://wire.example.com:6502/"}))
            w.close()
        finally:
            http.shutdown()
            srv.close()
            srv.close_clients()
            await srv.wait_closed()


if __name__ == "__main__":
    unittest.main()
