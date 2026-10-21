#!/usr/bin/env python3
"""Text client for the SYN-ACK WIRE protocol: a stand-in for the 8-bit client.

    python3 testclient.py [--host H] [--port P] [--table T] [--name NAME] [--token HEX]
                          [--auto] [--start N] [--quiet]

--auto answers every prompt with a random legal move chosen ONLY from what arrived over the wire,
and cross-checks its copy of the board against the server's hive sizes. If it can play, a real client can.
--start N: when this client is the host, start the game with N seats (bots fill the rest).
"""
import argparse
import asyncio
import random
import sys

from acquire import proto, theme

NODES, NSYN = 108, 7
COLS = "ABCDEFGHIJKL"


def node_name(n):
    return f"{COLS[n % 12]}{n // 12}"


def parse_node(s):
    s = s.strip().upper()
    return int(s[1:]) * 12 + COLS.index(s[0])


def word(th, key, plural=False):
    """A themed term from theme.toml [terms] (e.g. 'coin' -> 'crypto')."""
    t = th.terms[key]
    return t.get("p", t["s"] + "s") if plural else t["s"]


def bits(m):
    return [i for i in range(8) if m >> i & 1]


class ProtocolError(Exception):
    pass


class Client:
    """Client-side state, rebuilt purely from server frames."""

    def __init__(self, th=None, rng=None):
        self.th = th or theme.load()
        self.rng = rng or random.Random()
        self.buf = bytearray()
        self.seat = None
        self.token = bytes(4)
        self.region = ""
        self.seats = [(0, "")] * 6
        self.board = [0] * NODES
        self.size = [0] * NSYN
        self.cash = [0] * 6
        self.coins = [[0] * NSYN for _ in range(6)]
        self.hand = [0xFF] * 6
        self.turn = None
        self.prompt = None          # (kind, timeout, params)
        self.logs = []
        self.faults = []
        self.winners = None
        self.coinsupply = self.th.rules["coinsupply"]

    # ------------------------------------------------------------ framing

    def feed(self, data):
        """Append bytes; return the list of complete (type, payload) frames."""
        self.buf += data
        frames = []
        while self.buf:
            t = self.buf[0]
            if t in proto.SERVER_VAR:
                if len(self.buf) < 2 or len(self.buf) < 2 + self.buf[1]:
                    break
                n, start = self.buf[1], 2
            elif t in proto.SERVER_LEN:
                n, start = proto.SERVER_LEN[t], 1
                if len(self.buf) < 1 + n:
                    break
            else:
                raise ProtocolError(f"unknown frame type {t:#x}")
            payload = bytes(self.buf[start:start + n])
            del self.buf[:start + n]
            frames.append((chr(t), payload))
            self.apply(chr(t), payload)
        return frames

    def apply(self, t, p):
        if t == "w":
            self.seat, self.token = p[0], p[1:5]
        elif t == "a":
            self.region = p.decode()
        elif t == "n":
            self.seats[p[0]] = (p[1], p[2:10].decode().rstrip())
        elif t == "s":
            self.prompt = None              # as the Atari client: a snapshot drops any open prompt
            for i in range(54):
                self.board[2 * i] = p[i] & 0x0F
                self.board[2 * i + 1] = p[i] >> 4
            self.size = list(p[54:61])
            self.cash = [p[61 + 2 * i] | p[62 + 2 * i] << 8 for i in range(6)]
            self.coins = [list(p[73 + 7 * i:80 + 7 * i]) for i in range(6)]
            self.turn = None if p[115] == 0xFF else p[115]
        elif t == "h":
            self.hand = list(p)
        elif t == "t":
            self.turn = p[0]
            self.prompt = None
        elif t == "x":
            for n in p[1:]:
                self.board[n] = p[0]
        elif t == "r":
            self.board = [p[1] if v == p[0] else v for v in self.board]
        elif t == "z":
            self.size[p[0]] = p[1]
        elif t == "m":
            self.cash[p[0]] = p[1] | p[2] << 8
        elif t == "k":
            self.coins[p[0]][p[1]] = p[2]
        elif t == "p":
            self.prompt = (chr(p[0]), p[1], p[2:])
            if self.prompt[0] != "S":
                self.verify()
        elif t == "l":
            self.logs.append(p.decode().replace("\\", self.th.game["currency"]))
        elif t == "e":
            self.winners = bits(p[0])
            self.verify()
        elif t == "f":
            self.faults.append(p[0])

    def verify(self):
        """At every prompt the state is settled: the board must agree with the hive sizes."""
        for s in range(NSYN):
            n = self.board.count(s + 2)
            if n != self.size[s]:
                raise ProtocolError(f"syndicate {s}: board shows {n} nodes, server says {self.size[s]}")
        if self.prompt and self.prompt[0] == "B":
            supply = self.prompt[2][3 + NSYN:]
            for s in range(NSYN):
                held = sum(self.coins[p][s] for p in range(6))
                if held + supply[s] != self.coinsupply:
                    raise ProtocolError(f"coin {s}: held {held} + supply {supply[s]} != {self.coinsupply}")

    # ------------------------------------------------------------ automatic play

    def auto_answer(self, start_seats=None):
        """A random legal answer to the current prompt, as a frame. Uses wire data only."""
        kind, _, p = self.prompt
        r = self.rng
        if kind == "S":
            return b"S" + bytes([start_seats]) if start_seats else None
        if kind == "P":
            ok = [n for n in self.hand if n != 0xFF and not n & 0x80]
            if not ok:
                raise ProtocolError("prompted to play with no runnable exploit in the kit")
            return proto.from_action("P", r.choice(ok))
        if kind == "F":
            return proto.from_action("F", r.choice(bits(p[0])))
        if kind == "K":
            return proto.from_action("K", r.choice(bits(p[1])))
        if kind == "D":
            _, _, held, supply = p
            swap = 2 * r.randint(0, min(held // 2, supply))
            return proto.from_action("D", (r.randint(0, held - swap), swap))
        if kind == "B":
            cash, can_end = p[0] | p[1] << 8, p[2] & 1
            prices, supply = list(p[3:3 + NSYN]), list(p[3 + NSYN:])
            buys = []
            for _ in range(r.randint(0, 3)):
                ok = [s for s in range(NSYN) if prices[s] and supply[s] and prices[s] <= cash]
                if not ok:
                    break
                s = r.choice(ok)
                buys.append(s)
                cash -= prices[s]
                supply[s] -= 1
            return proto.from_action("B", (buys, bool(can_end) and r.random() < 0.3))
        raise ProtocolError(f"unknown prompt {kind}")

    # ------------------------------------------------------------ text display

    def render(self):
        letters = [s.letter for s in self.th.syndicates]
        glyph = {0: self.th.glyphs["empty"], 1: self.th.glyphs["spike"]}
        lines = ["    " + " ".join(COLS)]
        mine = {n & 0x7F for n in self.hand if n != 0xFF}
        for r in range(9):
            row = []
            for c in range(12):
                n = r * 12 + c
                v = self.board[n]
                ch = glyph.get(v) or letters[v - 2]
                row.append(self.th.glyphs["target"] if n in mine and v == 0 else ch)
            lines.append(f" {r}  " + " ".join(row))
        lines.append("")
        cur = self.th.game["currency"]
        syn = self.th.syndicates
        for i, (kind, name) in enumerate(self.seats):
            if kind:
                tag = {1: "", 2: " (away)", 3: " (bot)"}[kind]
                held = " ".join(f"{syn[s].letter}{self.coins[i][s]}" for s in range(NSYN) if self.coins[i][s])
                mark = ">" if i == self.turn else " "
                lines.append(f"{mark}{i} {name:8}{tag:7} {cur}{self.cash[i] * 100:<7} {word(self.th, 'coin', True)}: {held or '-'}")
        safe = self.th.rules["safesize"]
        hives = [f"{syn[s].letter} {syn[s].short} {self.size[s]}{' ICE' if self.size[s] >= safe else ''}"
                 for s in range(NSYN) if self.size[s]]
        lines.append("hives: " + ("  ".join(hives) or "-"))
        kit = " ".join(node_name(n & 0x7F) + ("x" if n & 0x80 else "") for n in self.hand if n != 0xFF)
        lines.append(f"kit:   {kit}   (x = can't run now; * on the grid)")
        return "\n".join(lines)


FAULTS = {
    proto.F_VERSION: "server speaks a different protocol version",
    proto.F_FULL: "that table is full (game in progress)",
    proto.F_TOKEN: "unknown token: that seat doesn't exist",
    proto.F_ILLEGAL: "move rejected (reason above)",
}


def ask_human(c):
    """Interactive answer to the current prompt. Checks the input locally before sending.
    Returns a frame or None."""
    kind, timeout, p = c.prompt
    syn = c.th.syndicates
    cur = c.th.game["currency"]
    crypto = word(c.th, "coin", True)

    def ask(question):
        return input(question).strip().upper()

    def pick_syn(options):
        names = ", ".join(f"{s.letter}={s.short}" for s in syn if s.id in options)
        while True:
            a = ask(f"  syndicate letter [{names}]: ")
            hit = [s.id for s in syn if s.letter == a and s.id in options]
            if hit:
                return hit[0]
            print("  not one of those letters")

    def number(question):
        while True:
            a = ask(question)
            if not a:
                return 0
            if a.isdigit():
                return int(a)
            print("  enter a number")

    print(c.render())
    if kind == "S":
        while True:
            n = ask("You are the host. Seats to start with (2-6, Enter to wait): ")
            if not n:
                return None
            if n.isdigit() and 2 <= int(n) <= 6:
                return b"S" + bytes([int(n)])
            print("  2 to 6")
    print(f"--- your move ({timeout}s) ---")
    if kind == "P":
        runnable = {n for n in c.hand if n != 0xFF and not n & 0x80}
        while True:
            a = ask("  run exploit (e.g. C4): ")
            try:
                node = parse_node(a)
            except (ValueError, IndexError):
                print("  a node is a column letter A-L and a row 0-8, like C4")
                continue
            if node in runnable:
                return proto.from_action("P", node)
            in_kit = {n & 0x7F for n in c.hand if n != 0xFF}
            print(f"  {a} can't be run now" if node in in_kit else f"  {a} is not in your kit")
    if kind == "F":
        print("  INFILTRATION: broker the new hive to a syndicate")
        return proto.from_action("F", pick_syn(bits(p[0])))
    if kind == "K":
        print("  tie: pick the surviving syndicate" if p[0] == 0 else "  tie: pick which hive is absorbed next")
        return proto.from_action("K", pick_syn(bits(p[1])))
    if kind == "D":
        absorbed, absorber, held, supply = p
        a, b = syn[absorbed], syn[absorber]
        print(f"  HIVEJACK: {b.letter} {b.short} absorbs {a.letter} {a.short}. You hold {held} {a.letter} {crypto}.")
        print(f"  cash out {a.letter} {crypto}, swap 2 {a.letter} for 1 {b.letter} ({supply} {b.letter} left), "
              f"HODL the rest")
        while True:
            sell = number("  cash out how many: ")
            swap = number("  swap how many (even): ")
            if swap % 2:
                print("  swaps go 2 for 1: use an even number")
            elif sell + swap > held:
                print(f"  you only hold {held}")
            elif swap // 2 > supply:
                print(f"  only {supply} {b.letter} left: swap at most {2 * supply}")
            else:
                return proto.from_action("D", (sell, swap))
    if kind == "B":
        cash, can_end = p[0] | p[1] << 8, p[2] & 1
        prices, supply = p[3:3 + 7], p[3 + 7:]
        print(f"  buy up to 3 {crypto}. You have {cur}{cash * 100}.")
        for s in syn:
            if prices[s.id]:
                print(f"    {s.letter}  {s.short:8}  {cur}{prices[s.id] * 100:<5} {supply[s.id]:2} left")
        while True:
            letters = ask("  syndicate letters, e.g. LLB (Enter = none): ")
            buys, left, money, problem = [], list(supply), cash, None
            for ch in letters:
                hit = [s for s in syn if s.letter == ch and prices[s.id]]
                if not hit:
                    problem = f"{ch} is not a syndicate with a hive"
                    break
                s = hit[0]
                if not left[s.id]:
                    problem = f"no {s.letter} {crypto} left"
                    break
                if prices[s.id] > money:
                    problem = "not enough yen"
                    break
                buys.append(s.id)
                left[s.id] -= 1
                money -= prices[s.id]
            if len(buys) > 3:
                problem = f"at most 3 {crypto}"
            if problem:
                print(f"  {problem}")
                continue
            break
        end = bool(can_end) and ask("  call the null-route and end the game? [y/N] ") == "Y"
        return proto.from_action("B", (buys, end))
    return None


async def run(host, port, table, name, token=bytes(4), auto=False, start=None, quiet=False, rng=None,
              stop_after_game=True, need_humans=1):
    c = Client(rng=rng)
    reader, writer = await asyncio.open_connection(host, port)
    writer.write(proto.hello(table, token, name))
    loop = asyncio.get_running_loop()
    try:
        while True:
            data = await reader.read(4096)
            if not data:
                break
            for t, payload in c.feed(data):
                if t == "l" and not quiet:
                    print(payload.decode().replace("\\", c.th.game["currency"]))
                if t == "f" and not quiet:
                    print(f"[server: {FAULTS.get(payload[0], f'fault {payload[0]}')}]")
                if t == "p" and not auto:
                    answer = await loop.run_in_executor(None, ask_human, c)
                    if answer:
                        writer.write(answer)
            if c.winners is not None and stop_after_game:
                break
            if auto and c.prompt:
                if c.prompt[0] == "S" and sum(1 for k, _ in c.seats if k == 1) < need_humans:
                    continue                        # host waits for the other humans to jack in
                answer = c.auto_answer(start)
                c.prompt = None
                if answer:
                    writer.write(answer)
    finally:
        writer.close()
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=6502)
    ap.add_argument("--table", type=int, default=0)
    ap.add_argument("--name", default="TESTER")
    ap.add_argument("--token", default="00000000", help="hex token to reclaim a seat")
    ap.add_argument("--auto", action="store_true")
    ap.add_argument("--start", type=int, help="as host, start with this many seats")
    ap.add_argument("--humans", type=int, default=1, help="as auto host, wait for this many humans")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    c = asyncio.run(run(a.host, a.port, a.table, a.name, bytes.fromhex(a.token), a.auto, a.start, a.quiet,
                        need_humans=a.humans))
    print(c.render())
    print(f"token {c.token.hex()}  winners {[c.seats[w][1] for w in c.winners or []]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
