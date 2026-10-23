# SYN-ACK WIRE server

Python 3.11+, standard library only.

| Path | What |
|---|---|
| `acquire/theme.py` | Loads `../theme/theme.toml`: syndicates, rule numbers, log wording |
| `acquire/rules.py` | Rules engine. Pure: actions in, events out. No I/O, no clocks |
| `acquire/bots.py` | `RandomBot` (any legal move) and `HeuristicBot` (plays sensibly) |
| `acquire/proto.py` | Wire protocol v3 (`docs/protocol.md`): frame encode/decode |
| `acquire/table.py` | A table: seats, tokens, lobby, shot clock, bot takeover, game loop |
| `acquire/lobby.py` | Keeps this server's entry up to date in the FujiNet game lobby |
| `serve.py` | The asyncio TCP server |
| `testclient.py` | Protocol client for a modern PC: interactive, or `--auto` random play from wire data only |
| `tests/test_rules.py` | Rulebook edge cases |
| `tests/test_server.py` | End-to-end over TCP: full game, timeouts, drop/reconnect, faults |
| `sim.py` | Bot-vs-bot soak test with invariant checks after every action |

```sh
python3 serve.py                              # port 6502; --timeout 60 --bot-delay 1.0 --keepalive 10
python3 serve.py --trace                      # the log shows the game as players see it; --trace adds every frame clients send
python3 serve.py --think-exploit 20,10 --think-buy 12,5   # bots take 10-30 s to run an exploit, 7-17 s to buy
python3 serve.py --abandon 300                # drop a game after 5 min with no player connected (default 600; 0 = never)
python3 serve.py --afk 3                      # autopilot after 3 timeouts in a row (default 2; 0 = never)
python3 serve.py --link-timeout 30            # drop a client that vanished without closing after 30 s (default 60)
python3 serve.py --region NEO-LAGOS-3         # the fictional grid region in the title bar and log (default NEO-KANSAI-1)
python3 serve.py --no-jack-in                 # newcomers to a game in progress get TABLE FULL, not a bot seat
python3 serve.py --host-wait 120              # a host keeping others waiting has 2 min to start (default 60; 0 = never)
python3 serve.py --lobby TCP://wire.example.com:6502/ --lobby-client TNFS://tnfs.example.com/synack.atr
                                              # list the server in the FujiNet game lobby (see below)
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
- A host alone in the lobby can take as long as they like. Once another human is waiting, the host's prompt shows
  a countdown of `--host-wait` seconds (default 60); if it runs out, the next human becomes host with a fresh
  countdown, so an absent host can't hold the table. Players on autopilot are skipped (unless everyone waiting
  is), and a host on autopilot hands over at once when an active player arrives. The others are told who may start and for how long
  (`FRED HAS 60 S TO START`), again at each handoff and on arrival. The log says `host passes to NAME`.
- The host also hears how many players are at the title screen (connected, still typing a handle), each time
  that changes, whether they join or leave: `2 AT THE DOOR`. They don't start the countdown.
- Once a game starts, new players get fault 2 (table full). A token reclaims a seat at any time. Without a token
  (the Atari was switched off, say), joining under the handle of a seat that is away reclaims it, and `w` hands
  the client that seat's token. A connected player's seat, or one handed to the bot with `Q`, can't be taken
  this way.
- A prompt that times out is answered by that seat's bot; the seat stays human. After `--afk` timeouts in a row
  (default 2, a whole turn) the player is on autopilot: shown as away (`?`) to everyone, still prompted, but on the
  bot's clock (its thinking time, at least 10 s, never more than `--timeout`). Answering one in time ends it.
  Everyone gets a log line when autopilot starts and when it ends (theme `autopilot`, `returned`).
  Autopilot lasts through a dropped game and into the next until the player presses a key (starting a game
  counts).
- A dropped connection marks the seat away and its bot plays until the token comes back. `Q` hands the seat to
  the bot for good. A client that vanishes without closing (switched off, Wi-Fi gone) counts as dropped after
  `--link-timeout` seconds (default 60): the server sets `TCP_USER_TIMEOUT`, so its unacknowledged keepalives end
  the connection. Without that option (it is Linux only) the system's own limit applies, about 15 minutes.
- Someone new joining a game in progress jacks in for a bot seat: the one worth least, cash and crypto at current
  prices. It becomes theirs, holdings and kit included, under their handle. The newcomer is told first (theme
  `takeover`), then everyone gets a log line (`jackin`). Away seats are kept for their owners; with no bot seat
  the newcomer gets TABLE FULL, and so does everyone with `--no-jack-in`.
- A client at its title screen can watch a table before joining (`I`): it is sent the region and a one-line status
  (theme `[status]`), and a new status whenever that changes. A join refused as full, or because the handle is
  already playing there (fault 5), leaves the connection open and watching, so the player can try again from the
  title screen.
- With no human connected at all, the game pauses instead of the bots playing it out for nobody, and play resumes
  when someone reconnects (by token or handle). After `--abandon` seconds (default 600) the game is dropped and the
  table is an empty lobby again, so it doesn't stay full; an old token then gets fault 3 and its client joins as new.
  The log says `paused`, `resumed` or `abandoned`.
- The same `--abandon` time applies when players are connected but every one of them is on autopilot (walked away
  with the Atari on). The game is dropped with no result. The connected players stay, in a fresh lobby with the
  first of them as host, and see `GAME DROPPED: NOBODY PLAYING`.
- Bots, and away seats, wait `--bot-delay` before each decision. `--think-exploit` and `--think-buy` give them a
  thinking time instead before running an exploit and before buying: `MEAN,JITTER` in seconds, drawn evenly
  from MEAN-JITTER to MEAN+JITTER. A prompt that timed out is answered at once.
- After a game the table returns to the lobby. Bot and away seats stay visible until a new player needs the chair,
  and a reconnecting player is sent the result (`e`) of the last game.

## FujiNet game lobby
With `--lobby ADDRESS --lobby-client URL` the server lists itself in the FujiNet game lobby
(https://lobby.fujinet.online, from github.com/FujiNetWIFI/fujinet-lobby), so players can pick it from the lobby
program on their Atari. The lobby boots the client disk from `--lobby-client` (a TNFS URL to `synack.atr`) and
saves `--lobby` (`TCP://host:port/`, the server's public address) in an AppKey; the client reads it and dials
`N:` + it (client/README.md).
- The entry: game `SYN-ACK WIRE` (theme title), server name = the `--region`, real-world region `--lobby-region`
  (1-2 letters, default `us`), platform `atari`, appkey `--lobby-key` (default 11; it must match the client's
  `LOBBY_KEY`). Each server process is its own entry, so four regions are four `serve.py` on four ports.
- PLAYERS shows table 0, the one clients join. In the lobby: humans here of 6. In a game: the seats humans hold
  (away ones too) of the seats playing, the rest being bots a newcomer can jack in for; with `--no-jack-in` a game
  shows full.
- The server posts when that changes (at most once a second) and every 5 minutes regardless, since the lobby shows
  how long ago each server last posted. Ctrl-C or SIGTERM deletes the entry.
- The public lobby only accepts a listing from, and advertising, a publicly reachable address. To try one out, use
  `--lobby-url https://qalobby.fujinet.online`, or run the lobby server locally (its `server/`, with
  `db/lobby.sqlite3` made from `lobby_schema.sql`) and use `--lobby-url http://127.0.0.1:8080`.
