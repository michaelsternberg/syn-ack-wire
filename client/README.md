# SYN-ACK WIRE client

The 8-bit side. The server enforces every rule; the client draws what it is told and answers prompts
(`../docs/protocol.md`). Common code is C written for cc65's code generator; platform code lives in `src/<platform>/`.

| Path | What |
|---|---|
| `src/platform.h` | The contract each platform implements: network calls, the receive ring, keys, a tick counter |
| `src/display.h` | Everything the game shows. One text version per platform; hand-made screens can replace it |
| `src/config.h` | Build choices: `FANCY_TITLE`, `FANCY_PLAYFIELD`, `HINTS`, server `URL`, `LOBBY_KEY`, `TABLE`, `DISPLAY_IDLE` |
| `src/common/main.c` | Title (with the table's status), connect, the main loop, reconnect with the seat token |
| `src/common/game.c` | Game state from server frames; keys → prompt answers, checked locally first |
| `src/common/proto.c` | Frame assembler: bytes in `rx_ring` → one whole frame in `msg[]` |
| `src/common/proto_tab.h` | Frame lengths, **generated** from `server/acquire/proto.py` by `tools/gen_proto.py` |
| `src/common/theme_tab.h` | Words, syndicate letters and names, rule numbers, **generated** from `theme/theme.toml` |
| `src/atari/display_text.c` | Text-only screens: OS 40×24 text, written straight to screen memory |
| `src/atari/snow.s` | The title's static: a deferred VBI fills the art's letters from POKEY's `RANDOM` |
| `src/atari/dli.s` | The playfield's color bands: display list interrupts behind the seats and the info row |
| `src/atari/title4.atascii` | The title art (ATASCII, $9B line ends) |
| `src/atari/title_art.h`, `.inc` | The art in screen codes and the static's masks, **generated** by `tools/title_art.py` |
| `src/atari/input.c` | Keyboard (the OS's last-key byte, no K: handler) and the frame counter |
| `src/atari/net_sio.s` | Network calls for the Atari: FujiNet `N1:` ($71) over raw SIO |
| `src/atari/ring.s` | `rx_ring` = page 6 ($0600): page-aligned, so the 8-bit indexes wrap for free |
| `src/atari/game.cfg` | cc65's `atari.cfg` plus 1K-aligned `FONT` and `PM` segments |
| `src/apple2/display_text.c` | Apple II text screens (apple2_text): 40×24, page 1, normal/inverse/flash |
| `src/apple2/net_fn.c` | Network calls for the Apple II: FujiNet over SmartPort, through fujinet-lib |
| `src/apple2/input.c` | Keyboard latch, Open-/Closed-Apple, and `ticks()` from FujiNet's clock |
| `src/apple2/help.s` | The instruction pages on text page 2, read with the ProDOS MLI |
| `src/apple2/sound.s` | Chords as short notes on the speaker |
| `src/apple2/ring.s`, `game.cfg` | `rx_ring` at $0C00; cc65's `apple2-system.cfg`: a ProDOS system file at $2000 |
| `src/apple2/title.txt` | The Apple title logo, plain text; `title_art.h` is **generated** by `tools/title_text.py` |
| `lib/fujinet-lib/` | fujinet-lib, a pinned copy (version and checksum in its README) |
| `test/wiremon.c` | Protocol monitor; proves the network layer before any game screens exist |
| `test/autoplay.c` | Test build that presses keys itself and BRKs for screen dumps |
| `test/pc_harness.c` | The common code on a PC with gcc: sockets, printed display, autoplay keys |
| `tools/run-test.sh` | Runs an `.xex` on atari800 with FujiNet-PC (NetSIO) |
| `tools/screen2txt.py` | Shows a screen dump as text |
| `tools/render_screen.py` | Draws a screen dump as the screen looks, for the manual's captures |
| `tools/help_apple2.py` | Checks the Apple instruction pages and writes them as text-page images (`SYNACK.HLP`) |
| `tools/prodos_boot.py`, `title_text.py` | The Apple disk's ProDOS boot block; the Apple title logo as a header |
| `tools/apple2_preview.py` | Draws a screen dump as Apple II hi-res would show it on a colour monitor, three ways |
| `tools/capture.sh` | Captures the manual's staged screens (`docs/scenarios.toml`, `server/stage.py`) on atari800 |

## Build
```sh
make                                     # build/atari/synack.xex (the game), build/atari/wiremon.xex, build/atari/wiremon-auto.xex
make URL=N:TCP://192.168.1.20:6502/      # aim them at another server (default: syn-ack-wire.fly.dev:6502)
make atrtest                             # build/atari/local/synack.atr: the disk, dialing localhost:6502
make LOBBY_KEY=0                         # ignore the game lobby's AppKeys (see below)
make FANCY=1                             # hand-made screens instead of text (once they exist)
make HINTS=0                             # move hints start off (the player can still turn them on)
make test                                # build/atari/synack-auto.xex: plays by itself, for the emulator
make atr                                 # build/atari/synack.atr: the game disk (PicoBoot + game, help pages in raw sectors)
make pc                                  # build/pc/pc-client: the common code on this PC
```
Toolchain: cc65 master, `V2.19 - Git 8d86ca7` (2026-10-18). Size numbers below are from that build.

## Play it in the emulator
```sh
cd ../server && python3 serve.py --trace                  # terminal 1
cd ../client && tools/run-test.sh build/atari/synack.xex  # terminal 2
```
`run-test.sh` starts FujiNet-PC from `$FUJINET_DIR` (default: your `fujinet-firmware/build/dist`) with a scratch copy of
its `fnconfig.ini`, with the config disk turned off, then runs `atari800 -xl -nobasic -netsio -run <xex>`. The emulator
loads the program itself, so FujiNet only provides the network. Your own `fnconfig.ini` is never changed. Like
`run-fujinet`, it restarts FujiNet-PC when it exits with code 75, which happens when the emulator resets it on connect.
PROCEED needs atari800 7.2.1 or newer (installed in `/usr/local/bin`); `ATARI800=/path/to/atari800` picks another build.

The title screen dials at once and shows the server's region and, under it, its status: what joining would do (a
lobby seat, taking over a bot in a game in progress, or a full table). If joining is refused anyway (someone took the
last seat first, or the handle is taken there), it returns to the title screen and says so, still connected;
Return tries again.

The title art (`TITLE` and `TITLE_ROW` in the Makefile) is ATASCII box drawing: a letter's strokes are the spaces
inside its outline. `tools/title_art.py` finds them (a flood fill from outside each line of letters) and makes the
players' masks; `snow.s` fills those cells with static from POKEY's `RANDOM` every frame, using all four players
side by side (quad width, one bit a character), so the strokes must fit in 32 columns. The static pauses while SIO
runs (the deferred VBI is skipped then), and the help pages hide the players.

**From the FujiNet game lobby:** the lobby saves the chosen server's address in an AppKey (creator 1, app 1, key =
the `appkey` number the server registers with the lobby: `LOBBY_KEY` here and `serve.py --lobby-key` there, both
default 11) and boots the game. Before each dial the client reads that key and, if it holds an address, dials `N:` +
it, so the server registers its address as `TCP://host:port/`. With no key it dials `URL`. The player's lobby name
(key 0) starts the handle on the title screen, uppercased, with characters a handle can't have left out; the player
can still change it. To try it in the emulator, put the keys in FujiNet's SD folder as `FujiNet/00010100.key` (the
name) and `FujiNet/0001010b.key` (the address, key 11 in hex); `run-test.sh` uses an empty scratch folder, so it
never finds any.

