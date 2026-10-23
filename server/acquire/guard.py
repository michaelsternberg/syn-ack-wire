"""Keeping a public server usable: limits on connections, and a short block for addresses that keep misbehaving.

A port open to the internet is soon found by scanners, which connect and say nothing, or say HTTP. Each such
misbehaviour is a strike against the address; a few strikes in a few minutes block it for a while, and its new
connections are closed at once, without a word. A real client never earns a strike in normal play: the first
frame it sends is `I` or `H`, at once, for a table this server has.
"""
import logging
import time

log = logging.getLogger("synack")

STRIKES = 3             # misbehaviours from one address...
STRIKE_WINDOW = 300.0   # ...within this many seconds...
BLOCK = 900.0           # ...block it this long
PER_ADDRESS = 12        # connections at once from one address: a LAN party behind one router
TOTAL = 200             # connections at once in all
PRUNE_AT = 1000         # tidy the books when they hold this many addresses


class Guard:
    def __init__(self, strikes=STRIKES, window=STRIKE_WINDOW, block=BLOCK, per_address=PER_ADDRESS, total=TOTAL,
                 clock=time.monotonic):
        self.limit, self.window, self.block = strikes, window, block
        self.per_address, self.total = per_address, total
        self.clock = clock
        self.strikes = {}       # address -> times of its recent strikes
        self.blocked = {}       # address -> when its block ends
        self.conns = {}         # address -> connections open now
        self.open = 0

    def admit(self, addr):
        """A new connection from `addr`: True to serve it (then release() it when it closes), False to close it."""
        now = self.clock()
        until = self.blocked.get(addr)
        if until is not None:
            if now < until:
                return False
            del self.blocked[addr]
        if self.open >= self.total or self.conns.get(addr, 0) >= self.per_address:
            return False
        self.conns[addr] = self.conns.get(addr, 0) + 1
        self.open += 1
        return True

    def release(self, addr):
        n = self.conns.get(addr, 0) - 1
        if n > 0:
            self.conns[addr] = n
        else:
            self.conns.pop(addr, None)
        self.open -= 1

    def strike(self, addr, why):
        """`addr` misbehaved: count it, and block the address once it has STRIKES within STRIKE_WINDOW."""
        now = self.clock()
        times = [t for t in self.strikes.get(addr, ()) if now - t < self.window] + [now]
        if len(times) >= self.limit:
            self.strikes.pop(addr, None)
            self.blocked[addr] = now + self.block
            log.warning("%s blocked for %g s: %s", addr, self.block, why)
        else:
            self.strikes[addr] = times
            log.info("%s: %s", addr, why)
        if len(self.strikes) + len(self.blocked) > PRUNE_AT:
            self._prune(now)

    def _prune(self, now):
        self.strikes = {a: ts for a, ts in self.strikes.items() if now - ts[-1] < self.window}
        self.blocked = {a: until for a, until in self.blocked.items() if now < until}
