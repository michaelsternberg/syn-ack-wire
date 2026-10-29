"""The short links in the game disks' QR codes (acquire/web.py).
Run from server/:  python3 -m unittest -v"""
import asyncio
import unittest

from acquire.web import Web

MANUAL = "https://example.com/docs/manual/{platform}/syn-ack-wire-manual-{platform}.pdf"
HOME = "https://example.com/syn-ack-wire"


class AnswerTest(unittest.TestCase):
    def test_answers(self):
        web = Web(MANUAL, HOME)
        cases = {
            "GET /ATARI HTTP/1.1": (302, MANUAL.format(platform="atari")),
            "GET /atari/ HTTP/1.1": (302, MANUAL.format(platform="atari")),
            "HEAD /APPLE2?x=1 HTTP/1.0": (302, MANUAL.format(platform="apple2")),
            "GET / HTTP/1.1": (302, HOME),
            "GET /../etc/passwd HTTP/1.1": (404, None),
            "GET /ATARI/MORE HTTP/1.1": (404, None),
            "POST /ATARI HTTP/1.1": (405, None),
            "hello": (400, None),
        }
        for line, want in cases.items():
            with self.subTest(line=line):
                self.assertEqual(web.answer(line), want)
        self.assertEqual(Web(MANUAL).answer("GET / HTTP/1.1"), (404, None))

    def test_seats_page(self):
        self.assertEqual(Web(MANUAL, HOME, lambda: []).answer("GET /SEATS HTTP/1.1"), (200, None))
        self.assertEqual(Web(MANUAL, HOME, lambda: []).answer("GET /seats HTTP/1.1"), (200, None))
        self.assertEqual(Web(MANUAL, HOME).answer("GET /SEATS HTTP/1.1"), (404, None))   # not a platform's manual


class ServeTest(unittest.IsolatedAsyncioTestCase):
    async def test_a_request_gets_its_redirect(self):
        srv = await Web(MANUAL, HOME).serve("127.0.0.1", 0)
        port = srv.sockets[0].getsockname()[1]
        try:
            r, w = await asyncio.open_connection("127.0.0.1", port)
            w.write(b"GET /ATARI HTTP/1.1\r\nHost: x\r\nFly-Client-IP: 203.0.113.9\r\n\r\n")
            reply = (await asyncio.wait_for(r.read(), 5)).decode()
            self.assertTrue(reply.startswith("HTTP/1.1 302 Found\r\n"))
            self.assertIn(f"Location: {MANUAL.format(platform='atari')}\r\n", reply)
            w.close()
            r, w = await asyncio.open_connection("127.0.0.1", port)
            w.write(b"X" * 5000)                    # no end of head within the limit: closed, no answer
            self.assertEqual(await asyncio.wait_for(r.read(), 5), b"")
            w.close()
        finally:
            srv.close()
            await srv.wait_closed()

    async def test_the_seats_page_lists_each_server(self):
        srv = await Web(MANUAL, HOME, lambda: [("CHIBA-GRID-7", 2, 6), ("SEOUL-NODE-2", 0, 6)]).serve("127.0.0.1", 0)
        port = srv.sockets[0].getsockname()[1]
        try:
            r, w = await asyncio.open_connection("127.0.0.1", port)
            w.write(b"GET /SEATS HTTP/1.1\r\nHost: x\r\n\r\n")
            reply = (await asyncio.wait_for(r.read(), 5)).decode()
            self.assertTrue(reply.startswith("HTTP/1.1 200 OK\r\n"))
            self.assertTrue(reply.endswith("\r\n\r\nCHIBA-GRID-7 2 6\nSEOUL-NODE-2 0 6\n"))
            w.close()
        finally:
            srv.close()
            await srv.wait_closed()


if __name__ == "__main__":
    unittest.main()
