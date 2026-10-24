"""Short links for the QR codes on the game disks: GET /ATARI redirects to the Atari manual, /APPLE2 to an Apple II
one, and so on, by the platform's folder name (docs/manual/<platform>/); / redirects to the project's home page.

The links are short so the QR codes stay small, and upper case, which QR codes store more compactly; the case is
ignored. Nothing is served from here but redirects, so a manual changes where it lives (a push to GitHub), not on a
redeploy. On fly.io the proxy's http handler terminates TLS and passes plain HTTP/1.1, one request a connection
is all this answers.
"""
import asyncio
import logging
import re

log = logging.getLogger("synack")

TIMEOUT = 10.0          # seconds a request has to arrive
LIMIT = 4096            # bytes its head may take
PLATFORM = re.compile(r"/([A-Za-z0-9]{1,16})/?")
REASONS = {302: "Found", 400: "Bad Request", 404: "Not Found", 405: "Method Not Allowed"}


class Web:
    def __init__(self, manual, home=None):
        self.manual = manual    # a URL with {platform} in it
        self.home = home

    async def serve(self, host, port):
        srv = await asyncio.start_server(self._handle, host, port, limit=LIMIT)
        log.info("links listening on %s", srv.sockets[0].getsockname()[:2])
        return srv

    def answer(self, line):
        """A request line -> (status, Location or None)."""
        parts = line.split(" ")
        if len(parts) != 3 or not parts[2].startswith("HTTP/"):
            return 400, None
        method, target = parts[0], parts[1].split("?", 1)[0]
        if method not in ("GET", "HEAD"):
            return 405, None
        if target == "/":
            return (302, self.home) if self.home else (404, None)
        m = PLATFORM.fullmatch(target)
        if m:
            return 302, self.manual.format(platform=m.group(1).lower())
        return 404, None

    async def _handle(self, reader, writer):
        try:
            head = await asyncio.wait_for(reader.readuntil(b"\r\n\r\n"), TIMEOUT)
            lines = head.decode("latin-1").split("\r\n")
            status, location = self.answer(lines[0])
            who = next((h.split(":", 1)[1].strip() for h in lines if h.lower().startswith("fly-client-ip:")),
                       writer.get_extra_info("peername", ("?",))[0])
            if status == 302:
                log.info("link: %s %s -> %s", who, lines[0].split(" ")[1], location)
            body = b"" if status == 302 else f"{status} {REASONS[status]}\n".encode()
            writer.write((f"HTTP/1.1 {status} {REASONS[status]}\r\n"
                          + (f"Location: {location}\r\n" if location else "")
                          + "Content-Type: text/plain\r\n"
                          f"Content-Length: {len(body)}\r\nConnection: close\r\n\r\n").encode()
                         + (b"" if lines[0].startswith("HEAD ") else body))
            await writer.drain()
        except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, TimeoutError, ConnectionError, ValueError):
            pass                                        # half a request, too long, too slow, or gone: just close
        finally:
            writer.close()
