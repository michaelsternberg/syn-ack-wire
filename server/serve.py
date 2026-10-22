#!/usr/bin/env python3
"""SYN-ACK WIRE TCP server.

    python3 serve.py [--host 0.0.0.0] [--port 6502] [--timeout 60] [--bot-delay 1.0]
                    [--think-exploit 20,10] [--think-buy 12,5] [--keepalive 10] [--abandon 600] [--afk 2]
                    [--link-timeout 60] [--host-wait 60] [--region NEO-KANSAI-1] [--no-jack-in] [--trace]
                    [--lobby TCP://host:port/ --lobby-client TNFS://host/path.atr [--lobby-key 11]
                     [--lobby-region us] [--lobby-url https://lobby.fujinet.online]]

Clients connect with FujiNet N:TCP://host:port/ and speak docs/protocol.md. With --lobby, the server lists
table 0 (the one clients join) in the FujiNet game lobby.
"""
import argparse
import asyncio
import logging
import signal
import socket
import urllib.parse

from acquire import lobby, proto, theme
from acquire.table import Conn, Table

log = logging.getLogger("synack")

REGION_MAX = 12     # the Atari title bar's room for it
LOBBY_TABLE = 0     # the table clients join (client config.h TABLE)


class Server:
    def __init__(self, timeout=60.0, bot_delay=1.0, seed=None, keepalive=10.0, think=None, abandon=600.0,
                 afk=2, link_timeout=60.0, host_wait=60.0, region=None, jack_in=True):
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
        self.tables = {}
        self.lobby = None

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
        peer = writer.get_extra_info("peername")
        conn = Conn(writer)
        self.watch_link(writer)
        seat = table = watched = beat = None
        try:
            while seat is None:
                # before joining: `I` watches a table's status (a client at its title screen), `H` joins
                t = await reader.readexactly(1)
                if t == b"I":
                    ver, tid = proto.parse_info(await reader.readexactly(proto.CLIENT_LEN[b"I"]))
                    if ver == proto.VERSION:            # otherwise its `H` gets fault 1
                        if watched:
                            watched.unwatch(conn)
                        watched = self.table(tid)
                        watched.watch(conn)
                    continue
                if t != b"H":
                    return
                ver, tid, token, name = proto.parse_hello(await reader.readexactly(proto.CLIENT_LEN[b"H"]))
                if ver != proto.VERSION:
                    conn.send(proto.fault(proto.F_VERSION))
                    return
                if watched:
                    watched.unwatch(conn)
                table = self.table(tid)
                seat = table.join(conn, token, name)
                if isinstance(seat, int):
                    conn.send(proto.fault(seat))
                    code, seat = seat, None
                    if code not in (proto.F_FULL, proto.F_HANDLE):
                        return
                    watched = table                     # back to its title screen: still connected, watching
                    table.watch(conn)
            log.info("%s joined table %d as %s", peer, tid, seat.name)
            beat = asyncio.create_task(self.heartbeat(conn))
            while True:
                t = await reader.readexactly(1)
                n = proto.CLIENT_LEN.get(t)
                if n is None:
                    log.warning("%s sent unknown type %r; dropping", peer, t)
                    return
                payload = await reader.readexactly(n) if n else b""
                log.debug("%s (%s) sent %s %s", peer, seat.name, t.decode(), payload.hex(" "))
                table.receive(seat, t, payload)
                await writer.drain()
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
        log.info("listening on %s", ", ".join(str(s.getsockname()) for s in srv.sockets))
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
    ap.add_argument("--no-jack-in", dest="jack_in", action="store_false",
                    help="turn a newcomer away from a game in progress (TABLE FULL) instead of letting them take "
                         "over a bot seat, holdings and all, e.g. for a game among friends")
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
    if bool(a.lobby) != bool(a.lobby_client):
        ap.error("--lobby and --lobby-client go together")
    logging.basicConfig(level=logging.DEBUG if a.trace else logging.INFO, format="%(asctime)s %(message)s")
    logging.getLogger("asyncio").setLevel(logging.WARNING)
    think = {k: t for k, t in (("P", a.think_exploit), ("B", a.think_buy)) if t}
    server = Server(a.timeout, a.bot_delay, keepalive=a.keepalive, think=think,
                    abandon=a.abandon, afk=a.afk, link_timeout=a.link_timeout,
                    host_wait=a.host_wait, region=a.region, jack_in=a.jack_in)
    srv = await server.serve(a.host, a.port)
    if a.lobby:
        game = server.theme.game
        server.list_in(lobby.Lobby(a.lobby_url, lobby.entry(game["title"], game["region"], a.lobby_region,
                                                             a.lobby, a.lobby_client, a.lobby_key)))
    asyncio.get_running_loop().add_signal_handler(signal.SIGTERM, asyncio.current_task().cancel)
    try:
        async with srv:
            await srv.serve_forever()
    finally:                                    # Ctrl-C or SIGTERM: off the lobby's list (DELETE)
        if server.lobby:
            await server.lobby.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except (KeyboardInterrupt, asyncio.CancelledError):     # Ctrl-C, or SIGTERM
        pass
