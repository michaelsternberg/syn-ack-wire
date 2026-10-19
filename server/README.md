# SYN-ACK WIRE server

Python 3.11+, standard library only.

| Path | What |
|---|---|
| `acquire/theme.py` | Loads `../theme/theme.toml`: syndicates, rule numbers, log wording |
| `acquire/rules.py` | Rules engine. Pure: actions in, events out. No I/O, no clocks |
| `acquire/bots.py` | `RandomBot` (any legal move) and `HeuristicBot` (plays sensibly) |
| `acquire/proto.py` | Wire protocol v2 (`docs/protocol.md`): frame encode/decode |
| `acquire/table.py` | A table: seats, tokens, lobby, shot clock, bot takeover, game loop |
| `serve.py` | The asyncio TCP server |
| `testclient.py` | Protocol client for a modern PC: interactive, or `--auto` random play from wire data only |
| `tests/test_rules.py` | Rulebook edge cases |
| `tests/test_server.py` | End-to-end over TCP: full game, timeouts, drop/reconnect, faults |
| `sim.py` | Bot-vs-bot soak test with invariant checks after every action |

```sh
python3 serve.py                              # port 6502; --timeout 60 --bot-delay 1.0 --keepalive 10
python3 serve.py --trace                      # the log shows the game as players see it; --trace adds every frame clients send
python3 serve.py --think-exploit 20,10 --think-buy 12,5   # bots take 10-30 s to run an exploit, 7-17 s to buy
python3 testclient.py --name ALICE            # play by hand; first to join is host and picks seat count
python3 testclient.py --name BOB --auto       # a second human, playing randomly

python3 -m unittest -v        # all tests
python3 sim.py 2000           # soak: 2000 games, 2-6 seats, mixed bots
python3 sim.py 1 --seed 7 --show   # print one game's log
```

## Engine in one paragraph
`Game(names, seed)` holds the whole state. `start()` returns the first events. After that, `game.pending` is a
`Decision(kind, seat, data)` and `game.apply(seat, action)` answers it and returns events. Decision kinds use the
wire-protocol prompt letters (`P F K D B`), and events (`Cells`, `Recolor`, `Size`, `Cash`, `Coins`, `Hand`,
`Turn`, `Log`, `GameOver`) map one-to-one onto server-to-client messages. An illegal action raises `RuleError` and
leaves the state untouched. Money is in units of 100, like the protocol.

## Rule choices beyond the rulebook
- A kit with no runnable exploit is burned and redrawn automatically.
- `replace_patched=True` swaps out patched exploits at the end of each turn (house rule; off by default).
- If no one can run an exploit for a full round, the game ends (Grid Stall; now in the rulebook too).

## Seats and tables
- The first frame must be `H`. Its table byte picks the table (created on demand). The first human is host and gets
  prompt `S`; `S n` starts with `n` seats, and bots fill the empty ones.
- Once a game starts, new players get fault 2 (table full). A token reclaims a seat at any time.
- A prompt that times out is answered by that seat's bot; the seat stays human. A dropped connection marks the seat
  away and its bot plays until the token comes back. `Q` hands the seat to the bot for good.
- Bots, and away seats, wait `--bot-delay` before each decision. `--think-exploit` and `--think-buy` give them a
  thinking time instead before running an exploit and before buying: `MEAN,JITTER` in seconds, drawn from a
  triangular distribution (never outside MEAN±JITTER, most often near MEAN). A prompt that timed out is answered
  at once.
- After a game the table returns to the lobby. Bot and away seats stay visible until a new player needs the chair,
  and a reconnecting player is sent the result (`e`) of the last game.
