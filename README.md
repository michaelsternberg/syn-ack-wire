# SYN-ACK WIRE

A cyberpunk network game of hacking and crypto for the Atari 8-bit computers and
[FujiNet](https://fujinet.online/). Two to six netrunners spike nodes on the Grid, a region-sized cloud where
crime syndicates grow botnet hives. Back the syndicates with crypto, sell out when a bigger hive hivejacks
yours, and hold the most new yen when the region is null-routed.

Based on the rules of Sid Sackson's classic board game *Acquire*.

## How it fits together

```
 Atari 800/XL/XE  --SIO-->  FujiNet  --TCP-->  server (Python)
   client/                   N: device          server/
```

The **server** runs the game: it enforces every rule, keeps each table's state, plays the bots and lists itself
in the FujiNet game lobby. The **client** is a small 8-bit program that draws what the server tells it and sends
back the player's answers. The two speak a compact binary protocol, documented in
[`docs/protocol.md`](docs/protocol.md).

All names and terms (syndicates, coins, vocabulary, rule numbers) come from one file,
[`theme/theme.toml`](theme/theme.toml). The server reads it directly; the client headers and the rulebook are
generated from it.

## The server

Python 3.11 or newer, standard library only: nothing to install.

```sh
cd server
python3 serve.py                        # listens on port 6502
python3 serve.py --trace                # log every frame, to watch a game as the players see it
python3 -m unittest -v                  # the tests: rule edge cases, and full games over TCP
python3 sim.py 2000                     # soak test: 2000 bot-vs-bot games with invariant checks
```

- **Tables:** the first player to join hosts and chooses the number of seats; bots fill the empty ones. A
  dropped player can reconnect to the same seat, and a bot plays for them while they're away.
- **Bots:** a random player and a heuristic one; `--think-exploit` and `--think-buy` give them human-like
  thinking times.
- **FujiNet lobby:** `--lobby` lists the table in the FujiNet game lobby, with the disk image players boot to
  join it.
- **Testing without an Atari:** `testclient.py` plays a game from a PC terminal, by hand or automatically.
- **Several servers, and the cloud:** `--config` runs several servers in one process, each with its own pace;
  `Dockerfile` and `fly.toml` deploy them to fly.io (see `server/README.md`).

The rules engine is pure (actions in, events out, no I/O), which keeps it easy to test. Options, the engine and
the lobby are described in [`server/README.md`](server/README.md).

## The client

An Atari 8-bit program in C and 6502 assembly, built with [cc65](https://cc65.github.io/). It needs a FujiNet
(real, or FujiNet-PC with an emulator) for the network.

```sh
cd client
make                                     # build/atari/synack.xex, dialing the public server (syn-ack-wire.fly.dev)
make URL=N:TCP://192.168.1.20:6502/      # point it at your server
make atr                                 # build/atari/syn-ack-wire-atari.atr: a bootable disk, with the instructions
make pc                                  # the common code on a PC, for testing against the server
tools/run-test.sh build/atari/synack.xex # run it in atari800 with FujiNet-PC
```

- **Small:** about 12 KB in all. It talks to FujiNet with hand-written raw SIO rather than a library.
- **The screen:** a GR.0 text playfield: the Grid, the syndicates' prices, the netrunners' holdings, a log and
  the prompt. Player/missile graphics and display list interrupts add color bands; the title has animated TV
  static in its block letters.
- **Instructions:** eleven help pages, the last two a QR code for the manual and the credits, read straight from the disk with SIO, any time: on the title or mid-game.
- **Sound:** short chords for game events, from the vertical blank.
- **Lobby aware:** booted from the FujiNet lobby, it dials the chosen server and fills in the player's name.

Building needs cc65 (a recent master build), Python 3, and `dir2atr` for the disk image. Details, keys, the
memory map and the test setup are in [`client/README.md`](client/README.md).

The client code is split so other platforms can share it: the game logic and protocol (`src/common/`) are
portable C, and each platform supplies its network calls, keyboard and display behind `src/platform.h` and
`src/display.h`. An Apple II version is the next one being considered.

## Documentation

| | |
|---|---|
| [`docs/rulebook/`](docs/rulebook/) | The rulebook (LaTeX, with a built PDF) |
| [`docs/manual/atari/`](docs/manual/atari/) | The user manual for the Atari version, with screen captures |
| `docs/rules/`, `docs/style/` | The rules text and the style the rulebook and the manuals share; `make -C docs` builds them all from `theme.toml` |
| [`docs/protocol.md`](docs/protocol.md) | The wire protocol between client and server |
| [`docs/theme.md`](docs/theme.md) | The setting and the reasoning behind its names |
| [`server/README.md`](server/README.md) | Server options, the rules engine, tables and the lobby |
| [`client/README.md`](client/README.md) | Building, keys, screen layout, the help pages, sizes and testing |

## Repository layout

```
client/     the 8-bit client: src/common (portable), src/atari, help pages, tools
server/     the Python server, bots, tests and simulator
docs/       rulebook, platform manuals, their shared rules text and style, protocol, theme notes
theme/      theme.toml: every name and rule number
tools/      generators for the client's and the documents' tables
```

## License

Copyright (C) 2026 Michael Sternberg

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
option) any later version. It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
see [`LICENSE`](LICENSE) for details.

*Acquire* is a trademark of Hasbro, Inc. SYN-ACK WIRE is an independent fan project, not affiliated with or
endorsed by Hasbro or Avalon Hill. It uses the game's rules, written up and themed anew; it contains none of
the original's text or artwork.
