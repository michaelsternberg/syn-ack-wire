# SYN-ACK WIRE client

The 8-bit side. The server enforces every rule; the client draws what it is told and answers prompts
(`../docs/protocol.md`). Common code is C written for cc65's code generator; platform code lives in `src/<platform>/`.

| Path | What |
|---|---|
| `src/platform.h` | The contract each platform implements: network calls, the receive ring, keys, a tick counter |
| `src/display.h` | Everything the game shows. One text version per platform; hand-made screens can replace it |
| `src/config.h` | Build choices: `FANCY_TITLE`, `FANCY_PLAYFIELD`, `HINTS`, server `URL`, `TABLE` |
| `src/common/main.c` | Title, connect, the main loop, reconnect with the seat token |
| `src/common/game.c` | Game state from server frames; keys → prompt answers, checked locally first |
| `src/common/proto.c` | Frame assembler: bytes in `rx_ring` → one whole frame in `msg[]` |
| `src/common/proto_tab.h` | Frame lengths, **generated** from `server/acquire/proto.py` by `tools/gen_proto.py` |
| `src/common/theme_tab.h` | Words, syndicate letters and names, rule numbers, **generated** from `theme/theme.toml` |
| `src/atari/display_text.c` | Text-only screens: OS 40×24 text, written straight to screen memory |
| `src/atari/input.c` | Keyboard (the OS's last-key byte, no K: handler) and the frame counter |
| `src/atari/net_sio.s` | Network calls for the Atari: FujiNet `N1:` ($71) over raw SIO |
| `src/atari/ring.s` | `rx_ring` = page 6 ($0600): page-aligned, so the 8-bit indexes wrap for free |
| `src/atari/game.cfg` | cc65's `atari.cfg` plus a 1K-aligned `FONT` segment |
| `test/wiremon.c` | Protocol monitor; proves the network layer before any game screens exist |
| `test/autoplay.c` | Test build that presses keys itself and BRKs for screen dumps |
| `test/pc_harness.c` | The common code on a PC with gcc: sockets, printed display, autoplay keys |
| `test/bakeoff.c`, `test/net_fnlib.c` | Size comparison: raw SIO vs fujinet-lib |
| `tools/run-test.sh` | Runs an `.xex` on atari800 with FujiNet-PC (NetSIO) |
| `tools/screen2txt.py` | Shows a screen dump as text |
| `vendor/` | fujinet-lib 5.0.0, pinned (see its README) |

## Build
```sh
make                                     # build/synack.xex (the game), build/wiremon.xex, build/wiremon-auto.xex
make URL=N:TCP://192.168.1.20:6502/      # aim them at another server
make FANCY=1                             # hand-made screens instead of text (once they exist)
make HINTS=0                             # move hints start off (the player can still turn them on)
make test                                # build/synack-auto.xex: plays by itself, for the emulator
make pc                                  # build/pc-client: the common code on this PC
make bakeoff
```
Toolchain: cc65 master, `V2.19 - Git 8d86ca7` (2026-10-18). Size numbers below are from that build. `make bakeoff`
works with it directly; the 2.19 release can't link fujinet-lib 5.x.

## Play it in the emulator
```sh
cd ../server && python3 serve.py --trace           # terminal 1
cd ../client && tools/run-test.sh build/synack.xex  # terminal 2
```
`run-test.sh` starts FujiNet-PC from `$FUJINET_DIR` (default: your `fujinet-firmware/build/dist`) with a scratch copy of
its `fnconfig.ini`, with the config disk turned off, then runs `atari800 -xl -nobasic -netsio -run <xex>`. The emulator
loads the program itself, so FujiNet only provides the network. Your own `fnconfig.ini` is never changed. Like
`run-fujinet`, it restarts FujiNet-PC when it exits with code 75, which happens when the emulator resets it on connect.
PROCEED needs atari800 7.2.1 or newer (installed in `/usr/local/bin`); `ATARI800=/path/to/atari800` picks another build.

A table that already has a game running refuses newcomers ("TABLE FULL"). Restart the server or wait for the game to end.

### Keys
| Prompt | Keys |
|---|---|
| Title | type a handle, **Return**. **\*** turns move hints on or off |
| Any time | **\*** turns move hints on or off |
| Host in the lobby | **2–6**: start with that many seats; bots fill the empty ones |
| Your turn | **1–6**: run that exploit from your kit. Inverse = can't run now; pressing it says why |
| Infiltration | the **letter** of the syndicate to broker the new hive to |
| Tie | the **letter** of the syndicate that survives (or is absorbed next) |
| Hivejack | how many to cash out, **Return**; how many to swap (even), **Return**; the rest HODL |
| Buy | up to 3 **letters** (repeat a letter to buy more of it), **Delete** to undo, **Return**; then **Y/N** if you may call the null-route |

The client checks each answer before sending it and says why on the prompt line ("NOT ENOUGH NEW YEN"). The
server still decides; a refused answer comes back as a log line and the same prompt. The number at the right of the
prompt line is the seconds left before your seat's bot answers for you.

### Screen
```
 SYN-ACK WIRE              NEO-KANSAI-1    title bar (inverse)
  ABCDEFGHIJKL HIVES
0 .ggg..ggg.dd g GHOST//MODEM         11   hives: inverse letters; + spike; . empty
2 rr...g.*.... d HOLY DAEMON           2   * = your kit can run here (move hints)
...                                        the last move: its highlight flipped
8 *oo..l*l...+ LAST D6                     during a buy the panel shows price and supply
 COWBOY          ¥  g  r  d  i  b  l  o
>ATARI        3100  4  1  .  2  2  3  2    > you; inverse name = whose turn; * bot; ? away
 [$] ROOT K1LLSWCH +¥7000                  4 log lines
 EXPLOITS 1:H2 2:C5 3:H6 4:A8 5:G8 6:D6    your kit, or what the open prompt is about
 RUN WHICH EXPLOIT? PRESS 1-6          20  the question, seconds left
```
(In this sketch lowercase marks an inverse letter.) Every word on screen that names a game thing comes from
`theme.toml` through `theme_tab.h`, as do the glyphs (`.` `+` `*`). The panel shows each syndicate's `screen` name,
or its `name` when that fits in 20 characters. The yen sign is the `\` glyph of a RAM copy of the ROM font.

### Fancy screens
`display.h` is the whole interface. `display_title_screen()` is switched by `FANCY_TITLE`; everything else by
`FANCY_PLAYFIELD` (both in `config.h`, or `make FANCY=1` for both). With a switch at 1, `display_text.c` leaves those
functions out, and a hand-made file (e.g. `src/atari/display_fancy.s`) must provide them. It reads the game state
declared in `src/common/game.h`. Nothing in `common/` changes.

## Wiremon
```sh
tools/run-test.sh build/wiremon.xex
```
Keys: **2–6** start a game with that many seats (host only), **R** resync, **Q** leave. Every frame prints as its
type letter and payload in hex; log lines (`l`) print as text. The top lines count frames, bytes and bad frames. At
connect it asks for 10 full resyncs to push a few KB through the ring. `wiremon-auto.xex` also starts a 3-seat game by
itself and sends `Q` when it is over, or on the first bad frame. So a run can be read from the server's `--trace`
log with no screen: `HEADLESS=1 tools/run-test.sh build/wiremon-auto.xex`.

## Testing without watching
- **`make pc && build/pc-client [host] [port]`** runs `main.c`, `game.c` and `proto.c` compiled with gcc against a
  real server, pressing autoplay keys and printing prompts, logs and what it sends. It is the quickest way to find a
  logic bug, and gdb works on it.
- **`DUMPDIR=dir tools/run-test.sh build/synack-auto.xex`** runs the real Atari build headless. It executes `BRK` when
  a new prompt opens and every 4 seconds; atari800 `-bbrk` stops in its monitor, which saves screen memory to
  `dir/scrNNN.bin` and continues. `tools/screen2txt.py dir/scr042.bin` shows one as text.

## Network layer (Atari)
- **Five SIO commands to $71:**
  - `O` open: 256-byte devicespec, AUX1 12 = read/write, **AUX2 0 = no translation**. The protocol is binary.
  - `S` status: 4 bytes (bytes waiting, connected, error).
  - `R` read.
  - `W` write.
  - `C` close.
- **PROCEED:** FujiNet pulls SIO pin 9 (the PIA's CA1) when it has data.
  - `net_open` enables the CA1 interrupt (PACTL bit 0, OR'd in so the motor bit survives) and points `VPRCED` at a
    6-byte handler that sets a `ready` flag.
  - The latched flag can't be polled directly, because the OS vertical blank reads PORTA for the joysticks every
    frame, which clears it.
- **`net_poll()`:** costs a few instructions unless `ready` is set or a second has passed (the safety net). Then it
  makes one status call and at most one read of what fits:
  - It never overruns bytes the assembler hasn't taken yet.
  - It stops at the end of the ring's page. If bytes are left behind, `ready` stays set and the next poll gets them.
  - Call it from the main loop, never from an interrupt.
- **SIO beeps:** `net_open` sets SOUNDR ($41) to 0. Otherwise every poll would beep on real hardware.
- **Link down:** any non-zero return means the link is down: an SIO error (128–255) or `NET_CLOSED`.
  - The client closes, reopens, and sends `H` with its saved token.
  - The server answers with a full resync, so there is no other recovery code.
- **Sound:** SIO uses POKEY's serial clock, so game sound should use channels 1 and 2.

## Sizes (bytes)
| Piece | Code | Notes |
|---|---|---|
| `synack.xex` | 9167 | whole file; loads at $2000, the 1K font sits at $4400 |
| `display_text.c` | 2738 | + 534 rodata (mostly screen words), 70 bss, 1K font |
| `game.c` | 2414 | + 282 rodata, 279 bss (the game state) |
| `main.c` | 572 | + 143 rodata |
| `net_sio.s` | 299 | + 10 rodata, 25 bss. 222 before PROCEED/SOUNDR |
| `proto.c` | 180 | + 26-byte length table, 128-byte `msg[]`; hand assembly could roughly halve it |
| `input.c` | 47 | + 64-byte key table |
| Bake-off, network added on top of the same test program | | |
| raw SIO | 228 | |
| fujinet-lib 5.0.0 | 1712 | open/status/read_nb/write/close plus the cc65 runtime and memcpy/memmove it pulls in |
| `wiremon.xex` | 2982 | whole file, including cc65's Atari startup code |

## Tested (atari800 7.2.1 + FujiNet-PC 1.6 + server, NetSIO)
- **Game client:** the autoplay build played for four minutes as host with two bots. It sent 75 answers covering
  every prompt kind, and the server rejected none. 156 screen dumps checked by eye. The PC build played a whole game
  to the end (47 answers, 0 rejected).
- **Connect and hello:** wiremon gets its seat.
- **Ring stress:** 9 back-to-back resyncs, then a whole 3-seat game, with every frame parsed and 0 bad frames.
- **PROCEED:** over a one-minute game there were 163 status calls and 145 reads. Polling blindly at 15 Hz would have
  made about 840 status calls.
- **Reconnect:** a relay cut the TCP connection mid-game. Wiremon reconnected 2.7 s later with its token, got its
  seat back (the server only allows that mid-game for a valid token), and followed the game to the end.
