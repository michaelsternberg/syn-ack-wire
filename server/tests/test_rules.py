"""Rule edge cases, straight from the rulebook.  Run from server/:  python3 -m unittest -v"""
import unittest

from acquire.rules import (BLOCKED, OK, PATCHED, SPIKE, Decision, Game, GameOver, RuleError, cellval,
                           node_name)

LOW0, LOW1, MID2, MID3, MID4, HIGH5, HIGH6 = range(7)


def N(name):
    """'C4' -> node index."""
    return int(name[1:]) * 12 + "ABCDEFGHIJKL".index(name[0])


def make(n=3):
    return Game([f"P{i}" for i in range(n)], seed=1)


def _take_from_bag(g, nodes):
    for t in nodes:
        g.bag.remove(t)


def hive(g, syn, *names):
    nodes = [N(x) for x in names]
    _take_from_bag(g, nodes)
    for t in nodes:
        g.board[t] = cellval(syn)
    g.size[syn] += len(nodes)


def row_hive(g, syn, size, row):
    """A straight hive along `row`, starting at column A."""
    hive(g, syn, *[f"{c}{row}" for c in "ABCDEFGHIJKL"[:size]])


def spikes(g, *names):
    nodes = [N(x) for x in names]
    _take_from_bag(g, nodes)
    for t in nodes:
        g.board[t] = SPIKE


def give(g, seat, syn, k):
    g.coins[seat][syn] += k
    g.supply[syn] -= k


def my_turn(g, seat, *hand):
    nodes = [N(x) for x in hand]
    _take_from_bag(g, nodes)
    g.hands[seat] = nodes
    g.turn = seat
    g.pending = Decision("P", seat)


class Prices(unittest.TestCase):
    def test_chart(self):
        g = make()
        self.assertEqual(g.price(LOW0, 1), 0)
        self.assertEqual([g.price(LOW0, s) for s in (2, 3, 4, 5, 6, 10, 11, 20, 21, 31, 41, 60)],
                         [2, 3, 4, 5, 6, 6, 7, 7, 8, 9, 10, 10])
        self.assertEqual(g.price(MID2, 2), 3)
        self.assertEqual(g.price(MID2, 41), 11)
        self.assertEqual(g.price(HIGH5, 2), 4)
        self.assertEqual(g.price(HIGH6, 41), 12)


class Placing(unittest.TestCase):
    def test_lone_spike(self):
        g = make()
        hive(g, LOW0, "A0", "B0")
        my_turn(g, 0, "K8")
        g.apply(0, N("K8"))
        self.assertEqual(g.board[N("K8")], SPIKE)
        self.assertEqual(g.pending.kind, "B")

    def test_found_gives_finders_cut(self):
        g = make()
        spikes(g, "B6")
        my_turn(g, 0, "C6")
        g.apply(0, N("C6"))
        self.assertEqual(g.pending.kind, "F")
        self.assertEqual(g.pending.data["avail"], list(range(7)))
        g.apply(0, HIGH5)
        self.assertEqual(g.size[HIGH5], 2)
        self.assertEqual(g.coins[0][HIGH5], 1)
        self.assertEqual(g.supply[HIGH5], 24)
        self.assertEqual(g.board[N("B6")], cellval(HIGH5))

    def test_found_with_empty_supply_gives_nothing(self):
        g = make()
        give(g, 1, MID3, 25)
        spikes(g, "B6")
        my_turn(g, 0, "C6")
        g.apply(0, N("C6"))
        g.apply(0, MID3)
        self.assertEqual(g.coins[0][MID3], 0)
        self.assertEqual(g.size[MID3], 2)

    def test_found_absorbs_chain_of_spikes(self):
        g = make()
        spikes(g, "B6", "B7", "B8", "A8")
        my_turn(g, 0, "C6")
        g.apply(0, N("C6"))
        g.apply(0, LOW0)
        self.assertEqual(g.size[LOW0], 5)

    def test_cannot_broker_to_active_syndicate(self):
        g = make()
        hive(g, LOW0, "A0", "B0")
        spikes(g, "B6")
        my_turn(g, 0, "C6")
        g.apply(0, N("C6"))
        with self.assertRaises(RuleError):
            g.apply(0, LOW0)

    def test_eighth_hive_is_blocked(self):
        g = make()
        for s, col in enumerate("ACEGIK"):
            hive(g, s, f"{col}0", f"{col}1")
        hive(g, HIGH6, "A3", "A4")
        spikes(g, "K8")
        my_turn(g, 0, "L8", "L0")
        self.assertEqual(g.status(N("L8")), BLOCKED)
        self.assertEqual(g.status(N("L0")), OK)
        with self.assertRaises(RuleError):
            g.apply(0, N("L8"))
        self.assertEqual(g.board[N("L8")], 0)       # rejected move changed nothing

    def test_spread_picks_up_lone_spikes(self):
        g = make()
        hive(g, LOW1, "A0", "B0")
        spikes(g, "D0", "D1", "E1")
        my_turn(g, 0, "C0")
        g.apply(0, N("C0"))
        self.assertEqual(g.size[LOW1], 6)
        self.assertEqual(g.board[N("E1")], cellval(LOW1))

    def test_wrong_tile_or_wrong_seat(self):
        g = make()
        my_turn(g, 0, "C0")
        with self.assertRaises(RuleError):
            g.apply(0, N("D0"))
        with self.assertRaises(RuleError):
            g.apply(1, N("C0"))


