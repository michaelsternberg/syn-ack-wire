"""SYN-ACK WIRE rules engine (Acquire mechanics). Pure: no I/O, no clocks.

Usage:
    g = Game(names, seed=1)
    events = g.start()
    while not g.over:
        d = g.pending                    # Decision: who must answer what
        events = g.apply(d.seat, action) # action shape depends on d.kind (see below)

Decision kinds (same letters as the wire protocol prompts):
    'P'  run an exploit             action: node
    'F'  broker a new hive          action: syndicate id
    'K'  pick one syndicate         action: syndicate id   (data['ctx']: 0 survivor, 1 next absorbed)
    'D'  dispose of absorbed coin   action: (sell, swap)
    'B'  buy coin / call end        action: (list of up to maxbuy syndicate ids, call_end: bool)

Money is kept in units of 100 (the wire protocol's unit). Events map 1:1 onto server->client messages.
"""
import math
import random
from dataclasses import dataclass, field

from . import theme as theme_mod

ROWS, COLS = 9, 12
NODES = ROWS * COLS
EMPTY, SPIKE = 0, 1          # board cell values; hive of syndicate s is s + 2
NSYN = 7

OK, BLOCKED, PATCHED = 0, 1, 2   # exploit status


def cellval(syn):
    return syn + 2


