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
| `acquire/guard.py` | Connection limits, and short blocks for addresses that keep misbehaving |
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
python3 serve.py --tables 0,1                 # tables clients may use (default 0; others are refused)
python3 serve.py --config servers.toml        # several servers in one process (see servers.example.toml)
python3 serve.py --host-wait 120              # a host keeping others waiting has 2 min to start (default 60; 0 = never)
python3 serve.py --lobby TCP://wire.example.com:6502/ --lobby-client TNFS://tnfs.example.com/syn-ack-wire-atari.atr \
                 --lobby-client apple2=TNFS://tnfs.example.com/syn-ack-wire-apple2.po
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
  countdown, so an absent host can't hold the table; the old host keeps their seat. Players on autopilot are skipped (unless everyone waiting
  is), and a host on autopilot hands over at once when an active player arrives. The others are told who may start and for how long
  (`FRED HAS 60 S TO START`), again at each handoff and on arrival. At each handoff, and when a host leaves the
  lobby, everyone there, the new host included, first reads `KEEN IS THE HOST NOW` (theme `newhost`). The log says
  `host passes to NAME`.
- The host also hears how many players are at the title screen (connected, still typing a handle), each time
  that changes, whether they join or leave: `2 AT THE DOOR`. They don't start the countdown.
- Once a game starts, new players get fault 2 (table full). A token reclaims a seat at any time. Without a token
  (the Atari was switched off, say), joining under the handle of a seat that is away reclaims it, and `w` hands
  the client that seat's token. Under any other handle the player joins as someone new. A connected player's seat, or one handed to the bot with `Q`, can't be taken
  this way.
- Each prompt has `--timeout` seconds (default 60), the clock shown on the client's prompt line. A prompt that
  times out is answered by that seat's bot; the seat stays human. After `--afk` timeouts in a row
  (default 2, a whole turn) the player is on autopilot: shown as away (`?`) to everyone, still prompted, but on the
  bot's clock (its thinking time, at least 10 s, never more than `--timeout`). Answering one in time ends it.
  Everyone gets a log line when autopilot starts and when it ends (theme `autopilot`, `returned`).
  Autopilot lasts through a dropped game and into the next until the player presses a key (starting a game
  counts).
- A dropped connection marks the seat away and its bot plays until the token comes back. `Q` (the Atari's Esc, then Y)
  hands the seat to its bot for good, at once: it takes a free bot name, its token is forgotten, and a newcomer
  can jack in for it (theme `logoff`). A client that vanishes without closing (switched off, Wi-Fi gone) counts as
  dropped after `--link-timeout` seconds (default 60), at the title screen too: keepalives start as soon as a client
  connects, and the server sets `TCP_USER_TIMEOUT`, so its unacknowledged keepalives end the connection. Without
  that option (it is Linux only) the system's own limit applies, about 15 minutes.
- Someone new joining a game in progress jacks in for a bot seat: the one worth least, cash and crypto at current
  prices. It becomes theirs, holdings and kit included, under their handle. The newcomer is told first (theme
  `takeover`), then everyone gets a log line (`jackin`). Away seats are kept for their owners; with no bot seat
  the newcomer gets TABLE FULL, and so does everyone with `--no-jack-in`.
- A client at its title screen can watch a table before joining (`I`): it is sent the region and a one-line status
  (theme `[status]`), and a new status whenever that changes. A join refused as full, or because the handle is
  taken there, by a player or a bot (fault 5), leaves the connection open and watching, so the player can try again from the
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

## Several servers in one process
`--config servers.toml` runs several servers, each on its own port with its own region, pace and lobby entry;
`servers.example.toml` is a commented example with four. The settings are named like the options above, and any
left out take the same defaults. The file is checked when the server starts: unknown settings, a port or region used
twice (players tell servers apart by region), a shot clock over 255 s, or a lobby name over 32 characters stop it
with a message naming the server and the setting. Only `--host` and `--trace` go with `--config`.

- Each server's lobby entry reads `REGION - description` (`description = "relaxed pace"`; `--description` for a
  single server), at `TCP://<[lobby] address>:<port>/`.
- They share one guard: an address blocked on one port is blocked on all, and the connection limits count across
  them.
- With more than one server, each log line starts with the region of the server it comes from.
- Ctrl-C or SIGTERM stops at once: the lobby entries are removed, then every connection is closed, games in
  progress included. (A cloud host sends SIGTERM to redeploy; waiting for the games to end could take hours.)

