"""A public server's defences: the guard's books, and a real server against scanners, scripts and garbage.
Run from server/:  python3 -m unittest -v"""
import asyncio
import random
import unittest

import testclient
from acquire import proto
from acquire.guard import Guard
from serve import Server
from sim import check

HOST = "127.0.0.1"


class Clock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now


class GuardTest(unittest.TestCase):
    def test_strikes_block_for_a_while(self):
        clock = Clock()
        g = Guard(strikes=3, window=300, block=900, clock=clock)
        g.strike("1.2.3.4", "a")
        g.strike("1.2.3.4", "b")
        self.assertTrue(g.admit("1.2.3.4"))
        g.release("1.2.3.4")
        with self.assertLogs("synack", "WARNING"):
            g.strike("1.2.3.4", "c")
        self.assertFalse(g.admit("1.2.3.4"))
        self.assertTrue(g.admit("5.6.7.8"))                 # others are unaffected
        clock.now = 901
        self.assertTrue(g.admit("1.2.3.4"))                 # the block ends

    def test_old_strikes_are_forgotten(self):
        clock = Clock()
        g = Guard(strikes=3, window=300, clock=clock)
        g.strike("1.2.3.4", "a")
        g.strike("1.2.3.4", "b")
        clock.now = 301
        g.strike("1.2.3.4", "c")                            # the first two are out of the window
        self.assertTrue(g.admit("1.2.3.4"))

    def test_connection_caps(self):
        g = Guard(per_address=2, total=3)
        self.assertTrue(g.admit("a"))
        self.assertTrue(g.admit("a"))
        self.assertFalse(g.admit("a"))                      # per address
        self.assertTrue(g.admit("b"))
        self.assertFalse(g.admit("c"))                      # in all
        g.release("a")
        self.assertTrue(g.admit("c"))
        self.assertEqual(g.open, 3)


class ServerGuardTest(unittest.IsolatedAsyncioTestCase):
    async def start(self, **kw):
        kw.setdefault("guard", Guard())
        self.server = Server(timeout=5.0, bot_delay=0, seed=42, **kw)
        self.srv = await self.server.serve(HOST, 0)
        self.port = self.srv.sockets[0].getsockname()[1]

    async def asyncTearDown(self):
        self.srv.close()
        self.srv.close_clients()
        await self.srv.wait_closed()

    async def rest(self, reader):
        """Everything the server sends until it closes the connection (a reset counts as closing: the server
        closed with our data still unread)."""
        got = b""
        try:
            while data := await asyncio.wait_for(reader.read(4096), 5):
                got += data
        except ConnectionError:
            pass
        return got

    async def test_silent_connection_is_closed(self):
        await self.start(hello_timeout=0.2)
        r, w = await asyncio.open_connection(HOST, self.port)
        with self.assertLogs("synack", "INFO") as cm:
            await self.rest(r)
        self.assertTrue(any("said nothing" in m for m in cm.output))
        w.close()

    async def test_scanner_is_blocked_and_real_client_still_served_elsewhere(self):
        await self.start()
        for _ in range(3):                                  # three HTTP probes: three strikes
            r, w = await asyncio.open_connection(HOST, self.port)
            w.write(b"GET / HTTP/1.0\r\n\r\n")
            self.assertEqual(await self.rest(r), b"")
            w.close()
        r, w = await asyncio.open_connection(HOST, self.port)
        w.write(proto.hello(0, bytes(4), "LATER"))         # blocked: closed without a word, even a good hello
        self.assertEqual(await self.rest(r), b"")
        w.close()
        self.assertNotIn(0, self.server.tables)            # nothing was set up for it

    async def test_too_many_connections_from_one_address(self):
        await self.start(guard=Guard(per_address=2))
        held = [await asyncio.open_connection(HOST, self.port) for _ in range(2)]
        for _, w in held:
            w.write(proto.info(0))
        await asyncio.sleep(0.1)
        r, w = await asyncio.open_connection(HOST, self.port)
        w.write(proto.info(0))
        self.assertEqual(await self.rest(r), b"")
        w.close()
        for _, w in held:
            w.close()

    async def test_only_configured_tables(self):
        await self.start(tables=(0,))
        r, w = await asyncio.open_connection(HOST, self.port)
        w.write(proto.hello(9, bytes(4), "ODD"))
        self.assertEqual(await self.rest(r), b"")
        self.assertEqual(list(self.server.tables), [])
        w.close()

    async def test_handle_is_cleaned_on_arrival(self):
        await self.start()
        r, w = await asyncio.open_connection(HOST, self.port)
        w.write(b"H" + bytes([proto.VERSION, 0]) + bytes(4) + b"\x1b[2Jb o".ljust(8))
        c = testclient.Client()
        while c.prompt is None:
            c.feed(await asyncio.wait_for(r.read(4096), 5))
        self.assertEqual(self.server.table(0).seats[0].name, "[2JBO")
        w.close()

    async def test_garbage_does_not_hurt_the_server(self):
        """Random frames, valid types with random payloads and random bytes, from many connections, in the lobby
        and in games; then the server must still play a real game to the end, and every table be consistent."""
        await self.start(guard=Guard(strikes=10**6, per_address=1000, total=1000), tables=(0, 1, 2))
        errors = []
        asyncio.get_running_loop().set_exception_handler(lambda loop, ctx: errors.append(ctx))
        rng = random.Random(1)
        types = list(proto.CLIENT_LEN)

        async def fuzz(i):
            r, w = await asyncio.open_connection(HOST, self.port)
            if rng.random() < 0.8:
                w.write(proto.hello(rng.choice((0, 1)), bytes(4), f"FUZZ{i}"))
                w.write(b"S" + bytes([rng.randint(2, 6)]))   # a host starts a game; later ones jack in to it
            for _ in range(rng.randrange(1, 40)):
                if rng.random() < 0.9:
                    t = rng.choice(types)
                    w.write(t + bytes(rng.randrange(256) for _ in range(proto.CLIENT_LEN[t])))
                else:
                    w.write(bytes(rng.randrange(256) for _ in range(rng.randrange(1, 20))))
                await asyncio.sleep(rng.random() * 0.01)
            await asyncio.sleep(0.05)
            w.close()

        games = set()

        async def watch():                                  # the fuzz must reach games, not just the lobby
            while True:
                games.update(id(t.game) for t in self.server.tables.values() if t.game is not None)
                await asyncio.sleep(0.01)

        watcher = asyncio.create_task(watch())
        for _ in range(4):
            await asyncio.gather(*(fuzz(i) for i in range(30)))
        watcher.cancel()
        self.assertGreater(len(games), 1)
        # a real game on a fresh table: the fuzzed ones may still be mid-game, their seats with the bots
        c = await asyncio.wait_for(testclient.run(HOST, self.port, 2, "SOLO", auto=True, quiet=True,
                                                  rng=random.Random(2), start=2), 60)
        self.assertIsNotNone(c.winners)
        for t in self.server.tables.values():
            if t.game is not None:
                check(t.game)
        self.assertEqual(errors, [])