**Two Ataris at one table:** give each emulator its own instance number, so each gets its own FujiNet-PC ports.
```sh
cd ../server && python3 serve.py                                     # terminal 1
cd ../client && tools/run-test.sh build/atari/synack.xex             # terminal 2: the first to connect hosts
cd ../client && INSTANCE=1 tools/run-test.sh build/atari/synack.xex  # terminal 3
```
Type a handle and press Return on both. Once the host's screen lists both players, the host presses **2**–**6** to
start; bots take any seats beyond the humans. Both must join before the host starts.

### Keys
| Prompt | Keys |
|---|---|
| Title | type a handle, **Return**. **OPTION** turns move hints on or off |
| Instructions | **>** or **→** next page, **<** or **←** previous, **Esc** (or Return, HELP or ?) back to the title or the game |
| Any time | **HELP** or **?** shows the instructions, on the title or mid-game. **OPTION** turns move hints on or off. **SELECT** turns sound on or off (a chord says it's on) |
| Host in the lobby | **2–6**: start with that many seats; bots fill the empty ones |
| After joining | **Esc**, then **Y**: log off for good. A bot takes over the seat and the client goes back to the title; any other key stays |
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
  NEO-KANSAI-1:CONNECTED    SYN-ACK WIRE  title bar (inverse); NO SIGNAL after 20 s of silence
  ABCDEFGHIJKL   HIVES  SYNDICATES
0 .ggg..ggg.dd   g 11   GHOST//MODEM       hives: inverse letters; + spike; . empty
2 rr...g.2....   d  2   HOLY DAEMON        1-6 = that exploit in your kit can run here (move hints)
...                                        the last move: its highlight flipped
8 6oo..l4l...+ LAST: D6                    each hive's size by its letter; inverse = Black ICE
                                           during a buy: HIVES    PRICE LEFT

 NETRUNNER    ACCT  g  r  d  i  b  l  o
>ATARI       ¥3100  4  1  .  2  2  3  2    > you; inverse name = whose turn; * bot; ? away
 [$] ROOT K1LLSWCH +¥7000                  3 log lines
 EXPLOITS 1:H2 2:C5 3:H6 4:A8 5:G8 6:D6    your kit, or what the open prompt is about
 RUN WHICH EXPLOIT? PRESS 1-6          20  the question, seconds left
```
(In this sketch lowercase marks an inverse letter.) Every word on screen that names a game thing comes from
`theme.toml` through `theme_tab.h`, as do the glyphs (`.` `+`). The panel shows each syndicate's `screen` name,
or its `name` when that fits in 15 characters. The yen sign is the `\` glyph of a RAM copy of the ROM font.

Behind the Grid (columns A-L, rows 0-8) sits a panel one luminance step brighter than the background ($C2 on
$C0): two quad-width players, player 0 over 8 columns and player 1 ($F0) over the other 4, in front of the
playfield. That works on a text screen because a GR.0 letter takes only its luminance from COLOR1 and its hue
from whatever is behind it, here the player, so the letters stay readable. The title screen uses the same
players for its static; leaving it clears them, refills the panel and moves players 2 and 3 off screen. Player
memory is the 1K `PM` segment in `game.cfg`; the missiles are free.

The seats (rows 13-18) sit on the same green as the panel, as a band across the screen, and the info row and the
question (22-23) turn purple ($60, green's complement) while this seat has a decision to make: for its whole turn,
so it doesn't blink between prompts, and for a prompt out of turn (a hivejack's cash-out, the host's seat count). `dli.s` puts a display list interrupt on the last line of rows
12, 18 and 21, which sets COLPF2 to $C2, back to COLOR2, then to $60 or COLOR2 between lines. One more, below row 23, puts
COLOR2 back: the OS's vertical blank copies the shadow only in its second stage, which it skips when it interrupts a
key or SIO interrupt, and the purple would then show from the top. While the link is down (NO SIGNAL), two more,
on the last blank line above row 0 and at the end of row 0, turn the title bar purple as well. The DLI bits are set only on the
playfield (the title shows its handle there), except that a refusal on the title screen (`THAT HANDLE IS TAKEN`, `TOO
LATE: ...`) turns rows 22-23 purple alone, with the DLIs of rows 21 and 23; the help pages turn DLIs off. A DLI
is an NMI that may interrupt SIO's serial IRQs; at standard speed a byte takes about 930 cycles, far more than the
handler's few dozen (high-speed SIO would need a second look).

### Instructions (help pages)
The pages are `help/atari/screen/HELP01.TXT`, `HELP02.TXT`, and so on: plain ATASCII files, edited in any Atari text editor, and
they are the source (nothing generates them). `make atr` turns each into 960 bytes of screen codes and writes it to
raw sectors on the game disk, which the game reads straight onto the screen with SIO: no DOS, no files, no
conversion on the Atari. So they need `make atr`; the bare `.xex` with no game disk in D1 says
"NO HELP DISK IN D1. ESC:EXIT".
- **A screen of their own:** the pages load into a 960-byte buffer (the 1K-aligned `HELP` segment, so it can't
  cross a 4K boundary), and opening them points the display list's LMS at it; Esc points it back. The game screen
  isn't touched, so mid-game it keeps up underneath (the loop goes on reading the network, and frames draw as
  usual) and is simply there again on Esc. The buffer keeps the last page read, so the pages open again where
  they were left (the price chart stays a key away) without reading the disk. The title says `INSTRUCTIONS, ANY TIME`, and
  mid-game the inverse `WAITING FOR` line ends in an inverse `? HELP`, the moment a player has time to look something up. The pages hide the players and turn the DLIs off while they're up.
  The turn clock keeps running: a new turn's chord is the cue to go back.
- **One file is one screen:** at most 24 lines of at most 40 characters, each ended by EOL ($9B). The page bar
  (`<PREV  ESC:EXIT  NEXT>`, row 24) is part of the page: leave `<PREV` off page 1 and `NEXT>` off the last.
- **Every byte except EOL shows as its glyph:** graphics characters (boxes, lines, the grid's `.` `+`) and inverse
  video appear exactly as typed. `\` shows as the yen sign, because the game's font replaces it.
- **Numbering:** 01, 02... with no gaps. If a heading ends in `n/m`, `n` must be the page number and `m` the page
  count. The game doesn't count pages: `>` reads the next page's sectors and stays put if they aren't a page, so
  adding `HELP09.TXT` needs only `make atr`, not a new `.xex`.
- **`tools/help_disk.py`** checks the pages (too long, too wide, a gap in the numbers, a wrong `n/m`) and writes
  them. Page n is 8 sectors from `HELP_SECTOR + 8*(n-1)` (`HELP_SECTOR=600`, room for 15 pages up to sector 719).
  The first holds the page's last 64 bytes and the signature `SYNACK`, the other 7 its first 896. The sectors are
  marked used in the VTOC so DOS won't write over them; they have no directory entry, so DOS can't list or `TYPE`
  them. `help_disk.py --check help/atari/screen/*.TXT` only checks.
- **A QR code:** `[QR]` on a page becomes a QR code for `QR_URL` (the Makefile's; the Atari manual on the fly.io server by default), its
  top left corner where the marker is, in quarter- and half-block characters, 2x2 modules each (`tools/qr.py`, a
  small encoder, standard library only). It takes the highest error correction level that fits in blanks from
  the marker: the default URL, all upper case, is a version 3-H code, 17x17 characters. The font has no glyph for the
  two diagonal 2x2 patterns, so those cells get one module wrong; of the eight masks and four ways of laying the
  grid, the tool keeps the one with the fewest, and says how many (13 of 841 for the default URL, well within
  level H's correction).
- **Names are typed in:** unlike the rest of the client, these pages don't read `theme.toml`. Renaming a
  syndicate or a term means editing the pages too.

The disk boots with **PicoBoot** (`dir2atr -b PicoBoot406`), the smallest member of the MyPicoDos family: it fits
in the 3 boot sectors and loads the first file on the disk, `SYNACK.XEX`, at standard SIO speed (it has no
high-speed code). There is no DOS, and the game needs none. Measured through FujiNet-PC: 87 sectors in 7.2 s,
against 125 sectors in 11.2 s with DOS 2.5. Like DOS 2.5, it leaves BASIC on unless OPTION is held at power-on; the game runs either way
(`BASIC=1 tools/run-test.sh build/atari/synack.atr` boots with the BASIC ROM on).

`make clean && make atr BOOT=Dos25` builds the DOS 2.5 disk instead (the game as `AUTORUN.SYS`). It takes `DOS.SYS`
from your own DOS 2.5 disk image (`DOS25=path/to/disk.atr`, on the command line or in the environment)
with `atr`.
`tools/run-test.sh build/atari/synack.atr` boots the disk from FujiNet-PC's D1, the way the lobby will.

### Fancy screens
`display.h` is the whole interface. `display_title_screen()` is switched by `FANCY_TITLE`; everything else by
`FANCY_PLAYFIELD` (both in `config.h`, or `make FANCY=1` for both). With a switch at 1, `display_text.c` leaves those
functions out, and a hand-made file (e.g. `src/atari/display_fancy.s`) must provide them. It reads the game state
declared in `src/common/game.h`. Nothing in `common/` changes.

## Wiremon
```sh
tools/run-test.sh build/atari/wiremon.xex
```
Keys: **2–6** start a game with that many seats (host only), **R** resync, **Q** leave. Every frame prints as its
type letter and payload in hex; log lines (`l`) print as text. The top lines count frames, bytes and bad frames. At
connect it asks for 10 full resyncs to push a few KB through the ring. `wiremon-auto.xex` also starts a 3-seat game by
itself and sends `Q` when it is over, or on the first bad frame. So a run can be read from the server's `--trace`
log with no screen: `HEADLESS=1 tools/run-test.sh build/atari/wiremon-auto.xex`.

## Testing without watching
- **`make pc && build/pc/pc-client [host] [port]`** runs `main.c`, `game.c` and `proto.c` compiled with gcc against a
  real server, pressing autoplay keys and printing prompts, logs and what it sends. It is the quickest way to find a
  logic bug, and gdb works on it.
- **`DUMPDIR=dir tools/run-test.sh build/atari/synack-auto.xex`** runs the real Atari build headless. It executes `BRK` when
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
- **Sound:** SIO uses POKEY channels 3 and 4 as its serial clock. `sound.s` writes channels 1-3 from the
  deferred VBI, which the OS skips while SIO runs, so a transfer is never disturbed. Never channel 4: a note left
  there made later SIO reads fail. Each transfer turns the sound off, so `net_sio.s` puts the chord back as soon
  as SIO returns, and skips its once-a-second status check while a chord sounds (a read that FujiNet signals
  with PROCEED still happens at once).
  Chords (B Locrian): a short quiet one for every spike, a new hive, a hivejack, Black ICE, two rising ones when
  it's your turn and two more when it's time to buy, and a series for the null-route. Each fades out; a chord
  still sounding is only cut off by a more important one.

## Apple II (apple2_text)
The same common code with an Apple II platform layer (`src/apple2/`) and a text-mode display: the IIe, IIc and
IIgs (cc65's `apple2` target, so a plain 6502 IIe works). Text mode, because the Apple turns its colour burst off
there: hi-res text artifacts into colour on an NTSC monitor (`tools/apple2_preview.py` shows how), so
`apple2_hgr` and `apple2_dhgr` versions are for later, over the same platform layer.
```sh
make apple2_text                                         # build/apple2_text/synack (AppleSingle, ProDOS type SYS)
make apple2_text-po PRODOS=path/to/ProDOS-2.4-disk AC="java -jar path/to/ac.jar"
                                                         # build/apple2_text/synack.po: a ProDOS disk that boots it
```
The disk holds `PRODOS` and its boot block (both from your own ProDOS disk image: AppleCommander's own boot block
only shows a notice, so `tools/prodos_boot.py` copies the real one), the game as `SYNACK.SYSTEM`, and `SYNACK.HLP`,
the instruction pages. The game is a ProDOS system file at $2000, which ProDOS loads and runs itself, as most
ProDOS games boot. (Through cc65's `loader.system` it hung on a real //c booting from FujiNet over TNFS, after
`Loading SYNACK...`, though it ran in FujiNet Go and from a floppy in MAME's //c.)

- **Network:** fujinet-lib (`lib/fujinet-lib/`), not hand-written SmartPort code: 3.4 KB, against 386 bytes
  for the Atari's raw SIO, for one library every later platform can share. `net_poll` is one status call, then
  one read of what fits in the ring, as on the Atari. Two SmartPort quirks (FujiNet 1.6): a missing AppKey reads
  as a success, 64 zero bytes, so its length is counted to the first NUL; and a failed open answers fine, so
  `net_open` asks for the status at once and reports "not connected" as the failure.
- **The shot clock** runs on FujiNet's clock device, in whole seconds (`ticks_per_sec()` is 1). Counting video
  frames would run slow whenever a SmartPort call blocks past one, and then promise more time than the server
  gives; whole seconds can only err the other way.
- **Screen:** as the Atari's, in the three modes of the primary character set. Hives, the title bar, whose turn
  it is and Black ICE sizes are inverse. The last move **flashes** for 5 seconds, then shows as usual (the game
  loop calls `display_idle()` for that: `DISPLAY_IDLE=1`). Rows 22-23 are normal while others play and
  **inverse while this seat has a decision to make**; inside them, what is normally inverse (an exploit that
  can't run, a syndicate letter) shows normal. A refusal at the foot of the title screen is inverse. There is
  no yen glyph: money shows as `Y3200`. Only the 40 bytes of each row are ever written, never the screen holes
  (the peripheral cards' scratch memory, the FujiNet's included).
- **Keys:** as the Atari's, except **Open-Apple** (alone) for move hints and **Closed-Apple** for sound, where
  the Atari has OPTION and SELECT. Those two aren't latched like the other keys: they count only while held when
  the loop looks, once a pass, so a press counts on the first look that sees it (a 64-pass settle time, then a
  two-look rule, missed presses on a real //c, where a pass waits on SmartPort calls); **Delete** or **←** undoes a letter; **?** opens the instructions, **→** /
  **←** (or **>** / **<**) turn their pages.
- **Instructions:** on text page 2 ($0800), so the game keeps drawing on page 1 underneath and is there again on
  Esc, as on the Atari. The program starts at $2000, leaving page 2 free, and the display turns 80STORE off, or
  the page switch would select auxiliary memory. The pages are `help/apple2/screen/HELP01.TXT`...: plain ASCII,
  24 lines of 40, `{` and `}` around inverse text, checked and turned into 1024-byte screen images by
  `tools/help_apple2.py`. `help.s` reads a page straight onto page 2 with four ProDOS MLI calls (open, set mark,
  read, close), no cc65 file code. The two pages there now only try the mechanism out.
- **Title:** the logo is `src/apple2/title.txt`, any printable ASCII including `|` (normal video only for
  lower case and `|`); `tools/title_text.py` makes the header.

## Sizes (bytes)
| Piece | Code | Notes |
|---|---|---|
| `synack.xex` | 12266 | whole file; loads at $2000, the 1K font at $5400, the 1K player memory at $5800 and the help screen at $5C00 (all bss) |
| `display_text.c` | 3716 | + 1052 rodata (screen words, 364 of title art), 73 bss, 1K font, 1K player memory |
| `game.c` | 2800 | + 382 rodata, 312 bss (the game state) |
| `main.c` | 1149 | + 130 rodata |
| `help.s` | 302 | + 44 rodata, 131 bss, 960 its screen: the instruction pages, read with SIO (was 779 for `help.c` + `cio.s` through DOS) |
| `net_sio.s` | 372 | + 14 rodata, 26 bss, 8 data. 222 before PROCEED/SOUNDR, 318 before AppKeys |
| `proto.c` | 180 | + 26-byte length table, 128-byte `msg[]`; hand assembly could roughly halve it |
| `input.c` | 156 | + 64-byte key table; `ticks_per_sec()` tells PAL (50) from NTSC (60) by GTIA's PAL register |
| `sound.s` | 177 | + 55 rodata (11 chords), 10 bss/data: chords from the deferred VBI |
| `snow.s` | 74 | + 48 rodata (masks), 3 bss: the title's static, also from the deferred VBI |
| `dli.s` | 133 | the color bands' DLI handler and its display list switch (117 before the title's purple) |
| **apple2_text** (2026-10-24) | | |
| `synack` | 15183 | whole AppleSingle file; loads at $2000; `rx_ring` and the ProDOS buffer at $0C00 (bss) |
| `display_text.c` | 3594 | + 821 rodata (screen words, the 48-byte row table, 98 of title art), 28 bss/data |
| fujinet-lib | 3404 | open, status, read, write, close, AppKey read, clock; the bus layer alone is 1520 |
| cc65 runtime | 1585 | startup, memcpy/memset, the C stack helpers, ctype (pulled in by fujinet-lib's clock) |
| `net_fn.c` | 473 | the platform.h network calls on fujinet-lib; `net_open` checks status, as a failed open still answers fine |
| `input.c` | 239 | keys, the Apple keys, and `ticks()` from FujiNet's clock |
| `help.s` | 184 | + 68 rodata/data, 1K ProDOS buffer; the pages are read onto text page 2 |
| `sound.s` | 75 | + 42 rodata (7 chords of up to 3 notes) |
| `main.c` | 1152 | the Atari's plus 3 for the `display_idle()` call |
| Bake-off (2026-10-18), network added on top of the same test program | | |
| raw SIO | 228 | |
| fujinet-lib 5.0.0 | 1712 | open/status/read_nb/write/close plus the cc65 runtime and memcpy/memmove it pulls in; the test and the library have since been removed from the repo |
| `wiremon.xex` | 2982 | whole file, including cc65's Atari startup code |

## Tested (atari800 7.2.1 + FujiNet-PC 1.6 + server, NetSIO)
- **Game client:** the autoplay build played for four minutes as host with two bots. It sent 75 answers covering
  every prompt kind, and the server rejected none. 156 screen dumps checked by eye. The PC build played a whole game
  to the end (47 answers, 0 rejected).
- **PAL:** atari800 `-pal` plays the same. Its countdowns run at 50 frames a second, so they reach 0 when the
  server's clock does (measured 4.8 per 240 frames, against 4 on NTSC); chords decay 20% slower, harmlessly.
- **Connect and hello:** wiremon gets its seat.
- **Ring stress:** 9 back-to-back resyncs, then a whole 3-seat game, with every frame parsed and 0 bad frames.
- **PROCEED:** over a one-minute game there were 163 status calls and 145 reads. Polling blindly at 15 Hz would have
  made about 840 status calls.
- **Reconnect:** a relay cut the TCP connection mid-game. Wiremon reconnected 2.7 s later with its token, got its
  seat back (the server only allows that mid-game for a valid token), and followed the game to the end.