class Hivejack(unittest.TestCase):
    def setUp(self):
        # Lotus-12 (HIGH5): D2 E2 F2 F3 = 4.  Iron Vector (MID3): H3 H4 I4 = 3.  G3 joins them.
        g = self.g = make(3)
        hive(g, HIGH5, "D2", "E2", "F2", "F3")
        hive(g, MID3, "H3", "H4", "I4")

    def test_bigger_absorbs_with_tied_root(self):
        g = self.g
        give(g, 0, MID3, 4)
        give(g, 1, MID3, 4)
        give(g, 2, MID3, 2)
        my_turn(g, 1, "G3")
        before = list(g.cash)
        g.apply(1, N("G3"))
        # Iron Vector 3 nodes, mid tier: price 4 -> ROOT 40, SUDO 20; tied ROOT shares 60 -> 30 each
        self.assertEqual([g.cash[p] - before[p] for p in range(3)], [30, 30, 0])
        # disposal starts with the player who caused the hivejack, then around the table
        self.assertEqual((g.pending.kind, g.pending.seat), ("D", 1))
        g.apply(1, (0, 4))             # swap 4 -> 2 LTS
        self.assertEqual(g.pending.seat, 2)
        g.apply(2, (2, 0))             # cash out 2 at 4 each
        self.assertEqual(g.cash[2] - before[2], 8)
        self.assertEqual(g.pending.seat, 0)
        g.apply(0, (0, 0))             # HODL 4
        self.assertEqual(g.pending.kind, "B")
        self.assertEqual(g.coins[1][HIGH5], 2)
        self.assertEqual(g.coins[0][MID3], 4)
        self.assertEqual(g.size[MID3], 0)
        self.assertEqual(g.size[HIGH5], 8)             # 4 + 3 + the new spike
        self.assertNotIn(cellval(MID3), g.board)

    def test_tie_lets_player_choose_survivor(self):
        g = self.g
        hive(g, MID3, "I5")                            # Iron Vector now 4, same as Lotus-12
        my_turn(g, 0, "G3")
        g.apply(0, N("G3"))
        self.assertEqual(g.pending.kind, "K")
        self.assertEqual(g.pending.data["ctx"], 0)
        self.assertEqual(sorted(g.pending.data["options"]), [MID3, HIGH5])
        g.apply(0, MID3)
        self.assertEqual(g.size[HIGH5], 0)
        self.assertEqual(g.size[MID3], 9)

    def test_sole_holder_gets_both_bonuses(self):
        g = self.g
        give(g, 2, MID3, 1)
        my_turn(g, 0, "G3")
        g.apply(0, N("G3"))
        self.assertEqual(g.cash[2], 60 + 60)

    def test_disposal_validation(self):
        g = self.g
        give(g, 0, MID3, 5)
        give(g, 1, HIGH5, 24)                          # only 1 LTS left in the Darkpool
        my_turn(g, 0, "G3")
        g.apply(0, N("G3"))
        with self.assertRaises(RuleError):
            g.apply(0, (0, 3))                         # odd swap
        with self.assertRaises(RuleError):
            g.apply(0, (0, 4))                         # needs 2 LTS, only 1 left
        with self.assertRaises(RuleError):
            g.apply(0, (4, 2))                         # more than held
        g.apply(0, (3, 2))
        self.assertEqual(g.coins[0][HIGH5], 1)


