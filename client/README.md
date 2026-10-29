# SYN-ACK WIRE client

The 8-bit side. The server enforces every rule; the client draws what it is told and answers prompts
(`../docs/protocol.md`). Common code is C written for cc65's code generator; platform code lives in `src/<platform>/`.

| Path | What |
|---|---|
| `src/platform.h` | The contract each platform implements: network calls, the receive ring, keys, a tick counter |
| `src/display.h` | Everything the game shows. One text version per platform; hand-made screens can replace it |
| `src/config.h` | Build choices: `FANCY_TITLE`, `FANCY_PLAYFIELD`, `HINTS`, server `URL`, `LOBBY_KEY`, `TABLE`, `DISPLAY_IDLE`, `TITLE_IDLE` |
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
| `src/msdos/display_text.c` | MS-DOS text screens: the 40×24 layout centred in 80×25 text, monochrome attributes only (normal, reverse, blink); the instruction pages from `SYNACK.HLP` |
| `src/msdos/net_fn.c` | Network calls for MS-DOS: FujiNet RS-232 through fujinet-lib, which calls `FUJINET.SYS`'s INT F5h |
| `src/msdos/input.c` | Keys from the BIOS (F1 help, F2 hints, F3 sound, F4 40/80 columns, F5 colour) and `ticks()` from the BIOS tick, 18 a second |
| `src/msdos/sound.c` | Chords on an AdLib (OPL2 at 388h, also a Sound Blaster's) when one answers, else on the PC speaker |
| `src/msdos/title_art.h` | The Atari's title logo in code page 437's box characters, **generated** by `tools/title_art.py d` |
| `src/coco/text.c` | CoCo 1/2: the 40×24 cells drawn on the PMODE 4 screen, 4 cells at a time in 6809 code, and the inverse margins |
| `src/coco/font.txt` | The CoCo's 6×8 font as `#`/`.` pictures; `font.h` is **generated** by `tools/font_coco.py` |
| `src/coco/display_text.c` | CoCo text screens: the Apple's layout in `txt_cells`, normal/inverse, the last move flashed by `display_idle()` |
| `src/coco/net_fn.c` | Network calls for the CoCo: FujiNet over DriveWire (serial port or Becker port), through fujinet-lib |
| `src/coco/input.c` | Keys from Color BASIC's keyboard scan (CLEAR hints, SHIFT+CLEAR sound, up arrow colours) and `ticks()` from its 60 Hz TIMER |
| `src/coco/help.c` | The instruction pages: PMODE 4 screen images from `SYNACK.HLP`, read straight onto the screen through DSKCON |
| `src/coco/sound.c` | Chords as short notes from the 6-bit DAC |
| `src/coco/snow.c` | The CoCo title's static: random pixels in the logo's letters, written to the screen in 6809 code |
| `src/coco/title_art.h`, `title_snow.h` | The Atari's title art in the font's box glyphs, and which pixels get the static, **generated** by `tools/title_art.py k` and `m` |
| `src/nes/crt0.s`, `game.cfg` | NES: the iNES header (NROM-256, CHR-RAM, the cartridge's WRAM), start-up, the NMI, the FujiNet claim at $FFF0 |
| `src/nes/ppu.s` | NES: the NMI's share of drawing: the rows written since the last frame, the attributes, the title's static, in the vertical blank |
| `src/nes/text.c`, `text.h` | NES: the tiles (from `font.txt`), palettes and blank name tables at start-up; the first screen shown whole |
| `src/nes/font.txt` | The NES's 8x8 font (the NES CONFIG's) and the logo's box tiles; `font.h` is **generated** by `tools/font_nes.py` |
| `src/nes/display_text.c` | NES text screens: 30 columns of tiles, colour bands, the pad's cursor as sprites |
| `src/nes/input.c`, `pad.s` | NES: the pad (latched by the NMI) turned into keys, with a cursor on the open prompt's answers; the title's letter dial; `ticks()` |
| `src/nes/net_fn.c` | Network calls for the NES: the FujiNet NES cartridge's mailbox, through fujinet-lib |
| `src/nes/help.c` | NES: the instruction pages, from the ROM onto the second name table |
| `src/nes/sound.s`, `ring.s` | NES: the Atari's chords on the APU, from the NMI; `rx_ring` at $0300 |
| `src/nes/title_art.h` | The Atari's title art in the box tiles, its letters the static's tiles, **generated** by `tools/title_art.py n` |
| `lib/fujinet-lib/` | fujinet-lib, a pinned copy (version and checksum in its README) |
| `test/wiremon.c` | Protocol monitor; proves the network layer before any game screens exist |
| `test/autoplay.c` | Test build that presses keys itself and BRKs for screen dumps |
| `test/pc_harness.c` | The common code on a PC with gcc: sockets, printed display, autoplay keys |
| `test/dos_dump.c` | The MS-DOS autoplay build's screen dumps: the 40×24 game area to `SCRnnn.BIN` |
| `test/coco_text_pc.c` | `text.c`'s calls on a PC (`make pc-coco`): only the cells, which the dumps save |
| `test/coco_dump.c` | The CoCo autoplay build's dump hook: an empty `pc_dump` a debugger stops on |
| `test/coco/` | The CoCo screen test (`screentest.c`): the font, both colour sets, the title's static and the drawing speed, on a PMODE 4 screen |
| `test/nes_text_pc.c` | What `src/nes/display_text.c` needs on a PC (`make pc-nes`): the cells, attributes and sprites, which the dumps save |
| `test/nes_dump.c` | The NES autoplay build's dump hook: it counts dumps for `tools/nes.lua`, which saves the screen |
| `test/dos_glitch.c` | A flaky serial line for testing MS-DOS builds: with `-Dnetwork_status=glitch_status`, the status call fails 1 second in 20 |
| `tools/run-test.sh` | Runs an `.xex` on atari800 with FujiNet-PC (NetSIO) |
| `tools/run-dos.sh` | Plays the MS-DOS client in a DOSBox-X window, booting your PC-DOS disk with `FUJINET.SYS`, FujiNet-PC (RS-232) on COM1 |
| `tools/help_msdos.py` | Checks the MS-DOS instruction pages and writes them as screen images (`SYNACK.HLP`) |
| `tools/screen2txt.py` | Shows a screen dump as text |
| `tools/render_screen.py` | Draws a screen dump as the screen looks, for the manual's captures |
| `tools/help_apple2.py` | Checks the Apple instruction pages and writes them as text-page images (`SYNACK.HLP`) |
| `tools/prodos_boot.py`, `title_text.py` | The Apple disk's ProDOS boot block; the Apple title logo as a header |
| `tools/apple2_preview.py` | Draws a screen dump as Apple II hi-res would show it on a colour monitor, three ways |
| `tools/capture.sh` | Captures the manual's staged screens (`docs/scenarios.toml`, `server/stage.py`) on atari800; `PLATFORM=apple2`: with `pc-apple2`, and likewise `msdos`, `coco`; `PLATFORM=nes`: the autoplay image in MAME |
| `tools/render_apple2.py` | Draws an Apple II screen dump in the IIe/IIc character set, or shows it as text (`--text`) |
| `tools/help_coco.py` | Checks the CoCo instruction pages and writes them as PMODE 4 screen images (`SYNACK.HLP`), the QR code in pixels |
| `tools/xroar_dump.py` | Saves the CoCo autoplay build's screens from xroar through its GDB port |
| `tools/render_coco.py` | Draws a CoCo screen dump (6144 bytes of PMODE 4, or `pc-coco`'s 960 cells, a title's logo with its static) as the screen looks |
| `tools/run-nes.sh`, `nes.lua` | Runs the NES image in MAME (`-nes_slot fujinet`) with FujiNet-PC: the pad on a schedule or a pad bot, the screen as text, snapshots, the autoplay build's dumps |
| `tools/help_nes.py` | Checks the NES instruction pages and packs them for the ROM (`help_pages.h`), the QR code in quarter-block tiles |
| `tools/render_nes.py` | Draws an NES dump (`pc-nes`'s or the autoplay build's) or an instruction page as the screen looks, or as text |

## Build
```sh
make                                     # build/atari/synack.xex (the game), build/atari/wiremon.xex, build/atari/wiremon-auto.xex
make URL=N:TCP://192.168.1.20:6502/      # aim them at another server (default: syn-ack-wire.fly.dev:6502)
make atrtest                             # build/atari/local/syn-ack-wire-atari.atr: the disk, dialing localhost:6502
make LOBBY_KEY=0                         # ignore the game lobby's AppKeys (see below)
make FANCY=1                             # hand-made screens instead of text (once they exist)
make HINTS=0                             # move hints start off (the player can still turn them on)
make test                                # build/atari/synack-auto.xex: plays by itself, for the emulator
make atr                                 # build/atari/syn-ack-wire-atari.atr: the game disk (PicoBoot, game, help pages)
make pc                                  # build/pc/pc-client: the common code on this PC
make pc-apple2                           # build/pc/pc-apple2: the same with the Apple II's display, for captures
make msdos WATCOM=dir                    # build/msdos/SYNACK.EXE and SYNACK.HLP: the MS-DOS client (8088 and up)
make msdos-img WATCOM=dir                # build/msdos/syn-ack-wire-msdos.img: the release floppy (360 KB)
make msdos-test WATCOM=dir               # build/msdos/SYNAUTO.EXE: plays by itself, saving its screen
make msdos-wiremon WATCOM=dir            # build/msdos/WIREMON.EXE, WMAUTO.EXE: the protocol monitor for DOS
make pc-msdos                            # build/pc/pc-msdos: the common code with the MS-DOS display, for captures
make nes                                 # build/nes/syn-ack-wire-nes.nes: the NES client (the FujiNet NES cartridge)
make pc-nes                              # build/pc/pc-nes: the common code with the NES display, for captures
```
Toolchain: cc65 master, `V2.19 - Git 8d86ca7` (2026-10-18). Size numbers below are from that build. MS-DOS: Open
Watcom v2 (`2.0 beta Sep 22 2025`), as the FujiNet team builds its DOS programs; `WATCOM` is its folder (default
`~/openwatcom`). The game needs `FUJINET.SYS` (fujinet-rs232) loaded in CONFIG.SYS, from the same firmware version as
the FujiNet.

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
...                                        the last move: blinks for 5 seconds
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
(`BASIC=1 tools/run-test.sh build/atari/syn-ack-wire-atari.atr` boots with the BASIC ROM on).

`make clean && make atr BOOT=Dos25` builds the DOS 2.5 disk instead (the game as `AUTORUN.SYS`). It takes `DOS.SYS`
from your own DOS 2.5 disk image (`DOS25=path/to/disk.atr`, on the command line or in the environment)
with `atr`.
`tools/run-test.sh build/atari/syn-ack-wire-atari.atr` boots the disk from FujiNet-PC's D1, the way the lobby will.

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
- **`make pc-apple2`** builds the same with the Apple II's real display, `src/apple2/display_text.c`, drawing into
  an array instead of text page 1 (`PC_SCREEN`; the Apple build is unchanged). Each autoplay dump saves the
  array to `DUMPDIR/scrNNN.bin`, in the screen's own order. `PLATFORM=apple2 tools/capture.sh` uses it for the
  Apple II manual's captures, with no emulator: the staged scenarios, and `title` and `lobby-join` from a plain
  server with two test clients waiting in its lobby. `tools/render_apple2.py --text` shows a dump as text.
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
                                                         # build/apple2_text/syn-ack-wire-apple2.po: a boot disk
```
The disk holds `PRODOS` and its boot block (both from your own ProDOS disk image: AppleCommander's own boot block
only shows a notice, so `tools/prodos_boot.py` copies the real one), the game as `SYNACK.SYSTEM`, and `SYNACK.HLP`,
the instruction pages. The game is a ProDOS system file at $2000, which ProDOS loads and runs itself, as most
ProDOS games boot. (Through cc65's `loader.system` it hung on a real //c booting from FujiNet over TNFS, after
`Loading SYNACK...`, though it ran in FujiNet Go and from a floppy in MAME's //c.)

- **Network:** fujinet-lib (`lib/fujinet-lib/`), not hand-written SmartPort code: 3.4 KB, against 386 bytes
  for the Atari's raw SIO, for one library every later platform can share. `net_poll` is one status call, then
  one read of what fits in the ring, as on the Atari. SmartPort has no signal for "data waiting" (the
  Atari's PROCEED), so the Apple asks, but paced: about 10 status calls a second while nothing comes (every
  call while bytes arrive, and at once after sending), and the clock about 10 reads a second; the calls in
  between wait a moment in `pause` (sound.s), the Apple having no timer. Unpaced, the loop asked FujiNet
  continuously and its activity light never rested. Two SmartPort quirks (FujiNet 1.6): a missing AppKey reads
  as a success, 64 zero bytes, so its length is counted to the first NUL; and a failed open answers fine, so
  `net_open` asks for the status at once and reports "not connected" as the failure.
- **The shot clock** runs on FujiNet's clock device, in whole seconds (`ticks_per_sec()` is 1). Counting video
  frames would run slow whenever a SmartPort call blocks past one, and then promise more time than the server
  gives; whole seconds can only err the other way. The count can lag a reading by a tenth of a second, so the
  countdown shows one second short (59 for 60): the clock reaches zero with the server's.
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
  read, close), no cc65 file code. A page with `[QR]` on it is a lo-res page in mixed mode: the tool draws a QR
  code for `A2_QR_URL` (the Makefile's; the Apple manual by the fly.io short link) in the 40 x 40 blocks of
  graphics, at the highest error correction that fits (3-H for the default), lines 21-24 stay text, and a flag in
  the page's first screen hole tells `help.s` to show it in lo-res; closing always goes back to text.
- **Title:** the logo is `src/apple2/title.txt`, any printable ASCII including `|` (normal video only for
  lower case and `|`), in rows 0-13; `tools/title_text.py` makes the header. Under it, low on the screen as on
  the Atari: REGION, STATUS and HANDLE (rows 15-17), then the keys, OPEN-APPLE, CLOSED-APPLE and ? (19-21).

## MS-DOS (msdos)
The same common code with an MS-DOS platform layer (`src/msdos/`), for an IBM PC and up: plain 8086 code, so an
8088 at 4.77 MHz, DOS 2 or later, any display adapter. Built with Open Watcom v2, as the FujiNet team's DOS
programs are (`make msdos WATCOM=dir`). FujiNet is an RS-232 one, reached through `FUJINET.SYS` (fujinet-rs232),
which CONFIG.SYS loads and which gives programs INT F5h; fujinet-lib's MS-DOS build calls it.
```sh
make msdos WATCOM=dir                       # build/msdos/SYNACK.EXE and SYNACK.HLP
make msdos-img WATCOM=dir                   # build/msdos/syn-ack-wire-msdos.img: both on a 360 KB floppy
tools/run-dos.sh build/msdos/SYNACK.EXE     # play it in DOSBox-X; EMU=mame for an IBM PC 5150 (CARD=mda, ADLIB=1)
```
- **Network:** as the Apple's (`net_fn.c` follows it), through fujinet-lib: 1 KB. There is no "data waiting"
  signal, so while nothing comes `net_poll` asks once a BIOS tick (18 a second); while bytes arrive, every call.
  An exchange with FujiNet that fails on the serial line is let go, and asked again next tick; only about 3
  seconds of them in a row (or FujiNet saying the connection closed) count as a dropped line. Giving up on the
  first failure redialled on a blip of under a second, and the server's bot took the seat's move meanwhile.
  Tested with `test/dos_glitch.c` (1 s of failures every 20 s): an autoplay game in DOSBox-X redialled 7 times
  in 2.5 minutes before, and kept one connection for the whole game after.
- **The shot clock** is the BIOS tick (`ticks_per_sec()` 18), which the timer interrupt keeps counting while a
  serial call waits; the countdown shows one second short, as on the Apple.
- **Screen:** 80x25 text, the 40x24 layout in columns 20-59, or 40x25 text for a TV or a composite monitor,
  where 80 columns blur; written straight to video memory. A monochrome adapter (BIOS mode 7, B000) is 80 only.
  On anything else (B800) `SYNACK=40` or `SYNACK=80` in the environment picks the width, else a menu at start
  does, asked in 40 columns (1 or 2), and **F4** on the title screen swaps them. `SYNACK=` is read from the
  PSP's environment, as Watcom's `getenv` would add 800 bytes.
- **Colour:** on a colour adapter **F5** turns green phosphor on or off, anywhere but the instructions (the
  playfield is redrawn, its log lines kept). It starts off, menu included: on a grey LCD panel the colours
  were hard to read (a monochrome LCD laptop, 2026-10-25). The drawing code still writes the
  monochrome attributes, plus `HIVE` for a syndicate's letter; `attr()` turns them into colours by where the
  cell is: light green on black, black on green under the Grid's nodes and the seats' rows, not their
  labels and headers (as the Atari's panels), reverse black on light green, the title bar white on green, a question for this seat white on
  magenta, and each syndicate its own background with a bright letter. Bright backgrounds need the hardware
  blink off (INT 10h AX=1003h on EGA/VGA, port 3D8 on CGA and Tandy, the gate array on a PCjr), so in colour
  the last move blinks in software. BIOS modes 1 and 3 in colour; 0 and 2 without, text with a composite
  monitor's colour burst off. Without colour, monochrome attributes only, so it reads alike on MDA,
  Hercules, CGA, EGA, VGA and the grey LCD panels of old laptops: normal, reverse where the Apple has inverse,
  and reverse blinking for the last move (5 seconds, `DISPLAY_IDLE=1`). Code page 437 has a yen sign. The 20
  columns either side, in 80, hold circuit-board art (`src/msdos/margins.txt`, UTF-8, `tools/margins_msdos.py` makes the
  header): decoration only, normal grey (dark grey vanished on a grey LCD), green in colour. The title logo is the
  Atari's art in box characters (`tools/title_art.py d`).
- **Keys:** **F1** or **?** the instructions, **F2** move hints, **F3** sound (switched where the keys are read),
  **F4** 40 or 80 columns (the title screen only; on a PCjr's keyboard, Fn and 4), **F5** colour, **Esc** on
  the title screen quits to DOS after a Y (`QUIT_TO_OS=1`: the sound silenced, the connection closed, the
  video mode DOS had put back),
  **Backspace** or **Delete** undoes a letter, **Enter** confirms; the prompts say BACKSPACE and ENTER.
- **Sound:** an AdLib, or a Sound Blaster's FM part, when one answers at 388h: the Atari's chords on three FM
  voices, started and stopped by the BIOS tick (`snd_poll()`, with every key check), so nothing waits. Else the
  PC speaker: the Apple's chords, a note at a time, timed by a delay loop measured against the tick at start.
- **Instructions:** `SYNACK.HLP` beside the program, from `help/msdos/screen/HELP*.TXT` (the Apple's format, with
  `\` for the yen sign) by `tools/help_msdos.py`: 1920-byte pages of character and attribute, the QR code drawn
  in half blocks. A monochrome adapter has one text page, so while the pages are up the game draws into a copy of
  the screen in memory, copied back when they close.
- **Tested** (2026-10-24, FujiNet-PC built with `FUJINET_TARGET=RS232`, its BoIP port on COM1, PC-DOS 3.30):
  in DOSBox-X the autoplay build played two whole games and the protocol monitor ran 8 minutes, 632 frames, 0
  bad. On MAME's IBM PC 5150 (8088 at 4.77 MHz, 115200 bps) with CGA and an AdLib it played two whole games in
  13 minutes on one connection; with MDA and the speaker, one whole game. No errors in either log.
  On real hardware (2026-10-25), a 386 with a FujiNet RS-232 on its serial port: run by hand from the mounted disk
  (the title, the art, the sound, a connection that held), and from the FujiNet lobby, which mounts the floppy
  and runs its `AUTOEXEC.BAT`. With an SD card in the FujiNet, the table picked and the lobby name reached the
  game; without one, FujiNet keeps no app keys and the game dials its built-in server. The lobby finds
  `COMMAND.COM` by `PATH`, so an empty `PATH` leaves it at a blank screen.

## CoCo 1/2 (coco)
The same common code with a CoCo 1/2 platform layer (`src/coco/`), built with CMOC and lwtools. The CoCo's
6847 has no 40-column text (its text mode is 32×16, upper case), so the client draws its own: PMODE 4, 256×192 in
two colours, 40 columns of 6×8 cells from pixel 8, with 8 pixels either side.
```sh
make coco                                    # build/coco/synack.bin, and synack.hlp, the instruction pages
make coco-dsk DECB=path/to/toolshed/decb     # build/coco/syn-ack-wire-coco.dsk: both, and AUTOEXEC.BAS
make pc-coco && build/pc/pc-coco host port   # the display on this PC against a server, saving its cells
make coco-auto URL=N:TCP://host:port/ LOBBY_KEY=0 DECB=...   # build/coco/auto/syn-ack-wire-coco.dsk: plays by itself
make -C test/coco DECB=...                   # build/coco/screentest.dsk: the screen test (LOADM"SCRTEST":EXEC)
```
The disk is `syn-ack-wire-coco.dsk`, as the other platforms' are named, and the program on it `SYNACK.BIN`. The
FujiNet lobby boots a CoCo game by `RUNM` of the mounted disk's own name (fujinet-lobby,
`clients/src/coco/platform.c`), and Disk BASIC takes at most 8 letters: `RUNM"SYN-ACK-WIRE-COCO"` is `?FN ERROR`
(tried in MAME), so for the lobby the disk must be served under a name of 8 at most: the server's image serves it
as `synack.dsk` too, and the lobby runs `RUNM"SYNACK"` (server/README.md, "Deploying to fly.io"). Booted from the FujiNet
without the lobby, HDB-DOS runs `AUTOEXEC.BAS` itself (seen in xroar); from plain Disk BASIC, `RUN"AUTOEXEC"`. It runs
`PCLEAR1` first; with BASIC's 4 graphics pages its program starts at $2601, where LOADM would write over the
running line. fujinet-lib reaches FujiNet through HDB-DOS's DriveWire routines, so the game needs the HDB-DOS a
FujiNet boots; under plain Disk BASIC it falls back to BASIC at once.

- **Screen** (`text.c`): `txt_cells[]` holds the 40×24 cells (ASCII, bit 7 inverse); the display writes it,
  marks the rows it touched, and the outermost display call draws those rows as it returns, so the playfield,
  built from the others, is drawn once. A row is drawn 4 cells at a time, 3 whole bytes a pixel line, in 6809
  code: about 25,500 cycles (28 ms) a row with its margins, a full screen in 0.7 s (42/60 s, MAME's CoCo 2).
  Cell code in C took 1.5 s a screen. Every pixel is written once, as it ends up, so a row drawn again over
  itself (the clock, the flashing last move) doesn't flicker, and nothing is cleared first. Drawing the margins
  in a second pass was 20% faster, but they blinked each time a row was drawn again. The screen is at $0E00-$25FF, where HDB-DOS leaves room (the lobby's is
  there too), and the program loads at $2600; with its variables it ends at $7728, under the $7B00 a 32K
  CoCo leaves below its stack and Disk BASIC's strings.
- **Font** (`font.txt`, 95 glyphs): 5×7 in a 6×8 cell, the gap right and below; g j p q y use the gap row for
  their tails. The client's copy stops at $5F (the server sends nothing above it, and lower case is only in the
  help pages, drawn when the disk is built) and has the title's 11 box glyphs at $60-$6A instead. Vertical strokes are on even pixels, and cells start on even pixels, so on an NTSC TV every
  one-pixel stroke takes the same artifact tint, not some blue and some orange (`font_coco.py` lists the
  exceptions: only `#`). In colour set 0, green and black, MAME shows the tint as a darker green;
  alternating pixels (the middles of m, w, M, N, W) smear.
- **Inverse:** the cell's 6×8 the other way round. A glyph leaves its gap right and below, so an inverse
  letter's left and top strokes would run into the dark around it: a run of inverse cells also takes the pixel
  column left of it and, if shorter than 4 cells (a hive's letter), the line above, from its neighbours' gaps.
  Longer runs (the bars, the turn's name) go without the line above, which would sit on the row above's text.
  The margins are worked into each cell's flip before its row is drawn: the gap column of the cell left of an
  inverse one, and on line 7 (the gap row) the cells over a short run, are turned to the inverse background.
- **Colours:** green on black, or black on green (**up arrow**, anywhere: everything is in `txt_cells`, so it
  is drawn again). The VDG's colour set 0 both ways; the border stays green. About 60 bytes.
- **Title:** the Atari's logo in box glyphs (`tools/title_art.py k`), lines 2 pixels wide through each cell's
  columns 2-3 and lines 3-4, out to its edges: an even pair of pixels is a whole NTSC colour cycle, so the lines
  show plain green, where 1 pixel would take an artifact tint (dark brown at some start-up phases). Its
  letters fill with static as the Atari's do (`snow.c`, from `display_title_idle()`,
  `TITLE_IDLE=1`): 30% of the pixels lit, kept 2 pixels clear of the lines, which would otherwise drown in a
  static of their own colour. `title_art.py m` works out the pixels (the Atari's flood fill, then down to the
  pixel) and writes each line as a record of runs: bytes masked, skipped or all static, 35 different lines in
  all. The random bytes come from a 256-byte table, read from a new start (an 8-bit LFSR) on each line. A call
  does every other line, about 36,000 cycles (MAME's CoCo 2), so the static changes 12 times a second and keys
  and the network wait at most 40 ms. Not while the instructions are up. On an NTSC TV the static's single
  pixels shimmer in artifact colour, as snow does. At start-up the screen goes black (BASIC's text screen
  filled with dark blocks) while the title and two passes of static are drawn out of sight, then shows them
  whole: the VDG only switches to PMODE 4 then (`txt_show`). Later screens are drawn in view.
- **The last move flashes** for 5 seconds: inverse and back every 16 ticks, its row drawn each time, from
  `display_idle()` (`DISPLAY_IDLE=1`).
- **Network:** as the Apple's, through fujinet-lib (1.7 KB): one status call, then one read of what fits in the
  ring. A status call is a DriveWire exchange with interrupts off, so while nothing comes `net_poll` asks about
  10 times a second, by the 60 Hz clock; while bytes arrive, and after sending, every call. On the serial port
  FujiNet also signals waiting bytes on carrier detect (its firmware, `lib/bus/drivewire/drivewire.cpp`), which
  sets PIA1's CA1 flag ($FF21 bit 7): `net_poll` asks at once when it is set. It is only a hint (a Becker port
  has no such line), and not yet tried on a real CoCo.
- **The shot clock** is Color BASIC's TIMER ($0112, 60 a second); fujinet-lib holds interrupts off while it talks
  to FujiNet, so it runs a little slow, and the countdown shows one second short, as on the Apple.
- **Keys:** Color BASIC's keyboard scan. **?** the instructions, **CLEAR** move hints, **SHIFT+CLEAR** sound
  (switched where the keys are read), **up arrow** colours, **BREAK** for Esc, **left arrow** deletes, **ENTER**
  confirms, **right arrow** and **left arrow** turn the instruction pages. The prompts name them as the CoCo does:
  `TYPE A HANDLE, THEN ENTER`, `LETTERS, <-, ENTER TO BUY`.
- **Manual:** `docs/manual/coco/`, its captures from `PLATFORM=coco tools/capture.sh` (`pc-coco`'s cells, drawn by
  `render_coco.py` in the client's font, the title with a still of its static). Help page 10's QR code links to
  it by the fly.io short link (`COCO_QR_URL`).
- **Sound:** the 6-bit DAC ($FF20) through the analog switch to the TV: the Apple's chords, a note at a time,
  timed by a delay loop, pitches scaled to the 0.89 MHz clock. $FF20's bit 1 is the serial line to FujiNet, so
  every write keeps bits 1-0 as they were.
- **Instructions:** `SYNACK.HLP`, from `help/coco/screen/HELP*.TXT` (the MS-DOS pages' format and text, with
  the CoCo's keys) by `tools/help_coco.py`: each page a 6144-byte PMODE 4 screen image, drawn with the same
  font and margins as `text.c` (checked against MAME's dump: the same bytes), and the QR code in pixels, 4 to a
  module. `help.c` reads the directory entry and the FAT once, then a page's 24 sectors straight onto the
  screen through DSKCON (HDB-DOS sends it to FujiNet): no file code, no buffer but the 68-byte FAT. The game
  goes on writing `txt_cells` underneath, and closing draws it.
- **Tested** (2026-10-27): the screen test in MAME's CoCo 2 (timings, both colours, artifacting on and off); the
  display with `pc-coco` against a scratch server, a whole game and the next. With FujiNet, in xroar 1.13.0.dev
  (a CoCo 2B, HDB-DOS for the Becker port) and FujiNet-PC built `FUJINET_TARGET=COCO`, the Becker port on 65507,
  the disk as its `autorun.dsk` (and its `AUTOLOAD.DWL` taken away, so HDB-DOS stops at BASIC):
  `make coco-auto` played whole games against a scratch server (hivejacks, infiltrations, game over, the next
  game), rejoined after xroar restarted, and read its help pages from the disk through FujiNet.
  `tools/xroar_dump.py` saved the screens through xroar's GDB port (`-gdb -gdb-port N`), stopping at each
  `pc_dump`, or every so often with `--poll`. From the public lobby (2026-10-28, FujiNet Go CoCo 0.4.1, a CoCo 2):
  the lobby's `RUNM"SYNACK"` of `synack.dsk` started the game, and the title showed the server picked (its AppKey,
  through FujiNet's SD). Not yet tried: sound, carrier detect, a real CoCo. MAME 0.289's Becker port stops it at
  start ("Attempt to register save state entry after state
  registration is closed"), so MAME is for the screen test only.

## NES (nes)
The same common code with an NES platform layer (`src/nes/`), for the FujiNet NES cartridge, which serves the
console a mailbox at $5000-$57FF and 8K of WRAM at $6000. Built with cc65 (`-t nes`), our own start-up and link
config: NROM-256, 32K of PRG, 8K of CHR-RAM, vertical mirroring.
```sh
make nes                                     # build/nes/syn-ack-wire-nes.nes
make nestest                                 # build/nes/local/...: dialing LOCAL_URL
make nes-auto URL=N:TCP://host:port/ LOBBY_KEY=0   # build/nes/auto/...: plays by itself
make pc-nes && build/pc/pc-nes host port     # the display on this PC against a server, saving its screen
tools/run-nes.sh build/nes/syn-ack-wire-nes.nes   # play it in MAME with FujiNet-PC
```
The image is `syn-ack-wire-nes.nes`, as the other platforms' are named. The FujiNet game lobby mounts a `.nes` over
TNFS and boots it, having written the server's address to the AppKey (fujinet-lobby, branch `nes`, `st_boot.c`), so
the client's AppKey dial is the other platforms'. `nes-romstamp.py` (in the pinned fujinet-lib) writes `FUJI` at
$FFF0, without which the cartridge shuts its mailbox off once the game boots, and fails the build if any code would
write the mailbox's write-only pages ($5500-$57FF) with a read-modify-write instruction (the 6502 writes the old
value first, a spurious event).

- **Screen:** 8x8 tiles, so 32 columns where the others have 40: the screens are laid out anew in columns 1-30 and
  rows 1-28 of the name table, inside a TV's overscan. The Grid at the left (rows 3-11), beside it the hives panel
  with the syndicates' `short` names (`SYN_SHORTS`), the seats (14-19: handle, cash in 6 columns, 7 holdings of 2),
  the log (20-25: each line on 1 row, or wrapped at a word onto 2), then the info row and the question with the
  clock (26-27). No game name in the title bar: the region and the link take it. game.c's own refusals are worded
  for the question's 26 columns (`NARROW_SAYS`, `config.h`).
- **Drawing:** the display writes `txt_cells` (WRAM) and marks the rows it changed; the NMI copies at most 3 rows a
  frame in the vertical blank (`ppu.s`), so a whole screen takes 10 frames, in view, and a row drawn over itself
  doesn't flicker. Measured in MAME (a write tap on the NMI's first store and on PPUMASK, two minutes of autoplay):
  at most 2146 cycles to the last PPU write, inside the 2273 of the blank; 4 rows took 2568. The cells start on a
  page ($6000), so no row's `(ptr),y` crosses one. Only the first title is drawn with the display off.
- **Tiles and colour:** made in CHR-RAM at start-up (`text.c`) from `font.txt`, the NES CONFIG's 7x7 Family BASIC
  style: each glyph colour 3 on colour 2 (plane 1 all set), and its inverse at code | $80. So each palette gives
  its text its own background: green on black; the Grid and the seats on a dark green panel, as on the Atari; cash
  and every other syndicate's crypto in yellow there, so two-digit holdings side by side read apart; the bottom
  two rows purple while this seat has a decision to make, the title bar too while the link is down (NO SIGNAL).
  Attribute blocks are 2x2 tiles, which is why the bands and the holdings sit on even rows and columns.
- **Title:** the Atari's logo in box tiles (`tools/title_art.py n`), its letters' strokes in 4 static tiles that the
  NMI rewrites every frame, 64 random bytes from a different place in a table each time (the static pauses while
  rows are waiting, as the Atari's pauses for SIO). Under it the region, the status (no label: 28 columns of it),
  the handle and the buttons.
- **Pad:** the NMI reads it every frame and keeps the presses in an 8-event ring (`pad.s`), so none is lost while the
  program waits on the cartridge. `input.c` turns them into the keys `platform.h` promises, with a cursor (`pick`)
  on the answers the open prompt can take, from `game.h`'s state: **left/right** (or up/down) move it over the
  kit's exploits (an underline, and a box on the node it would spike), the offered syndicates (F, K) or, while
  buying, the panel's syndicates with a price (an arrow); **up/down** count a hivejack's cash-out and swap (written
  into `edit[]` as typing would; the swap by 2, up to what is held and twice the Darkpool's supply) and the host's
  seats. **A** sends the answer, or YES; **B** is Delete, or NO; **START** is Return. **SELECT**, on its own as it is
  let go, opens the instructions; **SELECT+A** move hints, **SELECT+B** sound, **SELECT+START** Esc (log off). On
  the title the d-pad is a dial: up/down step the letter it is on (A-Z, 0-9, then the signs a handle can have),
  left/right move along the handle, B deletes the letter. A direction held repeats.
- **Network:** as the CoCo's, through fujinet-lib (1.8 KB, built from fujinet-lib-experimental's `add-nes`, pinned).
  No "data waiting" signal: status about 10 times a second while nothing comes, every pass while bytes arrive. The
  read is `network_bus_read`, for no more than the status said (fujinet-lib's `network_read` asks for the status
  again first, and spins while nothing is there). fujinet-lib's wait is set to 95 s, past the cartridge's 90 s
  for a network call: one that gave up sooner would leave its reply to be taken for the next call's. At start it
  waits up to 15 s for the cartridge's link to the FujiNet, and the first call gets a second try.
- **The shot clock** is the NMI's frame count. fujinet-lib waits on the cartridge in a counted loop with interrupts
  on, so it keeps time through a call. `ticks_per_sec()` is measured at start-up, the cycles between two NMIs: 60 on
  NTSC, 50 on PAL or a Dendy (MAME's `nes` and `nespal`: 60 and 50).
- **Sound:** the Atari's chords (`sound.s`): the first note on the triangle an octave down, the others on the two
  pulses, faded by the NMI, which stops the triangle once they are quiet. NTSC periods; PAL plays about a semitone
  flat.
- **Instructions:** `help/nes/screen/HELP*.TXT`, 28 lines of 30 (`{` `}` inverse, `\` the yen sign), checked and
  packed into the ROM by `tools/help_nes.py` (6 KB: runs of spaces counted). A page is drawn into the second name
  table ($2400), which is shown once the NMI has copied it all, without the sprites; the game goes on drawing into
  its own underneath, so closing simply shows it again. The QR code (page 10, `NES_QR_URL`) is in 16 quarter-block
  tiles, 2x2 modules each, so every module is exact (zbarimg reads MAME's snapshot).
- **Manual:** `docs/manual/nes/`, its captures from `PLATFORM=nes tools/capture.sh`: the autoplay image in MAME
  against the staged games (`run-nes.sh`, the dumps from `nes.lua`, drawn by `render_nes.py`), so the cursor is
  where the console puts it. The rules text names the pad's buttons through `platform.tex` (`\typeword`,
  `\HostStart`, `\LogOffAsk`...; `docs/style/synack.sty` has the keyboard's words as defaults). Help page 10's QR
  code links to it by the fly.io short link (`NES_QR_URL`).
- **Tested** (2026-10-28, MAME 0.289 from `~/Workspace/mame` with the FujiNet NES cartridge, FujiNet-PC built
  `FUJINET_TARGET=RS232` on BoIP): `make nes-auto` played four minutes, jacked in for a bot in a game in progress (every
  prompt kind answered, 0 refused, 0 resyncs); `NES_PADBOT=1` (tools/nes.lua) played three whole games with the pad alone, through the
  cursor (53 exploits, 50 buys, 17 hivejacks, infiltrations, seat counts, the handle typed with the dial), none
  refused, one connection; `pc-nes` played whole games. Every screen captured in MAME from staged games
  (`server/stage.py`): the prompts, a tie, a refusal, log off, NO SIGNAL, a taken handle, the 11 instruction
  pages. Not yet: a real cartridge (none exists yet), the lobby boot, MesenCE (FujiNet Go NES).

## Sizes (bytes)
| Piece | Code | Notes |
|---|---|---|
| `synack.xex` | 12394 | whole file; loads at $2000, the 1K font at $5400, the 1K player memory at $5800 and the help screen at $5C00 (all bss) |
| `display_text.c` | 3830 | + 1052 rodata (screen words, 364 of title art), 77 bss, 1K font, 1K player memory; the last move's blink |
| `game.c` | 2800 | + 382 rodata, 312 bss (the game state) |
| `main.c` | 1152 | + 130 rodata; with the `display_idle()` call (the last move's blink) |
| `help.s` | 302 | + 44 rodata, 131 bss, 960 its screen: the instruction pages, read with SIO (was 779 for `help.c` + `cio.s` through DOS) |
| `net_sio.s` | 372 | + 14 rodata, 26 bss, 8 data. 222 before PROCEED/SOUNDR, 318 before AppKeys |
| `proto.c` | 180 | + 26-byte length table, 128-byte `msg[]`; hand assembly could roughly halve it |
| `input.c` | 156 | + 64-byte key table; `ticks_per_sec()` tells PAL (50) from NTSC (60) by GTIA's PAL register |
| `sound.s` | 177 | + 55 rodata (11 chords), 10 bss/data: chords from the deferred VBI |
| `snow.s` | 74 | + 48 rodata (masks), 3 bss: the title's static, also from the deferred VBI |
| `dli.s` | 133 | the color bands' DLI handler and its display list switch (117 before the title's purple) |
| **apple2_text** (2026-10-24) | | |
| `synack` | 15370 | whole AppleSingle file; loads at $2000; `rx_ring` and the ProDOS buffer at $0C00 (bss) |
| `display_text.c` | 3628 | + 845 rodata (screen words, the 48-byte row table, 101 of title art), 28 bss/data |
| fujinet-lib | 3404 | open, status, read, write, close, AppKey read, clock; the bus layer alone is 1520 |
| cc65 runtime | 1585 | startup, memcpy/memset, the C stack helpers, ctype (pulled in by fujinet-lib's clock) |
| `net_fn.c` | 509 | the platform.h network calls on fujinet-lib; `net_open` checks status, as a failed open still answers fine; status paced to ~10 a second |
| `input.c` | 280 | keys, the Apple keys, and `ticks()` from FujiNet's clock, read ~10 times a second |
| `help.s` | 211 | + 68 rodata/data, 1K ProDOS buffer; the pages are read onto text page 2; lo-res for a QR page |
| `sound.s` | 85 | + 42 rodata (7 chords of up to 3 notes); `pause`, the delay loop the polling is paced by |
| `main.c` | 1152 | the Atari's plus 3 for the `display_idle()` call |
| **msdos** (2026-10-25, Open Watcom `-0 -ms -os`) | | |
| `SYNACK.EXE` | 21466 | whole MZ file; code 18232 (below); plus 6.8 KB bss and a 2 KB stack once loaded |
| `display_text.c` | 5546 | + 2205 rodata/data (screen words, title art, the circuit-board panels, the width menu, 7 hive colours), 6072 bss: 4000 for the screen's copy while the instructions are up, 1920 for a page, 120 for the log while F5 redraws |
| `game.c` | 2322 | + 382 rodata, 310 bss (the game state) |
| `main.c` | 1220 | + 152 rodata; Esc and Y on the title quit to DOS (`QUIT_TO_OS`) |
| `sound.c` | 1068 | + 175 rodata (7 speaker chords, 11 AdLib chords); the OPL2 test and set-up, the speaker's delay loop |
| `net_fn.c` | 544 | + 283 bss (the 256-byte `rx_ring`); status paced to one call a BIOS tick while nothing comes; failed exchanges let go for 3 s |
| `proto.c` | 196 | + 26-byte length table, 128-byte `msg[]` |
| `input.c` | 203 | keys from INT 16h (F4 and F5 call the display's width and colour swaps), `ticks()` from the BIOS tick |
| fujinet-lib | 1033 | open, status, read, write, close, AppKey read; the INT F5h bus layer is 255 |
| Watcom C runtime | 6098 | start-up, the BIOS calls, the help pages' `open`/`lseek`/`read` (which bring the near heap), memcpy/memset |
| **coco** (2026-10-27, CMOC 0.1.100 `-O2`) | | |
| `synack.bin` | 18624 | whole Disk BASIC file; loads at $2600, ends at $7728 with its variables, under the $7B00 limit |
| `display_text.c` | 4517 | + 1078 rodata (screen words, title art), 48 bss/data |
| `game.c` | 3183 | + 376 rodata, 389 bss/data (the game state) |
| `main.c` | 1457 | + 152 rodata |
| `text.c` | 1193 | + 600 rodata (the font: $20-$5F and the box glyphs), 1098 bss: the 960 cells, each cell's two flips for a row, the temporaries |
| `help.c` | 822 | + 89 rodata, 152 bss (the FAT, a row of cells for "no file"); reads through DSKCON |
| `net_fn.c` | 547 | + 281 bss (the 256-byte `rx_ring`); status paced to ~10 a second, carrier detect as a hint |
| `proto.c` | 222 | + 26-byte length table, 128-byte `msg[]` |
| `sound.c` | 174 | + 42 rodata (7 chords) |
| `snow.c` | 178 | + 1184 rodata (760 of line records and their 35 offsets, 98 line numbers, the 256 random bytes), 4 bss/data |
| `input.c` | 99 | keys from Color BASIC's POLCAT, `ticks()` from TIMER |
| fujinet-lib | 1730 | open, status, read, write, close, AppKey read; DriveWire through HDB-DOS |
| CMOC runtime | 852 | start-up, memcpy/memset/memmove, multiply, divide and shift helpers |
| **nes** (2026-10-28, cc65 V2.19 Git 92fa7cb23) | | |
| `syn-ack-wire-nes.nes` | 32784 | whole iNES file; 25.6 KB of its 32K PRG used (25615), 7.1 KB free |
| `display_text.c` | 5198 | + 1057 rodata (screen words, title art), 73 bss/data |
| `game.c` | 2800 | + 340 rodata, 321 bss/data (the game state) |
| fujinet-lib | 1781 | open, status, read, write, close, AppKey read; the mailbox bus layer |
| `input.c` | 1594 | the pad's cursor on each prompt (set as a prompt is drawn), the title's dial, `ticks()` |
| cc65 runtime | 1341 | the C stack helpers, memcpy/memset/memmove, multiply, divide |
| `main.c` | 1152 | + 141 rodata |
| `text.c` | 774 | + 912 rodata (the font, 95 glyphs and 11 box tiles one bit a pixel; palettes; sprite tiles) |
| `net_fn.c` | 643 | the link wait and the first call's retry, status paced to ~10 a second |
| `ppu.s` | 572 | the NMI's row copier, attributes, the static; 2405 bss (the two name tables' cells, attributes, 256 random bytes) |
| `help.c` | 394 | + 6009 rodata: the 11 instruction pages |
| `sound.s` | 235 | + 79 rodata (11 chords, 12 periods) |
| `crt0.s` | 210 | start-up, the TV check, the NMI; 256 bss (the sprites) |
| `proto.c` | 180 | + 26-byte length table, 128-byte `msg[]` |
| `pad.s` | 103 | the pad and its event ring |
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