def node_name(n):
    return "ABCDEFGHIJKL"[n % COLS] + str(n // COLS)


def _neighbors(n):
    r, c = divmod(n, COLS)
    out = []
    if r > 0:
        out.append(n - COLS)
    if r < ROWS - 1:
        out.append(n + COLS)
    if c > 0:
        out.append(n - 1)
    if c < COLS - 1:
        out.append(n + 1)
    return tuple(out)


NEIGHBORS = tuple(_neighbors(n) for n in range(NODES))


class RuleError(Exception):
    """An action that the rules do not allow. Game state is unchanged. str() is the player-facing reason."""


# ------------------------------------------------------------------ events

@dataclass(frozen=True)
class Cells:      # set these nodes to cell value `val`
    val: int
    nodes: tuple


@dataclass(frozen=True)
class Recolor:    # every node with cell value `frm` becomes `to`
    frm: int
    to: int


@dataclass(frozen=True)
class Size:
    syn: int
    size: int


@dataclass(frozen=True)
class Cash:
    seat: int
    amount: int


@dataclass(frozen=True)
class Coins:
    seat: int
    syn: int
    count: int


@dataclass(frozen=True)
class Hand:       # private to `seat`; entries are (node, status)
    seat: int
    tiles: tuple


@dataclass(frozen=True)
class Turn:
    seat: int


@dataclass(frozen=True)
class Log:
    text: str


@dataclass(frozen=True)
class GameOver:
    winners: tuple


@dataclass
class Decision:
    kind: str
    seat: int
    data: dict = field(default_factory=dict)


# ------------------------------------------------------------------ game

class Game:
    def __init__(self, names, seed=None, theme=None, replace_patched=False):
        self.theme = theme or theme_mod.load()
        r = self.theme.rules
        if not r["minplayers"] <= len(names) <= r["maxplayers"]:
            raise ValueError(f"need {r['minplayers']}-{r['maxplayers']} players")
        self.names = list(names)
        self.n = len(names)
        self.kitsize = r["kitsize"]
        self.maxbuy = r["maxbuy"]
        self.safesize = r["safesize"]
        self.endsize = r["endsize"]
        self.tier = [theme_mod.TIER_OFFSET[s.tier] for s in self.theme.syndicates]
        self.replace_patched = replace_patched
        self.rng = random.Random(seed)

        self.board = [EMPTY] * NODES
        self.size = [0] * NSYN
        self.supply = [r["coinsupply"]] * NSYN
        self.cash = [r["startcash"] // 100] * self.n
        self.coins = [[0] * NSYN for _ in range(self.n)]
        self.hands = [[] for _ in range(self.n)]
        self.bag = list(range(NODES))
        self.rng.shuffle(self.bag)
        self.dead = []               # burned / replaced exploits

        self.turn = None
        self.pending = None
        self.over = False
        self.winners = ()
        self.end_called = False
        self.idle_turns = 0          # consecutive turns with no exploit run
        self.mg = None               # merge-in-progress state
        self._ev = []

    # -------------------------------------------------------------- public

    def start(self):
        """Opening spikes decide who goes first, then kits are dealt."""
        opening = []
        for seat in range(self.n):
            node = self.bag.pop()
            self.board[node] = SPIKE
            opening.append((node, seat))
            self._emit(Cells(SPIKE, (node,)))
            self._log("spike", player=self.names[seat], node=node_name(node))
        self.turn = min(opening)[1]          # lowest row, then column: lowest node index
        for seat in range(self.n):
            self._draw(seat, self.kitsize)
        for seat in range(self.n):
            self._emit_hand(seat)
        self._begin_turn()
        return self._flush()

    def apply(self, seat, action):
        d = self.pending
        if self.over or d is None:
            raise self._err("nodecision")
        if seat != d.seat:
            raise self._err("notyours")
        handler = {"P": self._act_play, "F": self._act_found, "K": self._act_pick,
                   "D": self._act_dispose, "B": self._act_buy}[d.kind]
        handler(action)          # validates fully before changing state
        return self._flush()

    def status(self, node):
        """OK, BLOCKED (would found an 8th hive) or PATCHED (would merge two safe hives)."""
        syns, spikes = self.adjacent(node)
        if len(syns) >= 2:
            safe = sum(1 for s in syns if self.size[s] >= self.safesize)
            return PATCHED if safe >= 2 else OK
        if not syns and spikes and all(self.size[s] for s in range(NSYN)):
            return BLOCKED
        return OK

    def price(self, syn, size=None):
        """Coin price in units of 100; 0 if the hive is not on the Grid."""
        size = self.size[syn] if size is None else size
        if size < 2:
            return 0
        if size <= 5:
            bracket = size
        elif size <= 10:
            bracket = 6
        elif size <= 40:
            bracket = 7 + (size - 11) // 10
        else:
            bracket = 10
        return bracket + self.tier[syn]

    def can_call_end(self):
        active = [s for s in range(NSYN) if self.size[s]]
        if not active:
            return False
        return (any(self.size[s] >= self.endsize for s in active)
                or all(self.size[s] >= self.safesize for s in active))

    def net_worth(self, seat):
        """Cash plus current coin value (bonuses not included). Public information."""
        return self.cash[seat] + sum(self.coins[seat][s] * self.price(s) for s in range(NSYN))

    # -------------------------------------------------------------- turn flow

    def _begin_turn(self):
        if self.idle_turns >= self.n:
            # Nobody could run an exploit for a full round: the game can never progress.
            self._log("quarantine")
            self._final()
            return
        seat = self.turn
        self._emit(Turn(seat))
        hand = self.hands[seat]
        if hand and not any(self.status(t) == OK for t in hand):
            self._log("burnkit", player=self.names[seat])
            self.dead.extend(hand)
            hand.clear()
            self._draw(seat, self.kitsize)
        self._emit_hand(seat)       # statuses may have changed since the last turn
        if any(self.status(t) == OK for t in hand):
            self.pending = Decision("P", seat)
        else:
            self._log("noplay", player=self.names[seat])
            self.idle_turns += 1
            self._buy_phase()

    def _act_play(self, node):
        seat = self.turn
        if node not in self.hands[seat]:
            raise self._err("notinkit", node=node_name(node))
        if self.status(node) != OK:
            raise self._err("cantrun", node=node_name(node))
        self.hands[seat].remove(node)
        self.idle_turns = 0
        self.board[node] = SPIKE
        self._emit(Cells(SPIKE, (node,)))
        self._log("spike", player=self.names[seat], node=node_name(node))
        syns, spikes = self.adjacent(node)
        if not syns:
            if spikes:
                avail = [s for s in range(NSYN) if not self.size[s]]
                self.pending = Decision("F", seat, {"node": node, "avail": avail})
            else:
                self._buy_phase()
        elif len(syns) == 1:
            s = syns[0]
            self._absorb_spikes(node, s)
            self._log("spread", syn=self._short(s), size=self.size[s])
            self._buy_phase()
        else:
            self._start_merge(node, syns)

    def _act_found(self, syn):
        d = self.pending
        if not 0 <= syn < NSYN:
            raise self._err("notoffered")
        if syn not in d.data["avail"]:
            raise self._err("hasahive", syn=self._short(syn))
        seat = self.turn
        self._absorb_spikes(d.data["node"], syn)
        self._log("found", syn=self._short(syn))
        if self.supply[syn]:
            self._give(seat, syn, 1)
            self._log("finderscut", player=self.names[seat], syn=self._short(syn), coin=self._coin(syn))
        self._buy_phase()

    # -------------------------------------------------------------- hivejack

    def _start_merge(self, node, syns):
        self.mg = {"node": node, "chains": list(syns), "survivor": None, "done": set(), "current": None,
                   "queue": []}
        top = max(self.size[s] for s in syns)
        largest = [s for s in syns if self.size[s] == top]
        if len(largest) > 1:
            self.pending = Decision("K", self.turn, {"ctx": 0, "options": largest})
        else:
            self.mg["survivor"] = largest[0]
            self._merge_next()

    def _act_pick(self, syn):
        d = self.pending
        if syn not in d.data["options"]:
            raise self._err("notoffered")
        if d.data["ctx"] == 0:
            self.mg["survivor"] = syn
            self._merge_next()
        else:
            self._resolve_defunct(syn)

    def _merge_next(self):
        m = self.mg
        left = [s for s in m["chains"] if s != m["survivor"] and s not in m["done"]]
        if not left:
            surv = m["survivor"]
            self._absorb_spikes(m["node"], surv)
            self.mg = None
            self._buy_phase()
            return
        top = max(self.size[s] for s in left)
        tied = [s for s in left if self.size[s] == top]
        if len(tied) > 1:
            self.pending = Decision("K", self.turn, {"ctx": 1, "options": tied})
        else:
            self._resolve_defunct(tied[0])

    def _resolve_defunct(self, d):
        m = self.mg
        m["current"] = d
        self._log("hivejack", absorber=self._short(m["survivor"]), absorbed=self._short(d))
        self._payout(d)
        order = [(self.turn + i) % self.n for i in range(self.n)]
        m["queue"] = [p for p in order if self.coins[p][d]]
        self._next_dispose()

    def _next_dispose(self):
        m = self.mg
        d, surv = m["current"], m["survivor"]
        if m["queue"]:
            seat = m["queue"].pop(0)
            self.pending = Decision("D", seat, {"absorbed": d, "absorber": surv, "held": self.coins[seat][d],
                                                "supply": self.supply[surv], "price": self.price(d)})
            return
        # all holders decided: the absorbed hive joins the survivor
        self._emit(Recolor(cellval(d), cellval(surv)))
        for i, v in enumerate(self.board):
            if v == cellval(d):
                self.board[i] = cellval(surv)
        self._set_size(surv, self.size[surv] + self.size[d])
        self._set_size(d, 0)
        if any(self.coins[p][d] for p in range(self.n)):
            self._log("flatlined", syn=self._short(d), coin=self._coin(d))
        m["done"].add(d)
        m["current"] = None
        self._merge_next()

    def _act_dispose(self, action):
        d = self.pending
        sell, swap = action
        held, d_syn, surv = d.data["held"], d.data["absorbed"], d.data["absorber"]
        if swap % 2:
            raise self._err("oddswap")
        if sell < 0 or swap < 0 or sell + swap > held:
            raise self._err("toomany", n=held)
        if swap // 2 > self.supply[surv]:
            raise self._err("swapsupply", n=self.supply[surv], syn=self._short(surv))
        seat = d.seat
        name, coin, short = self.names[seat], self._coin(d_syn), self._short(d_syn)
        if sell:
            self._take(seat, d_syn, sell)
            self._set_cash(seat, self.cash[seat] + sell * self.price(d_syn))
            self._log("cashout", player=name, n=sell, syn=short, coin=coin)
        if swap:
            self._take(seat, d_syn, swap)
            self._give(seat, surv, swap // 2)
            self._log("swap", player=name, n=swap, syn=short, coin=coin)
        kept = held - sell - swap
        if kept:
            self._log("hodl", player=name, n=kept, syn=short, coin=coin)
        self._next_dispose()

    # -------------------------------------------------------------- buy / end of turn

    def _buy_phase(self):
        active = [s for s in range(NSYN) if self.size[s]]
        if not active:
            self._end_turn()
            return
        seat = self.turn
        self.pending = Decision("B", seat, {
            "cash": self.cash[seat],
            "prices": [self.price(s) for s in range(NSYN)],
            "supply": list(self.supply),
            "can_end": self.can_call_end(),
        })

    def _act_buy(self, action):
        buys, call_end = action
        buys = list(buys)
        seat = self.turn
        if len(buys) > self.maxbuy:
            raise self._err("maxbuy", n=self.maxbuy)
        if call_end and not self.can_call_end():
            raise self._err("cantend")
        cash, supply = self.cash[seat], list(self.supply)
        for s in buys:
            if not 0 <= s < NSYN or not self.size[s]:
                raise self._err("nohive")
            if not supply[s]:
                raise self._err("soldout", syn=self._short(s))
            cash -= self.price(s)
            supply[s] -= 1
            if cash < 0:
                raise self._err("nocash")
        for s in sorted(set(buys)):
            k = buys.count(s)
            self._give(seat, s, k)
            self._log("buy", player=self.names[seat], n=k, syn=self._short(s), coin=self._coin(s))
        self._set_cash(seat, cash)
        if call_end:
            self.end_called = True
            self._log("callend", player=self.names[seat])
        self._end_turn()

    def _end_turn(self):
        seat = self.turn
        self.pending = None
        if self.replace_patched:
            for t in [t for t in self.hands[seat] if self.status(t) == PATCHED]:
                if not self.bag:
                    break
                self.hands[seat].remove(t)
                self.dead.append(t)
                self._log("patched", node=node_name(t))
        self._draw(seat, self.kitsize - len(self.hands[seat]))
        self._emit_hand(seat)
        if self.end_called:
            self._log("nullroute")
            self._final()
            return
        self.turn = (seat + 1) % self.n
        self._begin_turn()

    def _final(self):
        self.pending = None
        for s in sorted((s for s in range(NSYN) if self.size[s]), key=lambda s: self.size[s]):
            self._payout(s)
        for seat in range(self.n):
            total = self.cash[seat]
            for s in range(NSYN):
                k = self.coins[seat][s]
                if k:
                    if self.size[s]:
                        total += k * self.price(s)
                    self._take(seat, s, k)
            self._set_cash(seat, total)
            self._log("final", player=self.names[seat], amount=self.theme.money(total))
        best = max(self.cash)
        self.winners = tuple(p for p in range(self.n) if self.cash[p] == best)
        for p in self.winners:
            self._log("winner", player=self.names[p])
        self.over = True
        self._emit(GameOver(self.winners))

    # -------------------------------------------------------------- helpers

    def _payout(self, syn):
        """Pay ROOT and SUDO bonuses for `syn` at its current size."""
        holders = [(self.coins[p][syn], p) for p in range(self.n) if self.coins[p][syn]]
        if not holders:
            return
        price = self.price(syn)
        root, sudo = 10 * price, 5 * price
        top = max(k for k, _ in holders)
        firsts = [p for k, p in holders if k == top]
        rest = [(k, p) for k, p in holders if k != top]
        pay = []
        if len(firsts) > 1:
            share = math.ceil((root + sudo) / len(firsts))
            pay += [("root", p, share) for p in firsts]
        elif not rest:
            pay.append(("root", firsts[0], root + sudo))
        else:
            pay.append(("root", firsts[0], root))
            second = max(k for k, _ in rest)
            seconds = [p for k, p in rest if k == second]
            share = math.ceil(sudo / len(seconds))
            pay += [("sudo", p, share) for p in seconds]
        for kind, p, amount in pay:
            self._set_cash(p, self.cash[p] + amount)
            self._log(kind, player=self.names[p], amount=self.theme.money(amount))

    def adjacent(self, node):
        """(sorted adjacent syndicate ids, whether any adjacent lone spike)."""
        syns, spikes = set(), False
        for a in NEIGHBORS[node]:
            v = self.board[a]
            if v == SPIKE:
                spikes = True
            elif v >= 2:
                syns.add(v - 2)
        return sorted(syns), spikes

    def _absorb_spikes(self, node, syn):
        """Flood-fill from `node` over unaffiliated spikes, turning them into `syn`'s hive."""
        stack, seen = [node], {node}
        while stack:
            n = stack.pop()
            for a in NEIGHBORS[n]:
                if a not in seen and self.board[a] == SPIKE:
                    seen.add(a)
                    stack.append(a)
        nodes = tuple(sorted(seen))
        for n in nodes:
            self.board[n] = cellval(syn)
        self._emit(Cells(cellval(syn), nodes))
        self._set_size(syn, self.size[syn] + len(nodes))

    def _set_size(self, syn, size):
        old = self.size[syn]
        self.size[syn] = size
        self._emit(Size(syn, size))
        if old < self.safesize <= size:
            self._log("safe", syn=self._short(syn))

    def _set_cash(self, seat, amount):
        self.cash[seat] = amount
        self._emit(Cash(seat, amount))

    def _give(self, seat, syn, k):
        self.supply[syn] -= k
        self.coins[seat][syn] += k
        self._emit(Coins(seat, syn, self.coins[seat][syn]))

    def _take(self, seat, syn, k):
        self.supply[syn] += k
        self.coins[seat][syn] -= k
        self._emit(Coins(seat, syn, self.coins[seat][syn]))

    def _draw(self, seat, k):
        for _ in range(min(k, len(self.bag))):
            self.hands[seat].append(self.bag.pop())

    def _emit_hand(self, seat):
        self._emit(Hand(seat, tuple((t, self.status(t)) for t in self.hands[seat])))

    def _short(self, syn):
        return self.theme.syndicates[syn].short

    def _coin(self, syn):
        return self.theme.syndicates[syn].coin

    def _err(self, key, **kw):
        return RuleError(self.theme.error(key, **kw))

    def _log(self, key, **kw):
        self._emit(Log(self.theme.say(key, **kw)))

    def _emit(self, ev):
        self._ev.append(ev)

    def _flush(self):
        ev, self._ev = self._ev, []
        return ev
