"""Wire protocol v3 (docs/protocol.md): frame encoding and decoding.

Server -> client types are lowercase; client -> server types are UPPERCASE and fixed length.
"""
from . import rules

VERSION = 3
SEATS = 6                                   # snapshot and seat messages always cover 6 seats
NONE_SYN = 7

# client -> server payload lengths
CLIENT_LEN = {b"I": 2, b"H": 14, b"S": 1, b"P": 1, b"F": 1, b"K": 1, b"D": 2, b"B": 3, b"R": 0, b"Q": 0}
# server -> client: variable-length types carry a length byte; the rest are fixed
SERVER_VAR = frozenset(b"ailpx")
SERVER_LEN = {ord("w"): 5, ord("n"): 10, ord("s"): 116, ord("h"): 6, ord("t"): 1, ord("r"): 2, ord("z"): 2,
              ord("m"): 3, ord("k"): 3, ord("e"): 1, ord("f"): 1, ord("u"): 0}

# seat kinds (n message)
EMPTY, HUMAN, AWAY, BOT = 0, 1, 2, 3

# fault codes (f message)
F_VERSION, F_FULL, F_TOKEN, F_ILLEGAL, F_HANDLE = 1, 2, 3, 4, 5

# table status (i message): what joining now would do
ST_LOBBY, ST_JACKIN, ST_FULL = 0, 1, 2


def text(s, limit=255):
    """Restrict to the wire character set: ASCII $20-$5F, with the currency sign sent as $5C."""
    out = bytearray()
    for ch in s.upper():
        if ch == "¥":
            out.append(0x5C)
        elif 0x20 <= ord(ch) <= 0x5F:
            out.append(ord(ch))
        else:
            out.append(ord("?"))
    return bytes(out[:limit])


def handle(s):
    return text(s, 8).ljust(8, b" ")


def frame(t, payload=b""):
    t = t.encode() if isinstance(t, str) else t
    if t[0] in SERVER_VAR:
        return t + bytes([len(payload)]) + payload
    assert len(payload) == SERVER_LEN[t[0]], (t, len(payload))
    return t + payload


def u16(n):
    n = max(0, min(n, 0xFFFF))
    return bytes([n & 0xFF, n >> 8])


def mask(ids):
    m = 0
    for i in ids:
        m |= 1 << i
    return m


# ------------------------------------------------------------------ server messages

def keepalive():
    return frame("u")


def welcome(seat, token):
    return frame("w", bytes([seat]) + token)


def seat_info(seat, kind, name):
    return frame("n", bytes([seat, kind]) + handle(name))


def snapshot(game, turn=None):
    """Board (nibbles, even node low), sizes, cash, coins, turn. `game` may be None in the lobby."""
    if game is None:
        return frame("s", bytes(115) + b"\xff")
    board = bytes(game.board[i] | (game.board[i + 1] << 4) for i in range(0, rules.NODES, 2))
    sizes = bytes(min(s, 255) for s in game.size)
    cash = b"".join(u16(game.cash[p]) if p < game.n else u16(0) for p in range(SEATS))
    coins = b"".join(bytes(game.coins[p]) if p < game.n else bytes(rules.NSYN) for p in range(SEATS))
    t = 0xFF if turn is None else turn
    return frame("s", board + sizes + cash + coins + bytes([t]))


def hand(tiles):
    """tiles: (node, status) pairs. Bit 7 = can't run now. $FF = empty slot."""
    b = [n | (0x80 if st != rules.OK else 0) for n, st in tiles][:6]
    return frame("h", bytes(b + [0xFF] * (6 - len(b))))


def event(ev):
    """Engine event -> frame, or None for events that need routing (Hand) or are handled elsewhere."""
    if isinstance(ev, rules.Cells):
        return frame("x", bytes([ev.val]) + bytes(ev.nodes))
    if isinstance(ev, rules.Recolor):
        return frame("r", bytes([ev.frm, ev.to]))
    if isinstance(ev, rules.Size):
        return frame("z", bytes([ev.syn, min(ev.size, 255)]))
    if isinstance(ev, rules.Cash):
        return frame("m", bytes([ev.seat]) + u16(ev.amount))
    if isinstance(ev, rules.Coins):
        return frame("k", bytes([ev.seat, ev.syn, ev.count]))
    if isinstance(ev, rules.Turn):
        return frame("t", bytes([ev.seat]))
    if isinstance(ev, rules.Log):
        return frame("l", text(ev.text))
    if isinstance(ev, rules.GameOver):
        return frame("e", bytes([mask(ev.winners)]))
    return None


def prompt(d, timeout):
    """Engine Decision -> 'p' frame."""
    k, data = d.kind, d.data
    if k == "P":
        params = b""
    elif k == "F":
        params = bytes([mask(data["avail"])])
    elif k == "K":
        params = bytes([data["ctx"], mask(data["options"])])
    elif k == "D":
        params = bytes([data["absorbed"], data["absorber"], data["held"], data["supply"]])
    elif k == "B":
        params = u16(data["cash"]) + bytes([(1 if data["can_end"] else 0) | (0 if data["can_buy"] else 2)]) + bytes(data["prices"]) \
                 + bytes(data["supply"])
    else:
        raise ValueError(k)
    return frame("p", k.encode() + bytes([min(int(timeout), 255)]) + params)


def lobby_prompt(timeout=0):
    return frame("p", b"S" + bytes([timeout]))


def log_line(s):
    return frame("l", text(s))


def area(s):
    return frame("a", text(s))


def fault(code):
    return frame("f", bytes([code]))


def status(code, s):
    return frame("i", bytes([code]) + text(s, 28))


# ------------------------------------------------------------------ client messages

def parse_info(payload):
    return payload[0], payload[1]           # ver, table


def parse_hello(payload):
    ver, table = payload[0], payload[1]
    token = bytes(payload[2:6])
    name = payload[6:14].decode("ascii", "replace").strip()
    return ver, table, token, name


def to_action(kind, payload):
    """Client answer payload -> engine action for Decision `kind`."""
    if kind in "PFK":
        return payload[0]
    if kind == "D":
        return payload[0], payload[1]
    if kind == "B":
        call_end = bool(payload[0] & 0x80)
        buys = [b & 0x7F for b in payload if b & 0x7F != NONE_SYN]
        return buys, call_end
    raise ValueError(kind)


def from_action(kind, action):
    """Engine action -> client answer frame (used by test clients)."""
    if kind in "PFK":
        payload = bytes([action])
    elif kind == "D":
        payload = bytes(action)
    elif kind == "B":
        buys, call_end = action
        b = list(buys) + [NONE_SYN] * (3 - len(buys))
        if call_end:
            b[0] |= 0x80
        payload = bytes(b)
    else:
        raise ValueError(kind)
    return kind.encode() + payload


def info(table):
    return b"I" + bytes([VERSION, table])


def hello(table, token, name):
    return b"H" + bytes([VERSION, table]) + token + handle(name)