class Payouts(unittest.TestCase):
    def _merge_small(self, holdings):
        """Bratva (MID4, 2 nodes A0 B0) is absorbed by Ghost (LOW0, 3 nodes D0 E0 F0) via C0."""
        g = make(len(holdings))
        hive(g, MID4, "A0", "B0")
        hive(g, LOW0, "D0", "E0", "F0")
        for seat, k in enumerate(holdings):
            if k:
                give(g, seat, MID4, k)
        before = list(g.cash)
        my_turn(g, 0, "C0")
        g.apply(0, N("C0"))
        return [g.cash[p] - before[p] for p in range(g.n)]

    # mid tier, 2 nodes: price 3 -> ROOT 30, SUDO 15
    def test_tied_root_rounds_up(self):
        self.assertEqual(self._merge_small([3, 3, 1]), [23, 23, 0])     # 45/2 = 22.5 -> 23

    def test_tied_sudo_rounds_up(self):
        self.assertEqual(self._merge_small([5, 2, 2]), [30, 8, 8])      # 15/2 = 7.5 -> 8

    def test_root_and_sudo(self):
        self.assertEqual(self._merge_small([1, 4, 2]), [0, 30, 15])

    def test_three_way_tie_for_root(self):
        self.assertEqual(self._merge_small([2, 2, 2]), [15, 15, 15])

    def test_nobody_holds(self):
        self.assertEqual(self._merge_small([0, 0, 0]), [0, 0, 0])


class MultiWay(unittest.TestCase):
    def test_largest_absorbs_in_order_with_tie_choice(self):
        g = make(2)
        hive(g, LOW0, "A1", "B1", "C1", "A2", "A3")    # 5, west of D1
        hive(g, MID2, "D0", "E0")                       # 2, north
        hive(g, LOW1, "D2", "D3")                       # 2, south
        my_turn(g, 0, "D1")
        g.apply(0, N("D1"))
        # LOW0 is largest; MID2 and LOW1 tie at 2, so the player picks which is absorbed first
        self.assertEqual(g.pending.kind, "K")
        self.assertEqual(g.pending.data["ctx"], 1)
        self.assertEqual(sorted(g.pending.data["options"]), [LOW1, MID2])
        g.apply(0, MID2)
        self.assertEqual(g.pending.kind, "B")           # LOW1 then resolved automatically
        self.assertEqual(g.size[MID2], 0)
        self.assertEqual(g.size[LOW1], 0)
        self.assertEqual(g.size[LOW0], 5 + 2 + 2 + 1)


class BlackIce(unittest.TestCase):
    def test_two_safe_hives_patch_the_exploit(self):
        g = make()
        row_hive(g, LOW0, 11, 0)
        row_hive(g, LOW1, 11, 2)
        self.assertEqual(g.status(N("A1")), PATCHED)
        my_turn(g, 0, "A1")
        with self.assertRaises(RuleError):
            g.apply(0, N("A1"))

    def test_safe_absorbs_smaller(self):
        g = make()
        row_hive(g, LOW0, 11, 0)
        hive(g, MID2, "A2", "B2")
        my_turn(g, 0, "A1")
        self.assertEqual(g.status(N("A1")), OK)
        g.apply(0, N("A1"))
        self.assertEqual(g.size[LOW0], 14)
        self.assertEqual(g.size[MID2], 0)

    def test_burn_kit_when_nothing_playable(self):
        g = make()
        row_hive(g, LOW0, 11, 0)
        row_hive(g, LOW1, 11, 2)
        dead = [N(f"{c}1") for c in "ABCDEF"]
        _take_from_bag(g, dead)
        g.hands[0] = list(dead)
        g.turn = 0
        g._begin_turn()
        self.assertEqual(g.pending.kind, "P")
        self.assertEqual(len(g.hands[0]), 6)
        self.assertFalse(set(dead) & set(g.hands[0]))
        self.assertEqual(sorted(g.dead), sorted(dead))


