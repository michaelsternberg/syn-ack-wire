#!/usr/bin/env python3
"""SYN-ACK WIRE TCP server.

    python3 serve.py [--host 0.0.0.0] [--port 6502] [--timeout 60] [--bot-delay 1.0]
                    [--think-exploit 20,10] [--think-buy 12,5] [--keepalive 10] [--abandon 600] [--afk 2]
                    [--link-timeout 60] [--host-wait 60] [--trace]

Clients connect with FujiNet N:TCP://host:port/ and speak docs/protocol.md.
"""
import argparse
import asyncio
import logging
import socket

from acquire import proto, theme
from acquire.table import Conn, Table

log = logging.getLogger("synack")


class Server:
    def __init__(self, timeout=60.0, bot_delay=1.0, seed=None, keepalive=10.0, think=None, abandon=600.0,
                 afk=2, link_timeout=60.0, host_wait=60.0):
        self.theme = theme.load()
        self.timeout = timeout
        self.bot_delay = bot_delay
        self.think = think
        self.abandon = abandon
        self.afk = afk
        self.keepalive = keepalive
        self.link_timeout = link_timeout
        self.host_wait = host_wait
        self.seed = seed
        self.tables = {}

    def table(self, tid):
        if tid not in self.tables:
            self.tables[tid] = Table(tid, self.theme, self.timeout, self.bot_delay, self.seed, self.think,
                                     self.abandon, self.afk, self.host_wait)
        return self.tables[tid]

    async def handle(self, reader, writer):
        peer = writer.get_extra_info("peername")
        conn = Conn(writer)
        self.watch_link(writer)
        seat = table = beat = None
        try:
            t = await reader.readexactly(1)
            if t != b"H":
                return
            ver, tid, token, name = proto.parse_hello(await reader.readexactly(proto.CLIENT_LEN[b"H"]))
            if ver != proto.VERSION:
                conn.send(proto.fault(proto.F_VERSION))
                return
            table = self.table(tid)
            seat = table.join(conn, token, name)
            if isinstance(seat, int):
                conn.send(proto.fault(seat))
                seat = None
                return
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
    ap.add_argument("--trace", action="store_true", help="log every frame a client sends, as well as the game log")
    a = ap.parse_args()
    logging.basicConfig(level=logging.DEBUG if a.trace else logging.INFO, format="%(asctime)s %(message)s")
    logging.getLogger("asyncio").setLevel(logging.WARNING)
    think = {k: t for k, t in (("P", a.think_exploit), ("B", a.think_buy)) if t}
    srv = await Server(a.timeout, a.bot_delay, keepalive=a.keepalive, think=think,
                       abandon=a.abandon, afk=a.afk, link_timeout=a.link_timeout,
                       host_wait=a.host_wait).serve(a.host, a.port)
    async with srv:
        await srv.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
