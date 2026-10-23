#!/usr/bin/env python3
"""A server that stages a game for the manual's captures.

    python3 stage.py SCENARIO [--file ../docs/scenarios.toml] [--port 6599] [--region NAME]
    python3 stage.py --list

It is serve.py's server, except that the game the host starts is the scenario's: its seats, board,
holdings and cash, with seat 0 (the client, whatever its handle) on turn and its move already made.
The client then opens on the prompt that move leads to, drawn from real frames, as in any game.
client/tools/capture.sh does the rest for the Atari: it points the autoplay client at it and keeps the dump taken at
the scenario's `capture` prompt. Start the game with as many seats as the scenario has.
Scenarios are described in docs/scenarios.toml.
"""
import argparse
import asyncio
import logging
import tomllib
from pathlib import Path

import serve
from acquire import rules
from acquire import table as table_mod
from acquire.proto import BOT, HUMAN
from sim import check

SCENARIOS = Path(__file__).resolve().parent.parent / "docs" / "scenarios.toml"


def node(name):
    col, row = "ABCDEFGHIJKL".index(name[0]), int(name[1:])
    if not 0 <= row < rules.ROWS:
        raise ValueError(f"no node {name}")
    return row * rules.COLS + col


class StagedGame(rules.Game):
    def __init__(self, sc, theme):
        super().__init__(sc["seats"], seed=1, theme=theme)
        letters = {s.letter: s.id for s in self.theme.syndicates}
        for n in map(node, sc.get("spikes", ())):
            self.board[n] = rules.SPIKE
        for letter, nodes in sc.get("hives", {}).items():
            s = letters[letter]
            for n in map(node, nodes):
                self.board[n] = rules.cellval(s)
            self.size[s] = len(nodes)
        self.cash = [c // 100 for c in sc["cash"]]
        for p, name in enumerate(self.names):
            for letter, k in sc.get("coins", {}).get(name, {}).items():
                s = letters[letter]
                self.coins[p][s] += k
                self.supply[s] -= k
        self.play = node(sc["play"]) if sc.get("play") else None
        self.end = sc.get("end", False)
        kit = [node(n) for n in sc.get("kit", ())]
        if self.play is not None and self.play not in kit:
            kit.insert(0, self.play)
        self.bag = [n for n in self.bag if not self.board[n] and n not in kit]
        self.hands[0] = kit
        self._draw(0, self.kitsize - len(kit))
        for p in range(1, self.n):
            self._draw(p, self.kitsize)
        self.kit = tuple((t, self.status(t)) for t in self.hands[0])   # the kit as the turn opens
        check(self)                         # sim.py's invariants: a position the rules could reach

    def start(self):
        self.turn = 0
        if self.play is None:
            self._begin_turn()              # the turn opens: run which exploit?
        else:
            self._act_play(self.play)
            if self.end and self.pending and self.pending.kind == "B":
                self._act_buy(([], True))   # buy nothing, call the null-route: the game ends
        events = self._flush()
        self.moves = [e for e in events if not isinstance(e, (rules.Log, rules.Hand))]
        return events


class StagedTable(table_mod.Table):
    scenario = None

    def _start(self, seat, count):
        sc = self.scenario
        if count != len(sc["seats"]):
            self._send(seat, table_mod.proto.log_line(f"STAGE: START WITH {len(sc['seats'])} SEATS"))
            self._send(seat, self._lobby_prompt())
            return
        real = table_mod.rules

        class Rules:                        # the table's view of the rules: the same, but the game is staged
            def __getattr__(_, k):
                return getattr(real, k)

            @staticmethod
            def Game(names, seed=None, theme=None):
                for s, name in zip(self.seats, sc["seats"]):
                    s.name = name
                return StagedGame(sc, theme)

        table_mod.rules = Rules()
        try:
            super()._start(seat, count)
        finally:
            table_mod.rules = real
        g = self.game
        if g is not None and g.play is not None:
            # As a real move arrives: the client still holds the kit it played from (its next one comes at the
            # end of the turn), then the move's board events in order: the spike, which it marks LAST, and
            # whatever that spike did. They end where the snapshot already is.
            self._send(self.seats[0], table_mod.proto.hand(g.kit))
            data = b"".join(table_mod.proto.event(e) for e in g.moves)
            for s in self.seats:
                self._send(s, data)

    def _shown_kind(self, seat):
        """Every staged seat shows as a netrunner at the keyboard: the examples are about people."""
        kind = super()._shown_kind(seat)
        return HUMAN if kind == BOT else kind


class StagedServer(serve.Server):
    def table(self, tid):
        if tid not in self.tables:
            self.tables[tid] = StagedTable(tid, self.theme, self.timeout, self.bot_delay, self.seed, self.think,
                                           self.abandon, self.afk, self.host_wait, self.jack_in)
        return self.tables[tid]


async def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("scenario", nargs="?")
    ap.add_argument("--file", type=Path, default=SCENARIOS)
    ap.add_argument("--list", action="store_true", help="list the scenarios")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=6599)
    ap.add_argument("--region", type=serve.region_name)
    a = ap.parse_args()
    with a.file.open("rb") as f:
        scenarios = tomllib.load(f)
    if a.list or not a.scenario:
        for name, sc in scenarios.items():
            print(f"{name:16} {sc.get('about', '')}")
        return
    sc = scenarios[a.scenario]
    while "like" in sc:                     # a scenario may start from another's and change some of it
        base = dict(scenarios[sc["like"]])
        base.pop("about", None)
        sc = base | {k: v for k, v in sc.items() if k != "like"}
    StagedTable.scenario = sc
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")
    server = StagedServer(region=a.region, host_wait=0)
    srv = await server.serve(a.host, a.port)
    async with srv:
        await srv.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
