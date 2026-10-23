#!/usr/bin/env python3
"""SYN-ACK WIRE TCP server.

    python3 serve.py [--host 0.0.0.0] [--port 6502] [--timeout 60] [--bot-delay 1.0]
                    [--think-exploit 20,10] [--think-buy 12,5] [--keepalive 10] [--abandon 600] [--afk 2]
                    [--link-timeout 60] [--host-wait 60] [--region NEO-KANSAI-1] [--no-jack-in] [--trace]
                    [--lobby TCP://host:port/ --lobby-client TNFS://host/path.atr [--lobby-key 11]
                     [--lobby-region us] [--lobby-url https://lobby.fujinet.online]]

    python3 serve.py --config servers.toml [--host 0.0.0.0] [--trace]

Clients connect with FujiNet N:TCP://host:port/ and speak docs/protocol.md. With --lobby, the server lists
table 0 (the one clients join) in the FujiNet game lobby. With --config, one process runs several servers, each
on its own port with its own settings and its own lobby entry (servers.example.toml shows how).
"""
import argparse
import asyncio
import contextvars
import logging
import signal
import socket
import tomllib
import urllib.parse
from dataclasses import dataclass, field

from acquire import lobby, proto, theme
from acquire.guard import Guard
from acquire.table import Conn, Table

log = logging.getLogger("synack")
# The server a log line comes from, when one process runs several: set for each connection, and inherited by
# everything it starts (its table's game loop, timers), so their lines carry it too.
serving = contextvars.ContextVar("serving", default="")


class ServerName(logging.Filter):
    def filter(self, record):
        record.server = serving.get()
        return True

REGION_MAX = 12     # the Atari title bar's room for it
LOBBY_NAME_MAX = 32 # the lobby's `server` field: the region, " - ", and the description
LOBBY_TABLE = 0     # the table clients join (client config.h TABLE)
HELLO_TIMEOUT = 10.0    # seconds a new connection has to send its first frame (a client sends it at once)
FRAME_TIMEOUT = 10.0    # seconds a frame's payload may trail its type byte


