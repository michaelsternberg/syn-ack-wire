#!/usr/bin/env python3
"""SYN-ACK WIRE TCP server.

    python3 serve.py [--host 0.0.0.0] [--port 6502] [--timeout 60] [--bot-delay 1.0] [--keepalive 10] [--trace]

Clients connect with FujiNet N:TCP://host:port/ and speak docs/protocol.md.
"""
import argparse
import asyncio
import logging

from acquire import proto, theme
from acquire.table import Conn, Table

log = logging.getLogger("synack")


class Server:
    def __init__(self, timeout=60.0, bot_delay=1.0, seed=None, keepalive=10.0):
        self.theme = theme.load()
        self.timeout = timeout
        self.bot_delay = bot_delay
        self.keepalive = keepalive
        self.seed = seed
        self.tables = {}

    def table(self, tid):
        if tid not in self.tables:
            self.tables[tid] = Table(tid, self.theme, self.timeout, self.bot_delay, self.seed)
        return self.tables[tid]

    async def handle(self, reader, writer):
        peer = writer.get_extra_info("peername")
        conn = Conn(writer)
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
        except (asyncio.IncompleteReadError, ConnectionError):
            pass
        finally:
            if beat:
                beat.cancel()
            if seat is not None:
                log.info("%s (%s) disconnected", peer, seat.name)
                table.disconnect(seat, conn)
            conn.close()

    async def heartbeat(self, conn):
        """A `u` frame every few seconds, so a client can tell a quiet table from a dead link."""
        while conn.alive:
            await asyncio.sleep(self.keepalive)
            conn.send(proto.keepalive())

    async def serve(self, host, port):
        srv = await asyncio.start_server(self.handle, host, port)
        log.info("listening on %s", ", ".join(str(s.getsockname()) for s in srv.sockets))
        return srv


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=6502)
    ap.add_argument("--timeout", type=float, default=60.0, help="seconds per decision before the bot answers")
    ap.add_argument("--bot-delay", type=float, default=1.0, help="seconds a bot waits before acting")
    ap.add_argument("--keepalive", type=float, default=10.0, help="seconds between keepalive frames")
    ap.add_argument("--trace", action="store_true", help="log every frame a client sends")
    a = ap.parse_args()
    logging.basicConfig(level=logging.DEBUG if a.trace else logging.INFO, format="%(asctime)s %(message)s")
    logging.getLogger("asyncio").setLevel(logging.WARNING)
    srv = await Server(a.timeout, a.bot_delay, keepalive=a.keepalive).serve(a.host, a.port)
    async with srv:
        await srv.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
