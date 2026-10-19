#!/usr/bin/env python3
"""Headless bot-vs-bot soak test with invariant checks after every action.

    python3 sim.py [games] [--seed N] [--show]
"""
import argparse
import sys
from collections import Counter

from acquire.bots import HeuristicBot, RandomBot
from acquire.rules import NODES, NSYN, SPIKE, Game, Log, RuleError

MAX_ACTIONS = 20000


def check(g):
    coinsupply = g.theme.rules["coinsupply"]
    for s in range(NSYN):
        held = sum(g.coins[p][s] for p in range(g.n))
        assert held + g.supply[s] == coinsupply, f"coin {s} not conserved"
        assert g.supply[s] >= 0
        assert g.size[s] == g.board.count(s + 2), f"size {s} mismatch"
        assert g.size[s] == 0 or g.size[s] >= 2, f"hive {s} of size 1"
    assert all(c >= 0 for c in g.cash), "negative cash"
    tiles = sum(1 for v in g.board if v) + sum(len(h) for h in g.hands) + len(g.bag) + len(g.dead)
    assert tiles == NODES, f"tile count {tiles}"
    assert all(len(h) <= g.kitsize for h in g.hands)
    # no two different hives may touch
    for n, v in enumerate(g.board):
        if v >= 2:
            for a in (n + 1 if n % 12 < 11 else None, n + 12 if n < NODES - 12 else None):
                if a is not None and g.board[a] >= 2:
                    assert g.board[a] == v, f"hives touch at {n}/{a}"
    # a lone spike can't touch a hive (it would have joined)
    merging = g.mg["node"] if g.mg else None     # mid-hivejack, the new spike touches hives
    for n, v in enumerate(g.board):
        if v == SPIKE and n != merging:
            syns, _ = g.adjacent(n)
            assert not syns, f"lone spike {n} touches a hive"


def play(seed, show=False):
    n = 2 + seed % 5
    names = [f"BOT{i}" for i in range(n)]
    g = Game(names, seed=seed, replace_patched=bool(seed % 2))
    bots = [HeuristicBot(seed * 10 + i) if i % 2 == 0 else RandomBot(seed * 10 + i) for i in range(n)]
    long_lines = Counter()
    events = g.start()
    for step in range(MAX_ACTIONS):
        for e in events:
            if isinstance(e, Log):
                if len(e.text) > 38:
                    long_lines[e.text] += 1
                if show:
                    print(e.text)
        check(g)
        if g.over:
            return g, step, long_lines
        d = g.pending
        action = bots[d.seat].decide(g, d)
        try:
            events = g.apply(d.seat, action)
        except RuleError as ex:
            raise AssertionError(f"seed {seed}: bot made illegal move {d.kind} {action}: {ex}")
    raise AssertionError(f"seed {seed}: game did not finish in {MAX_ACTIONS} actions")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("games", type=int, nargs="?", default=500)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--show", action="store_true", help="print the log of each game")
    a = ap.parse_args()

    wins, steps, longest = Counter(), [], Counter()
    for seed in range(a.seed, a.seed + a.games):
        g, step, long_lines = play(seed, a.show)
        steps.append(step)
        longest.update(long_lines)
        for p in g.winners:
            wins["heuristic" if p % 2 == 0 else "random"] += 1 / len(g.winners)
    print(f"{a.games} games OK. actions/game: min {min(steps)} avg {sum(steps) / len(steps):.0f} max {max(steps)}")
    print("wins:", {k: round(v) for k, v in wins.items()})
    if longest:
        print(f"{len(longest)} distinct log lines over 38 chars, e.g.:")
        for text, _ in longest.most_common(5):
            print(f"  ({len(text)}) {text}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