class Server:
    def __init__(self, timeout=60.0, bot_delay=1.0, seed=None, keepalive=10.0, think=None, abandon=600.0,
                 afk=2, link_timeout=60.0, host_wait=60.0, region=None, jack_in=True, tables=(LOBBY_TABLE,),
                 guard=None, hello_timeout=HELLO_TIMEOUT, frame_timeout=FRAME_TIMEOUT):
        self.theme = theme.load()
        if region:
            self.theme.game["region"] = region
        self.timeout = timeout
        self.bot_delay = bot_delay
        self.think = think
        self.abandon = abandon
        self.afk = afk
        self.keepalive = keepalive
        self.link_timeout = link_timeout
        self.host_wait = host_wait
        self.jack_in = jack_in
        self.seed = seed
        self.allowed = frozenset(tables)    # the tables clients may watch or join
        self.guard = guard or Guard()
        self.hello_timeout = hello_timeout
        self.frame_timeout = frame_timeout
        self.tables = {}
        self.lobby = None
        self.writers = set()        # every open connection, to close them all on shutdown

    def table(self, tid):
        if tid not in self.tables:
            t = self.tables[tid] = Table(tid, self.theme, self.timeout, self.bot_delay, self.seed, self.think,
                                         self.abandon, self.afk, self.host_wait, self.jack_in)
            if tid == LOBBY_TABLE:
                t.on_news = self._listing
        return self.tables[tid]

    def list_in(self, lobby):
        """Keep the game lobby's entry (a lobby.Lobby) up to date with the table clients join."""
        self.lobby = lobby
        self._listing(self.table(LOBBY_TABLE))

    def _listing(self, table):
        if self.lobby:
            self.lobby.players(*table.players())

    async def handle(self, reader, writer):
        serving.set(self.theme.game["region"] + " ")    # this connection's task: its log lines name the server
        peer = writer.get_extra_info("peername")
        addr = peer[0] if peer else "?"
        if not self.guard.admit(addr):              # blocked, or too many connections: closed without a word
            writer.close()
            return
        self.writers.add(writer)
        try:
            await self._serve(reader, writer, peer, addr)
        finally:
            self.writers.discard(writer)
            self.guard.release(addr)

    async def _serve(self, reader, writer, peer, addr):
        conn = Conn(writer)
        self.watch_link(writer)
        seat = table = watched = None
        beat = None

        async def payload(n):                       # a frame's payload follows its type byte at once
            return await asyncio.wait_for(reader.readexactly(n), self.frame_timeout) if n else b""

        try:
            try:                                    # a client says something at once; a scanner may not
                t = await asyncio.wait_for(reader.readexactly(1), self.hello_timeout)
            except TimeoutError:
                self.guard.strike(addr, "said nothing")
                return
            beat = asyncio.create_task(self.heartbeat(conn))   # from the title screen on: a dead one is found there too
            while seat is None:
                # before joining: `I` watches a table's status (a client at its title screen), `H` joins
                if t not in (b"I", b"H"):
                    self.guard.strike(addr, f"opened with {t!r}")
                    return
                if t == b"I":
                    ver, tid = proto.parse_info(await payload(proto.CLIENT_LEN[b"I"]))
                    if tid not in self.allowed:
                        self.guard.strike(addr, f"asked for table {tid}")
                        return
                    if ver == proto.VERSION:            # otherwise its `H` gets fault 1
                        if watched:
                            watched.unwatch(conn)
                        watched = self.table(tid)
                        watched.watch(conn)
                    t = await reader.readexactly(1)     # a title screen may wait as long as its player types
                    continue
                ver, tid, token, name = proto.parse_hello(await payload(proto.CLIENT_LEN[b"H"]))
                if ver != proto.VERSION:
                    conn.send(proto.fault(proto.F_VERSION))
                    return
                if tid not in self.allowed:
                    self.guard.strike(addr, f"asked for table {tid}")
                    return
                if watched:
                    watched.unwatch(conn)
                table = self.table(tid)
                seat = table.join(conn, token, name)
                if isinstance(seat, int):
                    conn.send(proto.fault(seat))
                    code, seat = seat, None
                    if code == proto.F_TOKEN:
                        self.guard.strike(addr, "sent a token no seat has")
                    if code not in (proto.F_FULL, proto.F_HANDLE):
                        return
                    watched = table                     # back to its title screen: still connected, watching
                    table.watch(conn)
                    t = await reader.readexactly(1)
            log.info("%s joined table %d as %s", peer, tid, seat.name)
            while True:
                t = await reader.readexactly(1)
                n = proto.CLIENT_LEN.get(t)
                if n is None:
                    self.guard.strike(addr, f"{seat.name} sent unknown type {t!r}")
                    return
                data = await payload(n)
                log.debug("%s (%s) sent %s %s", peer, seat.name, t.decode(), data.hex(" "))
                table.receive(seat, t, data)
                await writer.drain()
        except TimeoutError:                                # a payload that never came
            self.guard.strike(addr, "stalled in the middle of a frame")
        except (asyncio.IncompleteReadError, OSError):     # OSError: reset, or the link timed out
            pass
        finally:
            if watched:
                watched.unwatch(conn)
            if beat:
                beat.cancel()
            if seat is not None:
                log.info("%s (%s) disconnected", peer, seat.name)
                table.disconnect(seat, conn)
            conn.close()

    def watch_link(self, writer):
        """Notice a client that vanished without closing (power off, Wi-Fi gone) in about link_timeout seconds.
        TCP keepalive can't do it: the kernel only probes idle connections, and the heartbeat keeps ours busy.
        Instead TCP_USER_TIMEOUT drops a connection whose sent data goes unacknowledged that long; Linux's
        default retransmission limit is about 15 minutes. Elsewhere, without the option, that default stays."""
        sock = writer.get_extra_info("socket")
        if self.link_timeout and sock is not None and hasattr(socket, "TCP_USER_TIMEOUT"):
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_USER_TIMEOUT, int(self.link_timeout * 1000))

    async def heartbeat(self, conn):
        """A `u` frame every few seconds, so a client can tell a quiet table from a dead link."""
        while conn.alive:
            await asyncio.sleep(self.keepalive)
            conn.send(proto.keepalive())

    async def serve(self, host, port):
        srv = await asyncio.start_server(self.handle, host, port)
        log.info("%s listening on %s", self.theme.game["region"], ", ".join(str(s.getsockname()) for s in srv.sockets))
        return srv