class BuyingAndEnd(unittest.TestCase):
    def _to_buy(self, g):
        my_turn(g, 0, "L4")
        g.apply(0, N("L4"))
        self.assertEqual(g.pending.kind, "B")

    def test_buy_limits(self):
        g = make()
        hive(g, LOW0, "A0", "B0")       # price 2
        hive(g, HIGH6, "A8", "B8")      # price 4
        g.cash[0] = 9
        self._to_buy(g)
        with self.assertRaises(RuleError):
            g.apply(0, ([LOW0] * 4, False))                # more than 3
        with self.assertRaises(RuleError):
            g.apply(0, ([MID3], False))                    # not on the Grid
        with self.assertRaises(RuleError):
            g.apply(0, ([], True))                         # can't end yet
        with self.assertRaises(RuleError):
            g.apply(0, ([HIGH6, LOW0, HIGH6], False))      # 10 > 9
        self.assertEqual((g.cash[0], g.coins[0][HIGH6]), (9, 0))   # rejected moves changed nothing
        g.apply(0, ([HIGH6, LOW0], False))
        self.assertEqual(g.cash[0], 3)

    def test_buy_then_draw_and_pass(self):
        g = make()
        hive(g, HIGH6, "A8", "B8")
        self._to_buy(g)
        g.apply(0, ([HIGH6, HIGH6], False))
        self.assertEqual(g.cash[0], 60 - 8)
        self.assertEqual(g.coins[0][HIGH6], 2)
        self.assertEqual(g.supply[HIGH6], 23)
        self.assertEqual(len(g.hands[0]), 6)
        self.assertEqual(g.turn, 1)

    def test_nothing_to_buy_skips(self):
        g = make()
        hive(g, HIGH6, "A8", "B8")      # price 4
        g.cash[0] = 3
        my_turn(g, 0, "L4")
        g.apply(0, N("L4"))
        self.assertEqual(g.turn, 1)     # no buy decision: straight on
        self.assertEqual(len(g.hands[0]), 6)
        give(g, 2, HIGH6, 25)           # all sold, however rich
        g.cash[1] = 60
        my_turn(g, 1, "F2")
        g.apply(1, N("F2"))
        self.assertNotEqual(g.turn, 1)  # (seat 2 has no kit here, so it may pass on too)

    def test_nothing_to_buy_still_asks_to_end(self):
        g = make()
        row_hive(g, HIGH6, 11, 0)       # Black ICE: the game can end
        g.cash[0] = 0
        self._to_buy(g)
        self.assertTrue(g.pending.data["can_end"])
        self.assertFalse(g.pending.data["can_buy"])

    def test_out_of_supply(self):
        g = make()
        hive(g, HIGH6, "A8", "B8")
        give(g, 1, HIGH6, 24)
        self._to_buy(g)
        with self.assertRaises(RuleError):
            g.apply(0, ([HIGH6, HIGH6], False))

    def test_lockdown_end_and_final_scoring(self):
        g = make(2)
        row_hive(g, LOW0, 11, 0)        # safe, price 7
        give(g, 0, LOW0, 3)
        give(g, 1, LOW0, 1)
        give(g, 1, MID3, 5)             # flatlined: worthless
        self._to_buy(g)
        self.assertTrue(g.pending.data["can_end"])
        events = g.apply(0, ([], True))
        self.assertTrue(g.over)
        self.assertIsInstance(events[-1], GameOver)
        # seat 0: ROOT 70 + 3*7 ; seat 1: SUDO 35 + 1*7
        self.assertEqual(g.cash, [60 + 70 + 21, 60 + 35 + 7])
        self.assertEqual(g.winners, (0,))
        self.assertEqual(g.coins[1][MID3], 0)

    def test_total_compromise(self):
        g = make(2)
        for row in (0, 2, 4, 6):
            row_hive(g, LOW0, 11, row)
        hive(g, LOW0, "A1", "A3", "A5")                 # one hive of 47
        self.assertTrue(g.can_call_end())
        hive(g, MID2, "L8", "K8")                       # a small unsafe hive doesn't matter
        self.assertTrue(g.can_call_end())


class Setup(unittest.TestCase):
    def test_opening_lowest_node_goes_first(self):
        g = make(4)
        opening = list(reversed(g.bag))[:4]             # start() pops from the end
        g.start()
        self.assertEqual(g.turn, min(range(4), key=lambda s: opening[s]))
        self.assertTrue(all(len(h) == 6 for h in g.hands))
        self.assertEqual(sum(g.size), 0)

    def test_node_names(self):
        self.assertEqual(node_name(N("C4")), "C4")
        self.assertEqual(N("C4"), 50)
        self.assertEqual(N("L8"), 107)


if __name__ == "__main__":
    unittest.main()