## Deploying to fly.io
The repository root has what [fly.io](https://fly.io) needs: `Dockerfile` (Python and the server, plus `tnfsd` to
serve the client disks to the FujiNet lobby), `fly.toml` (one small machine, always on, a TCP service per server and a
UDP one for `tnfsd`, and an HTTPS one for the QR codes' short links) and `server/servers.fly.toml` (the four servers: two relaxed, one quicker, one fast). Fly builds
the image on its own machines, so Docker isn't needed locally. Cost at the time of writing: about $2/month for the
machine (256 MB) and $2/month for the dedicated IPv4 address that raw TCP and UDP services need; the host name
`<app>.fly.dev` comes with the app.

1. Install `flyctl` and log in: `fly auth login`.
2. In `fly.toml`, set `app` to the name you want (it must be unique on fly.io; players dial `<app>.fly.dev`) and
   `primary_region` to a region near your players (`fly platform regions` lists them). Then create the app:
   `fly apps create <app>`.
3. Give it a dedicated IPv4 address: `fly ips allocate-v4`. (The free shared address only carries HTTP and TLS;
   FujiNet's `N:TCP://` is neither.)
4. Build the game disks for it: `cd client && make atr URL=N:TCP://<app>.fly.dev:6502/` and
   `make apple2_text-po URL=N:TCP://<app>.fly.dev:6502/ PRODOS=...` and
   `make msdos-img URL=N:TCP://<app>.fly.dev:6502/ WATCOM=...` and
   `make coco-dsk URL=N:TCP://<app>.fly.dev:6502/ DECB=...` and `make nes URL=N:TCP://<app>.fly.dev:6502/` and
   `make adam URL=N:TCP://<app>.fly.dev:6502/` and `make h89-disk URL=N:TCP://<app>.fly.dev:6502/ H89SYS=...`
   (the default URL is this project's own app, syn-ack-wire.fly.dev).
   The image takes `client/build/atari/syn-ack-wire-atari.atr`, `client/build/apple2_text/syn-ack-wire-apple2.po`,
   `client/build/msdos/syn-ack-wire-msdos.img`, `client/build/coco/syn-ack-wire-coco.dsk`,
   `client/build/nes/syn-ack-wire-nes.nes`, `client/build/adam/syn-ack-wire-adam.ddp` and
   `client/build/h89/syn-ack-wire-h89.h8d` as they are when you deploy, and serves them at
   `TNFS://<app>.fly.dev/syn-ack-wire-atari.atr`, `.../syn-ack-wire-apple2.po`,
   `.../syn-ack-wire-msdos.img`, `.../syn-ack-wire-coco.dsk`, `.../syn-ack-wire-nes.nes`,
   `.../syn-ack-wire-adam.ddp` and `.../syn-ack-wire-h89.h8d` (the NES lobby mounts a `.nes` and boots it:
   fujinet-lobby, branch `nes`; the Adam's mounts the `.ddp` in slot 0 and reboots: its shared client, branch
   `client-qr-screen`); the first two under their old names too, `synack.atr` and `synack.po` (v0.2.0 and
   before), for links saved then, and the CoCo's as `synack.dsk`, the name the lobby boots: it runs
   `RUNM"<the disk's name>"`, which Disk BASIC takes only up to 8 letters. A disk booted directly dials that one
   server (6502 is a relaxed one here); the FujiNet lobby gives each server its own port.
5. Deploy, as **one** machine: `fly deploy --ha=false`, or `deploy/deploy.sh`, which does it only while no one
   is playing: no players on this app's servers in the lobby, and no game activity in `fly logs` for 10 minutes
   (`--check` only reports). (By default fly.io starts two machines for redundancy, which
   here would be two separate servers behind one address, splitting players between unrelated games.)
   `fly status` should show one machine, started; `fly logs` shows `tnfsd` and the four servers listening.
6. To list the servers in the lobby, uncomment `[lobby]` in `servers.fly.toml`, set `address` and `clients` (the
   disks above, one for each platform), keep `url` on the QA lobby until a listing looks right, and deploy again.

To change the servers, edit `servers.fly.toml` (and `fly.toml` if ports change: a test checks the two agree), then
`deploy/deploy.sh`; to change a disk, `make atr`, `make apple2_text-po`, `make msdos-img`, `make coco-dsk`,
`make nes`, `make adam` or `make h89-disk` and `deploy/deploy.sh`. A deploy restarts the machine: games in progress end,
and the servers leave the lobby and rejoin it.

The game disks' help pages carry a QR code for `https://<app>.fly.dev/ATARI` (the client's `QR_URL`). `fly.toml`'s
HTTPS service (TLS by fly.io; port 80 plain HTTP) passes it to `servers.fly.toml`'s `[web]` section, which
redirects `/<PLATFORM>` to that platform's manual on GitHub (`docs/manual/<platform>/`, so a new platform needs
nothing here) and `/` to the project. Only redirects: a manual changes with a push, not a deploy. The links lead
to GitHub, so they work once the repository is public. The same section answers `GET /SEATS` with a line for
each server, `REGION TAKEN SEATS` (the lobby's PLAYERS column), for the desktop client's Region menu; it reads
it over plain HTTP on port 80, which is why that port isn't sent to 443.

fly.io delivers UDP only to a program listening on the machine's `fly-global-services` address, which stock `tnfsd`
can't do (it listens on all addresses), so the image builds it from upstream with `deploy/tnfsd-bind-address.patch`,
which adds `-b <address>`. `deploy/start.sh` starts it read-only, then the servers. There is no TCP service for it:
through fly.io's proxy every player would share one address, and `tnfsd` allows 16 sessions per address; FujiNet
tries TCP first and falls back to UDP.

Behind fly.io's proxy, every connection would seem to come from the proxy. `fly.toml` turns on its `proxy_proto`
handler, which sends a PROXY protocol header naming the player first, and `servers.fly.toml` sets
`proxy-protocol = true`, so the server reads it and its limits and blocks apply to players (a connection without the
header is closed). `--proxy-protocol` does the same for a single server.

## On the public internet
A port open to the internet is soon found by scanners. `acquire/guard.py` keeps them from wearing the server down:

- A new connection must send its first frame within 10 s, and it must be `I` or `H` for an allowed table
  (`--tables`, default 0). A frame's payload must follow its type byte within 10 s. A client does all of this at
  once; a title screen may then wait as long as its player types.
- Breaking those rules, sending a token no seat has, or an unknown frame type after joining, is a strike against the
  address. Three strikes within 5 minutes block it for 15 minutes: its new connections are closed at once, without
  a reply. The log has a line per strike and one when the block starts, not one per refused connection.
- At most 12 connections at once from one address (a LAN party behind one router) and 200 in all.
- Handles are cleaned on arrival to what a client's title screen can type, so a crafted handle can't carry control
  codes into the log or slip past the one-handle-one-seat check.
- Nothing a client sends reaches a shell, the filesystem or an interpreter: frames have fixed lengths, unknown types
  end the connection, and text to clients is limited to the wire character set. `tests/test_guard.py` throws random
  frames at a running server, in the lobby and in games, and checks it still plays a game and its tables stay
  consistent.

The limits are constants at the top of `guard.py`. Behind a proxy that hides the client's address (a cloud
service's TCP proxy, say), every connection seems to come from the proxy, so blocking would shut everyone out: run
the server with the proxy's PROXY protocol header turned on and `--proxy-protocol` (as on fly.io, above).

## FujiNet game lobby
With `--lobby ADDRESS --lobby-client URL` the server lists itself in the FujiNet game lobby
(https://lobby.fujinet.online, from github.com/FujiNetWIFI/fujinet-lobby), so players can pick it from the lobby
program on their Atari, Apple II, PC, CoCo, NES or Adam. The lobby boots the client disk for the player's platform from
`--lobby-client` (a TNFS URL to `syn-ack-wire-atari.atr`; `apple2=` and a TNFS URL to `syn-ack-wire-apple2.po`
for the Apple, `msdos=`, `coco=`, `nes=` and `adam=` likewise, the CoCo's disk named in 8 letters at most, `synack.dsk`; the
option once for each platform; `clients` in a `--config` file's `[lobby]`) and saves `--lobby`
(`TCP://host:port/`, the server's public address) in an AppKey; the client reads it and dials `N:` + it
(client/README.md).
- The entry: game `SYN-ACK WIRE` (theme title), server name = the `--region`, real-world region `--lobby-region`
  (1-2 letters, default `us`), a client disk for each platform (`atari`, `apple2`, `msdos`, `coco`, `nes`, `adam`), appkey `--lobby-key` (default
  11; it must match the clients' `LOBBY_KEY`). Each server process is its own entry, so four regions are four
  `serve.py` on four ports. One entry serves every platform: each platform's lobby program lists the entries with a
  disk for it, so players on every platform meet at the same tables.
- PLAYERS shows table 0, the one clients join. In the lobby: humans here of 6. In a game: the seats humans hold
  (away ones too) of the seats playing, the rest being bots a newcomer can jack in for; with `--no-jack-in` a game
  shows full.
- The server posts when that changes (at most once a second) and every 5 minutes regardless, since the lobby shows
  how long ago each server last posted. Ctrl-C or SIGTERM deletes the entry.
- The public lobby only accepts a listing from, and advertising, a publicly reachable address. To try one out, use
  `--lobby-url https://qalobby.fujinet.online`, or run the lobby server locally (its `server/`, with
  `db/lobby.sqlite3` made from `lobby_schema.sql`) and use `--lobby-url http://127.0.0.1:8080`.
