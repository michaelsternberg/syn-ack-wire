"""Bots that answer engine Decisions. They use only public state plus their own kit.

    action = bot.decide(game, decision)
"""
import random

from .rules import NSYN, OK


class RandomBot:
    """Any legal move. Useful for soak testing."""

    def __init__(self, seed=None):
        self.rng = random.Random(seed)

    def decide(self, g, d):
        r = self.rng
        if d.kind == "P":
            return r.choice([t for t in g.hands[d.seat] if g.status(t) == OK])
        if d.kind == "F":
            return r.choice(d.data["avail"])
        if d.kind == "K":
            return r.choice(d.data["options"])
        if d.kind == "D":
            held = d.data["held"]
            swap = 2 * r.randint(0, min(held // 2, d.data["supply"]))
            sell = r.randint(0, held - swap)
            return sell, swap
        if d.kind == "B":
            return _random_buys(r, g, d, r.randint(0, g.maxbuy)), d.data["can_end"] and r.random() < 0.5
        raise ValueError(d.kind)


class HeuristicBot:
    """Plays like a sensible beginner: chases ROOT/SUDO positions, founds expensive hives, swaps when it pays."""

    def __init__(self, seed=None):
        self.rng = random.Random(seed)

    def decide(self, g, d):
        return getattr(self, "_" + d.kind)(g, d)

    # --- run an exploit
    def _P(self, g, d):
        me = d.seat
        best, best_score = None, None
        for t in g.hands[me]:
            if g.status(t) != OK:
                continue
            syns, spikes = g.adjacent(t)
            if not syns:
                score = 6.0 if spikes else 0.0
            elif len(syns) == 1:
                score = 1.0 + 3.0 * self._share(g, me, syns[0])
            else:
                top = max(g.size[s] for s in syns)
                score = 0.0
                for s in syns:
                    if g.size[s] == top:
                        score += 2.0 * self._share(g, me, s)
                    else:
                        rank = self._rank(g, me, s)
                        score += {1: 8.0, 2: 4.0}.get(rank, -2.0)
            score += self.rng.random() * 0.5
            if best_score is None or score > best_score:
                best, best_score = t, score
        return best

    # --- broker a new hive: the most expensive syndicate available
    def _F(self, g, d):
        return max(d.data["avail"], key=lambda s: (g.tier[s], self.rng.random()))

    # --- tie-break: keep (or absorb first) the syndicate we hold most of
    def _K(self, g, d):
        me = d.seat
        return max(d.data["options"], key=lambda s: (g.coins[me][s], self.rng.random()))

    # --- dispose of absorbed coin
    def _D(self, g, d):
        held, absorbed, absorber = d.data["held"], d.data["absorbed"], d.data["absorber"]
        merged_size = g.size[absorber] + g.size[absorbed] + 1
        if g.price(absorber, merged_size) > 2 * d.data["price"]:
            swap = 2 * min(held // 2, d.data["supply"])
        else:
            swap = 0
        return held - swap, swap

    # --- buy coin, maybe end the game
    def _B(self, g, d):
        me = d.seat
        cash, supply = d.data["cash"], list(d.data["supply"])
        mine = list(g.coins[me])
        buys = []
        for _ in range(g.maxbuy):
            best, best_score = None, 0.0
            for s in range(NSYN):
                price = d.data["prices"][s]
                if not price or not supply[s] or price > cash:
                    continue
                others = max((g.coins[p][s] for p in range(g.n) if p != me), default=0)
                if mine[s] + 1 > others:
                    score = 3.0
                elif mine[s] + 3 >= others:
                    score = 2.0
                else:
                    score = 0.5
                if g.size[s] >= g.safesize:
                    score *= 0.4          # safe hives never pay a merger bonus
                score = score * 6.0 / price + self.rng.random() * 0.3
                if score > best_score:
                    best, best_score = s, score
            if best is None or best_score < 0.6:
                break
            buys.append(best)
            cash -= d.data["prices"][best]
            supply[best] -= 1
            mine[best] += 1
        call_end = d.data["can_end"] and g.net_worth(me) >= max(g.net_worth(p) for p in range(g.n))
        return buys, call_end

    # --- helpers
    @staticmethod
    def _share(g, me, s):
        total = sum(g.coins[p][s] for p in range(g.n))
        return g.coins[me][s] / total if total else 0.0

    @staticmethod
    def _rank(g, me, s):
        mine = g.coins[me][s]
        if not mine:
            return None
        return 1 + sum(1 for p in range(g.n) if g.coins[p][s] > mine)


def _random_buys(r, g, d, k):
    cash, supply, buys = d.data["cash"], list(d.data["supply"]), []
    for _ in range(k):
        options = [s for s in range(NSYN) if d.data["prices"][s] and supply[s] and d.data["prices"][s] <= cash]
        if not options:
            break
        s = r.choice(options)
        buys.append(s)
        cash -= d.data["prices"][s]
        supply[s] -= 1
    return buys