def think_time(text):
    """"MEAN,JITTER" in seconds, e.g. "20,10" for anywhere from 10 to 30, all equally likely."""
    try:
        mean, jitter = (float(x) for x in text.split(","))
    except ValueError:
        raise argparse.ArgumentTypeError(f"expected MEAN,JITTER in seconds, not {text!r}")
    if jitter < 0 or mean - jitter < 0:
        raise argparse.ArgumentTypeError(f"{text!r}: need 0 <= JITTER <= MEAN")
    return mean, jitter


def table_list(text):
    try:
        tables = tuple(int(t) for t in text.split(","))
    except ValueError:
        tables = ()
    if not tables or not all(0 <= t <= 255 for t in tables):
        raise argparse.ArgumentTypeError(f"{text!r}: need table numbers 0-255, separated by commas")
    return tables


def region_name(text):
    """The game's fictional grid region: 1-12 characters a client can show (letters, digits, punctuation)."""
    s = text.upper()
    if not 1 <= len(s) <= REGION_MAX or not all(0x20 <= ord(ch) <= 0x5F and ch != "\\" for ch in s):
        raise argparse.ArgumentTypeError(f"{text!r}: need 1-{REGION_MAX} letters, digits or punctuation")
    return s


def lobby_address(text):
    """The address the lobby hands clients: TCP://host:port/ (the client dials N: + it), at most 64 characters."""
    u = urllib.parse.urlsplit(text)
    try:
        port = u.port
    except ValueError:
        port = None
    if u.scheme.lower() != "tcp" or not u.hostname or not port or len(text) > 64:
        raise argparse.ArgumentTypeError(f"{text!r}: need TCP://host:port/, at most 64 characters")
    return text


def lobby_client(text):
    """Where the lobby boots the client disk from, e.g. TNFS://host/path.atr, at most 64 characters."""
    u = urllib.parse.urlsplit(text)
    if not u.scheme or not u.netloc or not u.path.strip("/") or len(text) > 64:
        raise argparse.ArgumentTypeError(f"{text!r}: need a URL like TNFS://host/path.atr, at most 64 characters")
    return text


def lobby_key(text):
    """The lobby's appkey number for the game: 1-255."""
    try:
        n = int(text)
    except ValueError:
        n = 0
    if not 1 <= n <= 255:
        raise argparse.ArgumentTypeError(f"{text!r}: need 1-255")
    return n


def lobby_region(text):
    """The lobby's region code: 1-2 letters, e.g. us or eu."""
    if not 1 <= len(text) <= 2 or not text.isalpha() or not text.isascii():
        raise argparse.ArgumentTypeError(f"{text!r}: need 1-2 letters, e.g. us")
    return text.lower()


def description(text):
    """What the lobby shows after the region, e.g. relaxed pace: printable ASCII."""
    if not text or not all(0x20 <= ord(ch) <= 0x7E for ch in text):
        raise argparse.ArgumentTypeError(f"{text!r}: need printable ASCII")
    return text


def lobby_name(region, desc):
    """The lobby's `server` field for a server: its region, and its description if it has one."""
    return f"{region} - {desc}" if desc else region


# ---------------------------------------------------------------- several servers: --config servers.toml

@dataclass
class Spec:
    """One server to run: its port, Server() arguments, and lobby details."""
    port: int
    args: dict = field(default_factory=dict)
    description: str = ""
    lobby_url: str = ""         # serverurl, when listed


def _seconds(lo=0.0, hi=None):
    def conv(v):
        if isinstance(v, bool) or not isinstance(v, (int, float)) or v < lo or (hi is not None and v > hi):
            raise ValueError(f"need a number of seconds{f' from {lo:g} to {hi:g}' if hi else f', at least {lo:g}'}")
        return float(v)
    return conv


def _count(v):
    if isinstance(v, bool) or not isinstance(v, int) or v < 0:
        raise ValueError("need a whole number, 0 or more")
    return v


def _flag(v):
    if not isinstance(v, bool):
        raise ValueError("need true or false")
    return v


def _text(fn):
    def conv(v):
        if not isinstance(v, str):
            raise ValueError("need a string in quotes")
        try:
            return fn(v)
        except argparse.ArgumentTypeError as e:
            raise ValueError(str(e))
    return conv


def _tables(v):
    if not isinstance(v, list) or not v or not all(isinstance(t, int) and not isinstance(t, bool) and 0 <= t <= 255
                                                   for t in v):
        raise ValueError("need a list of table numbers 0-255, e.g. [0, 1]")
    return tuple(v)


