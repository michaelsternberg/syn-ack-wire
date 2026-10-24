"""Listing this server in the FujiNet game lobby (github.com/FujiNetWIFI/fujinet-lobby).

The lobby is a registry. A game server POSTs its entry to /server; the lobby program on an 8-bit machine lists the
entries for its platform. When a player picks one, the lobby program stores the entry's address in an AppKey
(creator 1, app 1, key = the entry's `appkey`), then boots the client disk from the entry's client URL; the client
reads the key and dials that address (client/README.md). One entry serves every platform: it lists a client
disk for each (`clients`), and each platform's lobby program shows the entries that have one for it.

The lobby never ages an entry out: it shows how long ago the server last posted. So we post whenever the player
count changes (at most once a second) and every few minutes regardless, and DELETE the entry on shutdown.
"""
import asyncio
import json
import logging
import urllib.error
import urllib.request

from . import proto

KEEPALIVE = 300.0   # seconds between posts when nothing changes
log = logging.getLogger("synack")


def entry(game, server, region, address, clients, key):
    """The lobby's GameServer record, online with nobody here yet. `clients`: platform -> client disk URL,
    e.g. {"atari": "TNFS://host/syn-ack-wire-atari.atr", "apple2": "TNFS://host/syn-ack-wire-apple2.po"}."""
    return {"game": game, "appkey": key, "server": server, "region": region, "serverurl": address,
            "status": "online", "maxplayers": proto.SEATS, "curplayers": 0,
            "clients": [{"platform": p, "url": u} for p, u in clients.items()]}


class Lobby:
    """Keeps one entry up to date in the lobby at `url`. Create it inside the running event loop."""

    def __init__(self, url, entry, keepalive=KEEPALIVE):
        self.url = url.rstrip("/") + "/server"
        self.entry = entry
        self.keepalive = keepalive
        self.sent = None            # the entry as the lobby last accepted it
        self.listed = None          # the status the log last reported
        self.dirty = asyncio.Event()
        self.task = asyncio.create_task(self._run())

    def players(self, here, seats):
        """The PLAYERS column: `here` of `seats`."""
        self.entry["curplayers"], self.entry["maxplayers"] = here, seats
        self.dirty.set()

    async def close(self):
        """Take the entry off the lobby's list."""
        self.task.cancel()
        if await self._request("DELETE", {"serverurl": self.entry["serverurl"]}):
            log.info("lobby: removed %s from %s", self.entry["serverurl"], self.url)

    async def _run(self):
        self.dirty.set()
        while True:
            try:
                await asyncio.wait_for(self.dirty.wait(), self.keepalive)
            except TimeoutError:
                self.sent = None    # keepalive: post it unchanged
            self.dirty.clear()
            if self.entry != self.sent:
                await self._post(dict(self.entry))
            await asyncio.sleep(1.0)

    async def _post(self, entry):
        if not await self._request("POST", entry):
            return                              # try again at the next change or keepalive
        if entry["status"] != self.listed:
            self.listed = entry["status"]
            log.info("lobby: listed %s as %s at %s", entry["serverurl"], entry["status"], self.url)
        self.sent = entry

    async def _request(self, method, body):
        """Send `body` as JSON; True if the lobby took it."""
        req = urllib.request.Request(self.url, data=json.dumps(body).encode(), method=method,
                                     headers={"Content-Type": "application/json"})
        try:
            await asyncio.to_thread(self._send, req)
        except urllib.error.HTTPError as e:     # the lobby's reasons come back as JSON
            log.warning("lobby refused %s: %s %s", method, e.code, e.read().decode(errors="replace").strip())
            return False
        except OSError as e:
            log.warning("lobby unreachable: %s", e)
            return False
        return True

    @staticmethod
    def _send(req):
        with urllib.request.urlopen(req, timeout=10):
            pass