# A server's keys, named like the command-line options: Server() argument and conversion.
SERVER_KEYS = {
    "region": ("region", _text(region_name)),
    "timeout": ("timeout", _seconds(1, 255)),
    "bot-delay": ("bot_delay", _seconds()),
    "think-exploit": ("think P", _text(think_time)),
    "think-buy": ("think B", _text(think_time)),
    "keepalive": ("keepalive", _seconds(1)),
    "abandon": ("abandon", _seconds()),
    "afk": ("afk", _count),
    "link-timeout": ("link_timeout", _seconds()),
    "host-wait": ("host_wait", _seconds()),
    "jack-in": ("jack_in", _flag),
    "tables": ("tables", _tables),
}
LOBBY_KEYS = {
    "address": _text(lambda t: lobby_address(f"TCP://{t}:1/") and t),   # the host clients dial: name or IP
    "client": _text(lobby_client),
    "key": lambda v: _text(lobby_key)(str(v) if isinstance(v, int) and not isinstance(v, bool) else v),
    "region": _text(lobby_region),
    "url": _text(str),
}


def load_config(text, default_region):
    """servers.toml -> (Specs, lobby settings or None, listen address or None). ValueError says what's wrong."""
    try:
        cfg = tomllib.loads(text)
    except tomllib.TOMLDecodeError as e:
        raise ValueError(f"not valid TOML: {e}")
    for k in cfg:
        if k not in ("host", "lobby", "server"):
            raise ValueError(f"unknown setting {k!r} (expected host, [lobby] and [[server]])")
    host = cfg.get("host")
    if host is not None and not isinstance(host, str):
        raise ValueError("host: need a string in quotes")
    lob = None
    if "lobby" in cfg:
        lob = {"key": 11, "region": "us", "url": "https://lobby.fujinet.online"}
        for k, v in cfg["lobby"].items():
            if k not in LOBBY_KEYS:
                raise ValueError(f"[lobby]: unknown setting {k!r} (expected {', '.join(LOBBY_KEYS)})")
            try:
                lob[k] = LOBBY_KEYS[k](v)
            except ValueError as e:
                raise ValueError(f"[lobby] {k}: {e}")
        for k in ("address", "client"):
            if k not in lob:
                raise ValueError(f"[lobby]: needs {k}")
    servers = cfg.get("server")
    if not isinstance(servers, list) or not servers:
        raise ValueError("needs at least one [[server]]")
    specs, ports, regions = [], set(), {}
    for i, sv in enumerate(servers, 1):
        where = f"server {i}"
        port = sv.get("port")
        if isinstance(port, bool) or not isinstance(port, int) or not 0 <= port <= 65535:
            raise ValueError(f"{where}: port: need a TCP port number")
        where = f"server {i} (port {port})"
        if port and port in ports:
            raise ValueError(f"{where}: port {port} is used twice")
        ports.add(port)
        spec = Spec(port)
        think = {}
        for k, v in sv.items():
            if k in ("port",):
                continue
            if k == "description":
                try:
                    spec.description = _text(description)(v)
                except ValueError as e:
                    raise ValueError(f"{where} description: {e}")
                continue
            if k not in SERVER_KEYS:
                raise ValueError(f"{where}: unknown setting {k!r} (expected port, description, {', '.join(SERVER_KEYS)})")
            arg, conv = SERVER_KEYS[k]
            try:
                value = conv(v)
            except ValueError as e:
                raise ValueError(f"{where} {k}: {e}")
            if arg.startswith("think "):
                think[arg[-1]] = value
            else:
                spec.args[arg] = value
        if think:
            spec.args["think"] = think
        region = spec.args.get("region", default_region)
        if region in regions:
            raise ValueError(f"{where}: region {region} is also server {regions[region]}'s: players tell servers "
                             f"apart by their region")
        regions[region] = i
        name = lobby_name(region, spec.description)
        if len(name) > LOBBY_NAME_MAX:
            raise ValueError(f"{where}: {name!r} is {len(name)} characters; the lobby shows at most "
                             f"{LOBBY_NAME_MAX}: shorten the description")
        if lob:
            spec.lobby_url = f"TCP://{lob['address']}:{port}/"
        specs.append(spec)
    return specs, lob, host


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="0.0.0.0", metavar="ADDR",
                    help="address to listen on, e.g. 127.0.0.1 for this machine only (default: %(default)s, all)")
    ap.add_argument("--port", type=int, default=6502, metavar="N",
                    help="TCP port, e.g. 6599 for a second server; clients dial N:TCP://host:port/ "
                         "(default: %(default)s)")
    ap.add_argument("--timeout", type=float, default=60.0, metavar="SECONDS",
                    help="seconds a player has per decision before their bot answers, e.g. 90 for a relaxed "
                         "game; at most 255, the most a prompt can show (default: %(default)g)")
    ap.add_argument("--bot-delay", type=float, default=1.0, metavar="SECONDS",
                    help="seconds a bot waits before each decision, e.g. 0.05 for a quick test game "
                         "(default: %(default)g)")
    ap.add_argument("--think-exploit", type=think_time, metavar="MEAN,JITTER",
                    help="a bot's thinking time before running an exploit, e.g. 20,10 for 10 to 30 s "
                         "(instead of --bot-delay)")
    ap.add_argument("--think-buy", type=think_time, metavar="MEAN,JITTER",
                    help="a bot's thinking time before buying crypto, e.g. 12,5 for 7 to 17 s "
                         "(instead of --bot-delay)")
    ap.add_argument("--keepalive", type=float, default=10.0, metavar="SECONDS",
                    help="seconds between keepalive frames; keep it well under the client's 20 s NO SIGNAL, "
                         "e.g. 5 (default: %(default)g)")
    ap.add_argument("--abandon", type=float, default=600.0, metavar="SECONDS",
                    help="seconds a game waits with no player connected before it is dropped, e.g. 300 for "
                         "5 minutes; 0 = wait for ever (default: %(default)g)")
    ap.add_argument("--afk", type=int, default=2, metavar="N",
                    help="timeouts in a row before a player is on autopilot (the bot's clock, shown away), "
                         "e.g. 3; 0 = never (default: %(default)s)")
    ap.add_argument("--link-timeout", type=float, default=60.0, metavar="SECONDS",
                    help="seconds before a client that vanished without closing (switched off, Wi-Fi gone) is "
                         "dropped; keep it above --keepalive, e.g. 30; 0 = the system default, about 15 minutes "
                         "on Linux (default: %(default)g)")
    ap.add_argument("--host-wait", type=float, default=60.0, metavar="SECONDS",
                    help="seconds a host may keep other players waiting in the lobby before the role passes on, "
                         "e.g. 120; 0 = wait for ever (default: %(default)g)")
    ap.add_argument("--region", type=region_name, metavar="NAME",
                    help="the fictional grid region the game is set in, shown in the title bar and the game log, "
                         "so players can name the server they'll meet on, e.g. NEO-LAGOS-3; at most 12 characters "
                         "(default: %s from theme.toml)" % theme.load().game["region"])
    ap.add_argument("--description", type=description, metavar="TEXT",
                    help="what the FujiNet lobby shows after the region, e.g. \"relaxed pace\"; region, \" - \" and "
                         "this at most %d characters" % LOBBY_NAME_MAX)
    ap.add_argument("--no-jack-in", dest="jack_in", action="store_false",
                    help="turn a newcomer away from a game in progress (TABLE FULL) instead of letting them take "
                         "over a bot seat, holdings and all, e.g. for a game among friends")
    ap.add_argument("--tables", type=table_list, default=(LOBBY_TABLE,), metavar="N,N",
                    help="the tables clients may join, e.g. 0,1 for clients built with TABLE=1 too; any other "
                         "table is refused (default: 0)")
    ap.add_argument("--config", metavar="FILE",
                    help="run the servers a TOML file describes, each on its own port with its own settings and "
                         "lobby entry, instead of one from the options above (see servers.example.toml); only "
                         "--host and --trace go with it")
    ap.add_argument("--trace", action="store_true", help="log every frame a client sends, as well as the game log")
    g = ap.add_argument_group("FujiNet game lobby", "list this server in the lobby, so players can pick it from "
                              "their Atari; --lobby and --lobby-client turn it on")
    g.add_argument("--lobby", type=lobby_address, metavar="TCP://HOST:PORT/",
                   help="the public address players' Ataris dial, as the lobby gives it to the client, e.g. "
                        "TCP://wire.example.com:6502/")
    g.add_argument("--lobby-client", type=lobby_client, metavar="URL",
                   help="where the lobby boots the Atari client disk (synack.atr) from, e.g. "
                        "TNFS://tnfs.example.com/synack.atr")
    g.add_argument("--lobby-key", type=lobby_key, default=11, metavar="N",
                   help="the game's AppKey number in the lobby, 1-255; it must match the client's LOBBY_KEY "
                        "(default: %(default)s)")
    g.add_argument("--lobby-region", type=lobby_region, default="us", metavar="CODE",
                   help="the real-world region the lobby shows for the server, 1-2 letters, e.g. eu "
                        "(default: %(default)s)")
    g.add_argument("--lobby-url", default="https://lobby.fujinet.online", metavar="URL",
                   help="the lobby itself, e.g. https://qalobby.fujinet.online to try a listing out "
                        "(default: %(default)s)")
    a = ap.parse_args()
    default_region = theme.load().game["region"]
    if a.config:
        given = [d for d in vars(a) if d not in ("config", "host", "trace") and getattr(a, d) != ap.get_default(d)]
        if given:
            ap.error(f"--config replaces the other options: drop --{given[0].replace('_', '-')}, or set it in the file")
        try:
            with open(a.config, encoding="utf-8") as f:
                specs, lob, host = load_config(f.read(), default_region)
        except (OSError, ValueError) as e:
            ap.error(f"{a.config}: {e}")
        if host and a.host == ap.get_default("host"):
            a.host = host
    else:
        if bool(a.lobby) != bool(a.lobby_client):
            ap.error("--lobby and --lobby-client go together")
        if a.description and len(lobby_name(a.region or default_region, a.description)) > LOBBY_NAME_MAX:
            ap.error(f"--description: the lobby shows at most {LOBBY_NAME_MAX} characters with the region")
        think = {k: t for k, t in (("P", a.think_exploit), ("B", a.think_buy)) if t}
        specs = [Spec(a.port, dict(timeout=a.timeout, bot_delay=a.bot_delay, keepalive=a.keepalive, think=think,
                                   abandon=a.abandon, afk=a.afk, link_timeout=a.link_timeout, host_wait=a.host_wait,
                                   region=a.region, jack_in=a.jack_in, tables=a.tables),
                      a.description or "", a.lobby or "")]
        lob = a.lobby and {"client": a.lobby_client, "key": a.lobby_key, "region": a.lobby_region, "url": a.lobby_url}
    logging.basicConfig(level=logging.DEBUG if a.trace else logging.INFO,
                        format="%(asctime)s %(server)s%(message)s" if len(specs) > 1 else "%(asctime)s %(message)s")
    for h in logging.getLogger().handlers:
        h.addFilter(ServerName())
    logging.getLogger("asyncio").setLevel(logging.WARNING)
    servers, srvs = await start(specs, a.host)
    if lob:
        list_all(servers, specs, lob)
    asyncio.get_running_loop().add_signal_handler(signal.SIGTERM, asyncio.current_task().cancel)
    try:
        await asyncio.Event().wait()            # the servers serve already; wait for Ctrl-C or SIGTERM
    finally:
        await stop(servers, srvs)


async def start(specs, host):
    """Start a Server for each Spec, all sharing one Guard (a misbehaving address is blocked on every port, and the
    connection limits count across them). Returns the Servers and their listening sockets."""
    guard = Guard()
    servers = [Server(guard=guard, **spec.args) for spec in specs]
    srvs = [await server.serve(host, spec.port) for server, spec in zip(servers, specs)]
    return servers, srvs


async def stop(servers, srvs):
    """Shut down at once, players connected or not: off the lobby's list first (DELETE), then stop listening and
    close every connection. (Waiting for the connections to close, as asyncio's serve_forever() does when it is
    cancelled, could take until the last game ends.)"""
    await asyncio.gather(*(server.lobby.close() for server in servers if server.lobby))
    for srv in srvs:
        srv.close()
    for server in servers:
        for w in list(server.writers):
            w.close()
    log.info("stopped")


def list_all(servers, specs, lob):
    """List each server in the FujiNet lobby `lob` describes, as "REGION - description", at its own address."""
    for server, spec in zip(servers, specs):
        game = server.theme.game
        server.list_in(lobby.Lobby(lob["url"], lobby.entry(game["title"], lobby_name(game["region"], spec.description),
                                                          lob["region"], spec.lobby_url, lob["client"], lob["key"])))

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except (KeyboardInterrupt, asyncio.CancelledError):     # Ctrl-C, or SIGTERM
        pass
