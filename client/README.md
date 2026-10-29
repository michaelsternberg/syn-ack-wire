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
| `src/adam/text.c`, `text.h` | Adam: the TMS9918A in Graphics II, each of the 768 cells its own pattern and colours, painted where they changed; the title's static on shared tiles |
| `src/adam/vdp.h`, `vdp.asm` | Adam: the VDP's ports and the NMI's lock; writes paced for the VDP while the screen is drawn |
| `src/adam/display_text.c` | Adam text screens: the NES's in 32×24 cells, colour bands, the controller's cursor as sprites |
| `src/adam/input.c` | Adam: the keyboard (through EOS) and controller 1 turned into keys, with the NES's cursor and dial; `ticks()` |
| `src/adam/nmi.asm` | Adam: the NMI: the frame count, the sound, controller 1 into an event ring, the title's static |
| `src/adam/net_fn.c` | Network calls for the Adam: AdamNet's network device, its DCB driven without waiting; open, close and AppKeys through fujinet-lib |
| `src/adam/help.c` | Adam: the instruction pages, from the program over the screen |
| `src/adam/sound.asm`, `eoscall.asm` | Adam: the Atari's chords on the SN76489, from the NMI; the keyboard through EOS's jump table |
| `src/adam/title_art.h` | The Atari's title art in the NES's box glyphs, **generated** by `tools/title_art.py n 0` |
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
| `test/adam_text_pc.c` | What `src/adam/display_text.c` needs on a PC (`make pc-adam`): the cells, palettes and sprites, which the dumps save |
| `test/dos_glitch.c` | A flaky serial line for testing MS-DOS builds: with `-Dnetwork_status=glitch_status`, the status call fails 1 second in 20 |
| `tools/run-test.sh` | Runs an `.xex` on atari800 with FujiNet-PC (NetSIO) |
| `tools/run-dos.sh` | Plays the MS-DOS client in a DOSBox-X window, booting your PC-DOS disk with `FUJINET.SYS`, FujiNet-PC (RS-232) on COM1 |
| `tools/help_msdos.py` | Checks the MS-DOS instruction pages and writes them as screen images (`SYNACK.HLP`) |
| `tools/screen2txt.py` | Shows a screen dump as text |
| `tools/render_screen.py` | Draws a screen dump as the screen looks, for the manual's captures |
| `tools/help_apple2.py` | Checks the Apple instruction pages and writes them as text-page images (`SYNACK.HLP`) |
| `tools/prodos_boot.py`, `title_text.py` | The Apple disk's ProDOS boot block; the Apple title logo as a header |
| `tools/apple2_preview.py` | Draws a screen dump as Apple II hi-res would show it on a colour monitor, three ways |
| `tools/capture.sh` | Captures the manual's staged screens (`docs/scenarios.toml`, `server/stage.py`) on atari800; `PLATFORM=apple2`: with `pc-apple2`, and likewise `msdos`, `coco`, `adam`; `PLATFORM=nes`: the autoplay image in MAME; `PLATFORM=intv`: the autoplay ROM in jzIntv |
| `tools/render_apple2.py` | Draws an Apple II screen dump in the IIe/IIc character set, or shows it as text (`--text`) |
| `tools/help_coco.py` | Checks the CoCo instruction pages and writes them as PMODE 4 screen images (`SYNACK.HLP`), the QR code in pixels |
| `tools/xroar_dump.py` | Saves the CoCo autoplay build's screens from xroar through its GDB port |
| `tools/render_coco.py` | Draws a CoCo screen dump (6144 bytes of PMODE 4, or `pc-coco`'s 960 cells, a title's logo with its static) as the screen looks |
| `tools/run-nes.sh`, `nes.lua` | Runs the NES image in MAME (`-nes_slot fujinet`) with FujiNet-PC: the pad on a schedule or a pad bot, the screen as text, snapshots, the autoplay build's dumps |
| `tools/help_nes.py` | Checks the NES instruction pages and packs them for the ROM (`help_pages.h`), the QR code in quarter-block tiles |
| `tools/render_nes.py` | Draws an NES dump (`pc-nes`'s or the autoplay build's) or an instruction page as the screen looks, or as text |
| `tools/run-adam.sh` | Runs the Adam image in adamcore (FujiNet Go Adam's core) or ADAMEm, booted from FujiNet-PC on BoIP: keys and the controller on a schedule, pictures |
| `tools/render_adam.py` | Draws a `pc-adam` dump or an Adam instruction page as the screen looks, or as text |
| `tools/z88dk`, `fnlib_adam.sh`, `adam_end.py` | z88dk from the defoogi container; fujinet-lib built with zsdcc; where the program ends, against the bootstrap |
| `src/intv/` | The Intellivision client: the common code again in IntyBASIC (`main.bas`, `game.bas`, `proto.bas`), its display (`display.bas`, `prompt.bas`, `title.bas`, `help.bas`), the FujiNet cartridge (`fujinet.bas`, `net.bas`), the controller and the ECS keyboard (`input.bas`, `ecskbd.bas`, `ecskbd.asm`), `sound.bas`, `isr.bas`, `memory.bas` (the cartridge RAM's map) |
| `tools/bas_pp.py` | Fills the theme's and the protocol's `{NAME}`s into the IntyBASIC sources from `theme_tab.h` and `proto_tab.h`, keeps or drops `'#if` lines, and fails a filled-in string too long for its place (`'fit N`) |
| `tools/checkcfg.py` | Fails an Intellivision ROM that would land on the ECS's ROM, the cartridge's RAM or a GRAM mirror |
| `tools/run-intv.sh`, `intvsim.py` | Runs the Intellivision ROM in jzIntv with FujiNet-PC: headless, a scenario of presses (the keypad, the buttons, the disc, the ECS keyboard), the screen as text and pictures; or in a window |
| `tools/render_intv.py` | Draws an Intellivision dump (BACKTAB, GRAM, the mode: `intvsim.py` saves them) as a TV shows it, or as text |

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
make adam                                # build/adam/syn-ack-wire-adam.ddp: the Coleco Adam client
make pc-adam                             # build/pc/pc-adam: the common code with the Adam display, for captures
make h89                                 # build/h89/SYNACK.COM and SYNACK.HLP: the Heathkit H89 client (CP/M)
make h89-disk H89SYS=disk                # build/h89/syn-ack-wire-h89.h8d: an H17 disk that boots CP/M, with both
make h89-wiremon H89SYS=disk             # build/h89/wiremon.h8d: the protocol monitor on the H89
make pc-h89                              # build/pc/pc-h89: the common code with the H89 display, for captures
make intv                                # build/intv/syn-ack-wire-intv.rom: the Intellivision client (the FujiNet cartridge)
```
Toolchain: cc65 master, `V2.19 - Git 8d86ca7` (2026-10-18). Size numbers below are from that build. Adam: z88dk
`v25230-82c677065d` with zsdcc, in the `fozztexx/defoogi:1.4.7` container (`tools/z88dk` runs it). MS-DOS: Open
Watcom v2 (`2.0 beta Sep 22 2025`), as the FujiNet team builds its DOS programs; `WATCOM` is its folder (default
`~/openwatcom`). The game needs `FUJINET.SYS` (fujinet-rs232) loaded in CONFIG.SYS, from the same firmware version as
the FujiNet. Intellivision: IntyBASIC 1.5.2 (9393a28), built by `make intv` from its source (`IB_SRC`, default
`~/Workspace/IntyBASIC`), and jzIntv's `as1600` on PATH.

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

## Coleco Adam (adam)
The same common code with an Adam platform layer (`src/adam/`), the NES's display and controls ported: its screens
on the TMS9918A's 32×24 cells, its cursor on controller 1, and the keyboard typing as on the other computers.
Built with z88dk (`zcc +coleco -subtype=adam`, zsdcc) from the defoogi container (`tools/z88dk`); FujiNet on
AdamNet through fujinet-lib, built for zsdcc and pinned (`lib/fujinet-lib/adam-a29e57f`).
```sh
make adam                                    # build/adam/syn-ack-wire-adam.ddp
make adamtest                                # build/adam/local/...: dialing LOCAL_URL
make adam-auto URL=N:TCP://host:port/ LOBBY_KEY=0   # build/adam/auto/...: plays by itself
make pc-adam && build/pc/pc-adam host port   # the display on this PC against a server, saving its screen
tools/run-adam.sh build/adam/syn-ack-wire-adam.ddp   # boot it in adamcore (or EMU=adamem) from FujiNet-PC
```
The image is `syn-ack-wire-adam.ddp`, a tape image: z88dk's bootstrap in block 0 loads the program from block 1
to 0000h and runs it. The FujiNet mounts it as the first disk and the Adam boots it. The game lobby's code mounts a
platform's image in slot 0 and reboots (`fujinet-lobby`, `clients/src/main.c`), having written the server's
address to the AppKey, so the client's AppKey dial is the other platforms' (the keys tested; the lobby boot not
yet). The program must end below
C800h, where the bootstrap sits (`tools/adam_end.py` fails the build otherwise); its variables follow it, the
stack is below D390h and EOS above.

- **Screen:** Graphics II with the name table 0-255 in each third of the screen, so each of the 768 cells has a
  pattern and colours of its own: any glyph in any colours anywhere, where the NES's palettes go by 2×2 blocks. The
  display writes `txt_cells` and `txt_pal` and marks rows; the outermost call paints them (`text.c`), cell by cell
  against a copy of what the VDP holds, so only what changed is written: 8 pattern bytes for a new glyph, 8 colour
  bytes for new colours. A whole screen is about a fifth of a second. Writes are paced 36 cycles apart
  (`vdp.asm`), past the 29 the VDP may need while it draws the screen.
- **Layout:** the NES's in 32 columns and the 24 rows the TMS9918A has, all of them on the screen: the title bar
  (0), the Grid with the hives panel and LAST beside it (1-10), the seats (11-17: handle, cash in 6 columns, 7
  holdings of 2), a log of 4 rows where the NES has 6 (18-21), the info row and the question with the clock
  (22-23: a question is at most 28 columns). game.c's refusals are the NES's narrow ones, but `SAY_MAXBUY` names
  RETURN.
- **Colour:** the NES's palettes in TMS9918A colours: light green on black; white on dark green for the Grid and
  the seats, light yellow there for cash and every other syndicate's crypto; white on magenta for the bottom rows
  while this seat decides, and the title bar while the link is down (NO SIGNAL). Inverse swaps a cell's colours.
- **Title:** the Atari's logo in the NES's box glyphs (`tools/title_art.py n 0`, rows 0-13). Its strokes are 4
  codes; in each third of the screen the first cell with each code keeps its own tile and the others are pointed
  at it in the name table (`txt_title`), so the NMI makes static of the whole logo by writing 8 tiles a frame.
  Under it the region, the status, the handle and the keys, the keyboard's and the controller's side by side.
- **NMI** (`nmi.asm`): the VDP's frame interrupt, which can't be masked. Reading the VDP's status in it would break
  an address being set, so while the program is at the VDP (`VDP_LOCK`, a row at a time) the NMI only notes it and
  `VDP_UNLOCK` catches up, as fujinet-multiplayer-mule does. Each frame it counts `frames` (`ticks()`, 60 a
  second), fades the chords, reads controller 1 and writes the static.
- **Keys:** the keyboard through EOS (`eoscall.asm`): letters, digits, RETURN, BACKSPACE (Delete), ESCAPE (Esc);
  SmartKey I the instructions, II move hints, III sound; the arrows move the cursor as the stick does, and turn the
  instruction pages. A typed letter goes on the end of the handle, where the cursor starts.
- **Controller 1:** read from its port by the NMI ($C0/$80/$FC: EOS's poller doesn't run under our NMI) into an
  8-event ring in the NES pad's own buttons, so `input.c` is the NES's cursor and dial: left fire is A (the
  cursor's answer, YES), right fire B (Delete, NO), keypad # START (Return), * SELECT (on its own the
  instructions; with left fire hints, with right fire sound, with # log off); the stick moves the cursor, or on
  the title dials the handle's letters. The keypad's digits type. A count changed by the stick or typed is the
  same `edit[]`.
- **Network** (`net_fn.c`): fujinet-lib's calls set a DCB going and spin until it finishes, retrying a timeout 20
  times, and an AdamNet exchange can take a frame or more (FujiNet Go Adam's core answers about once a frame). So
  only the open, the close and the AppKeys are fujinet-lib's; the stream drives the network device's DCB itself and
  never waits, as fujinet-multiplayer-mule's `nstrans.c` does: each `net_poll` takes what finished and starts the
  next call, a queued WRITE first, else an AdamNet STATUS (every frame while bytes come, about 10 a second while
  none do), whose device status byte says whether bytes wait and the socket is up, and a READ only when bytes
  wait. A READ brings a whole packet whatever it asks for, up to 1024 bytes, so it reads into a buffer that size
  and hands the bytes on as the frame assembler makes room. The link is down after 3 statuses in a row say the
  socket is shut.
- **Sound** (`sound.asm`): the Atari's chords on the SN76489's three tone channels, faded together by the NMI.
- **Instructions:** `help/adam/screen/HELP*.TXT`, 24 lines of 30 (`tools/help_nes.py --rows 24 --top 0`), packed
  into the program (5.9 KB). A page is painted over the screen without the sprites; the game goes on writing its
  own cells underneath, unpainted, and closing paints them again. The QR code (page 10, `ADAM_QR_URL`) is in the
  quarter-block glyphs, 2x2 modules a cell (zbarimg reads it from `render_adam.py --page`).
- **Manual:** `docs/manual/adam/`, its captures from `PLATFORM=adam tools/capture.sh`: `pc-adam` against the staged
  games, drawn by `render_adam.py`. Keys and Controller is its own chapter; the rules text has the keyboard's words.
  Help page 10's QR code links to it by the fly.io short link (`ADAM_QR_URL`).
- **Emulators** (`tools/run-adam.sh`): FujiNet-PC (an ADAM build) serves the image as disk 1 with CONFIG off, and
  the network; the emulator listens on BoIP and FujiNet-PC dials it. adamcore (FujiNet Go Adam's core,
  `adamcore-cli`) runs headless in real time, keys on a schedule (`KEYS`), a picture every `EVERY` frames;
  ADAMEm (`EMU=adamem`) in its window. The controller in `KEYS` needs an adamcore-cli that takes the controller's
  word on stdin as well (`FE hi lo`); the released one takes only keys.
- **Tested** (2026-10-29, FujiNet-PC built for the ADAM, `fujinet-pc-adam` cd010f7be, on BoIP): in adamcore
  (`adamcore-cli`), `make adam-auto` played four minutes of a game in progress (27 exploits, 26 buys, 2 hivejack
  answers, an infiltration, a tie; the server rejected none, no resyncs), and in ADAMEm (SDL 2.1g) hosted one (the
  seat count, then exploits and buys). The controller alone dialled a handle, jacked in, set the seats, ran exploits, brokered, bought, opened
  and paged the instructions and turned hints off; the keyboard did the same with its keys. Also: the lobby's
  AppKeys (the name, and an address overriding the built-in URL) from the SD card; a taken handle; NO SIGNAL, the
  server stopped for 36 s and the seat taken back on the redial; log off; the chords, in adamcore's sound. The
  NES, Atari and Apple images build byte for byte as before. `pc-adam` played a whole game to GAME OVER and the
  rematch. Not yet: a real Adam, the VDP's timing on one, FujiNet Go Adam itself, the lobby's boot.

## Heathkit H89 (h89)
The same common code with an H89 platform layer (`src/h89/`), for a Heathkit H89 (or Zenith Z-89) with 64K under
Heath/Zenith CP/M 2.2.04, and the H89 FujiNet card. z88dk with its zsdcc back end (`zcc +cpm -compiler=sdcc
-SO3`), as FujiNet's own M.U.L.E. client builds for the H89; zcc and zsdcc on the PATH. A string `-D` goes
through a shell twice in zcc, so the Makefile quotes it twice (`H89DEFS`).
```sh
make h89-disk H89SYS=cpm-2.2.04-64k.h8d     # build/h89/syn-ack-wire-h89.h8d; boot it, then SYNACK
```
- **The disk:** `H89SYS` is a 64K CP/M 2.2.04 system disk for the H17 (fujinet-go-h89-desktop has one,
  `tools/disks/boot/cpm-2.2.04-64k.h8d`); the Makefile copies it, takes off its development tools and copies the
  game and its help on, with cpmtools and `tools/h89/diskdefs` (the H17 format: 40 tracks of ten 256-byte
  sectors, skew 4, three system tracks). The H89 can't boot from FujiNet, so the game comes on a disk of its own.
- **Network:** fujinet-lib built from source (`lib/fujinet-lib/README.md`: no release has the H89 yet), over
  the card's 82C55 at D0h. `net_fn.c` is MS-DOS's, with the BIOS's 2 ms clock for the tick: asked once a
  tick while nothing comes, 3 seconds of failed exchanges before the line counts as dropped.
- **The shot clock:** the word at 000Bh, which the BIOS counts up every 2 ms; `ticks()` is it in eighths, 62 a
  second (62.5: the countdown shows one second short, as elsewhere).
- **Screen:** the H19 terminal, 80x24, on a serial line inside the H89 at 9600 bps, about a millisecond a
  character. MS-DOS's layout, in columns 20-59, with its circuit board either side (`src/h89/margins.txt`, its 24
  rows), drawn into memory: each cell a character and its attributes (reverse, the graphics characters).
  `display_flush()`, with every key check, sends the H19 only the cells that changed, each run after a cursor
  move (ESC Y), switching reverse (ESC p/q) and graphics (ESC F/G) as the cells need: graphics only matter for
  the codes 5Eh-7Eh they replace, lower case among them, so the game's upper case never switches. A whole new
  screen takes about a second; a move, a few milliseconds. The last move flips between normal and reverse for 4
  seconds (the H19 has no blink). Money shows as `Y3200`. Holy Daemon is **H** on the H89 (`BOARD_LETTERS`, from
  theme.toml's letters with D as H), as every client will be.
- **Title:** the Atari's logo as an outline in the graphics characters, each column doubled to keep its
  proportions on the H19's narrow cells and centred, over the board, which is cleared a cell round it
  (`tools/h89/screens.py`: a chip the clearing touches goes whole, cut traces end in pads).
- **Keys:** the H19's f1-f3 and arrows send ESC and a letter; ESC alone, with nothing after it in about 50 ms, is
  the ESC key. **F1** or **?** the instructions, **F2** move hints, **F3** sound, **ESC** on the title quits to
  CP/M after a Y (`QUIT_TO_OS=1`), **BACK SPACE** or **DELETE** undoes a letter, **RETURN** confirms.
- **Sound:** the H19's bell, for your turn, a hivejack and the null-route; the other chords stay quiet.
- **Instructions:** `SYNACK.HLP` from `help/h89/screen/HELP*.TXT` (MS-DOS's pages, with the H89's keys and manual
  link) by `tools/h89/help.py`, which lays them out with `tools/help_msdos.py` and turns each cell into the H19's
  (the QR code in the graphics characters' half block). Read with CP/M's random reads from the logged-in drive:
  page 1 at start, while the drive still turns from loading the program, and each page reads the next one ahead
  into a second buffer, so a page starts drawing as soon as its key is pressed (the H17's motor is off by then,
  and waiting for it was most of a second).
- **The .COM** is cut where its uninitialised variables start (`tools/h89/trimcom.py`): z88dk writes their zeros
  into the file, and the start-up clears them anyway.
- **Captures:** `make pc-h89` and `PLATFORM=h89 tools/capture.sh`; `tools/h89/render.py` draws a dump of the whole
  screen in the H19's own font (MAME's `heath_tlb.zip`).
- **Tested** (2026-10-29, FujiNet Go H89 0.2.1, its FujiNet built in): the protocol monitor through 10 resyncs
  and a game, 0 bad frames; the client through a whole game against bots, the title, the instructions and the
  bell. Not yet on a real H89.
## Intellivision (intv)
For the Intellivision with the FujiNet cartridge, which serves the console a mailbox at $9C00-$9F3F and RAM at
$8000-$9BFF, 8 bits a location. There is no C compiler for the CP1610, so `src/intv/` is the common code again, in
IntyBASIC: `main.bas` is `main.c`, `game.bas` is `game.c` (every frame and every local check: `why_not`, the hivejack
and buy checks), `proto.bas` is `proto.c`. A change to the common code's behaviour needs making here too.
```sh
make intv                                    # build/intv/syn-ack-wire-intv.rom
make intvtest                                # build/intv/local/...: dialing LOCAL_URL
make intv-auto URL=N:TCP://host:port/ LOBBY_KEY=0   # build/intv/auto/...: plays by itself
tools/run-intv.sh build/intv/syn-ack-wire-intv.rom SCENARIO    # headless in jzIntv with FujiNet-PC
WINDOW=1 ECS=1 tools/run-intv.sh build/intv/syn-ack-wire-intv.rom   # in jzIntv's window, an ECS attached
```
The image is `syn-ack-wire-intv.rom`, the cartridge's and jzIntv's format (the FujiNet lobby and CONFIG boot a `.rom`
from TNFS, as M.U.L.E.'s Intellivision client is booted); `synack.bin` and `synack.cfg` beside it are only for
`tools/checkcfg.py`.

- **Build:** `tools/bas_pp.py` fills each `{NAME}` in the sources from `theme_tab.h`, `proto_tab.h` and the Makefile
  (URL, LOBBY_KEY, TABLE, HINTS), so every word that names a game thing still comes from `theme.toml`; a line ending
  `'fit N` fails the build if its filled-in strings are longer than N, the room they have on a 20-column screen.
  IntyBASIC 1.5 (`OPTION MAP 2`), then as1600. The ROM keeps to $4810-$4FFF, $5000-$6FFF, $A000-$B7FF and
  $C040-$DFFF: `checkcfg.py` fails it on $2000-$2FFF, $7000-$7FFF or $E000-$EFFF (the ECS's ROM; the EXEC also runs
  whatever is at $7000 at power-up), on $8000-$9FFF (the cartridge's RAM and mailbox), or on $B800-$BFFF and
  $F800-$FFFF (where some consoles show GRAM).
- **Memory:** IntyBASIC has about 220 8-bit and 50 16-bit variables and a 24-word stack, so the game state, the frame
  buffer and a 1K receive ring are in the cartridge's RAM (`memory.bas` has the map), reached with PEEK and POKE.
  The stack is the tight one: 21 of its 24 words used at most, measured through play and a redial
  (`intvsim.py`'s `stack`). Keep the frame handler shallow and the main loop's calls few deep.
- **Network** (`net.bas`, `fujinet.bas` after M.U.L.E.'s): one mailbox transaction out at a time, never waited on
  while the game runs. Once a frame the main loop looks for its answer and posts the next: frames queued by
  `net_send` first, else a STATUS (at once while bytes arrive or an answer is due, else every 6 frames), and after a
  STATUS that says bytes wait, a READ of as many as the mailbox's 512 and the ring's room allow. The open, the close
  and the AppKey reads wait: they come before play, or after it.
- **Screen:** 20 x 12 cards in foreground/background mode: upper case from the GROM, any background colour per card,
  the yen sign a GRAM card. Too small for the other platforms' layout, so three views, which the disc (or the lower
  right button) steps through; rows 10 and 11 are the same in all of them:
  - GRID: the column letters (or NO SIGNAL, in red), the Grid with its row digits, each syndicate's hive a letter on
    the syndicate's colour (`syn_cw` in `display.bas`; the colours are the theme's, as near as the Intellivision's
    16 come); empty nodes dim dots, spikes white; the kit as digits where each exploit would run, yellow if it can,
    red if not yet (move hints). Beside it a line per syndicate: its keypad digit while a prompt asks for one, its
    letter, its hive's size (Black ICE black on white), your crypto (yellow), or while buying its price; then your
    cash and the last move, which blinks for 5 seconds on the Grid.
  - SEATS: the region, the six seats (> you, * bot, ? away; whose turn in green; cash in 6 columns), the log's last
    3 rows (a line over 20 wraps at a word).
  - CRYPTO: who holds how much of each syndicate's crypto (handles cut to 5), what is left in the bank, the hives'
    sizes, and while buying the prices.
  - Row 10 says what the open prompt is about (the kit, `INFILTRATION!`, `I TAKES O. HOLD 2`, the letters bought),
    a refusal (game.c's, worded to fit 20), or the newest log line, scrolling when it is longer than 20. Row 11 is the
    question (17 columns) and the clock. Both turn purple while this seat has a decision to make; a new prompt brings
    GRID back.
- **Title:** colour-stack mode. The Atari's logo in colored squares, a square for each character of
  `title4.atascii` (`tools/title_art.py i`): its outline green, its letters' strokes static, black and tan squares
  that the frame handler rewrites a third of a frame at a time (`SNOW`, in `title.bas`). Under it the region, the
  status (wrapped), the handle and a line of keys that changes every 2.5 seconds.
- **Controller:** the keypad answers: 1-6 run an exploit (or the
  host's seat count), 1-7 pick a syndicate by its line in the GRID's panel, digits and ENTER the hivejack's counts,
  CLEAR undoes, and 1 and 0 are yes and no (so ENTER after a buy never calls the null-route). The top buttons open
  the instructions, the lower left the options on rows 10-11 (1 move hints, 2 sound, 3 log off, which asks again),
  the lower right the next view. The handle is the lobby's name, or dialled: the disc up and down changes the letter
  at the dial (A-Z, 0-9, punctuation; past the end it starts a new one), right and left move it, CLEAR deletes.
  `input.bas` is M.U.L.E.'s decoding, which every FujiNet Intellivision client shares (a keypad key, a button and the
  disc all report on the controller's one byte): the frame handler puts each press into a 16-key ring with the ECS's,
  so none is lost while the main loop redraws a view (about 6 frames) or waits on the cartridge.
- **ECS keyboard**, when one is attached (`ECS.AVAILABLE`; the ROM says `ecs_compat = 2`: it runs without one):
  netcat's scan of the keyboard's matrix every frame (`ecskbd.asm`, Joe Zbiciak's CC0 scanner). It types as the
  computers do: the handle, syndicate letters, digits, RTN, the left arrow to delete, Y and N, `?` for the
  instructions, ESC for the options; the other arrows step the views.
- **Sound** (`sound.bas`): the Atari's chords on the PSG's three tone channels, faded by the frame handler. The PSG's
  periods are the Adam's SN76489 dividers (the same 3.58 MHz clock); a PAL console's PSG is a little sharp.
- **Instructions:** `help/intv/screen/HELP*.TXT`, 17 pages of 11 lines of 20 and a page bar, written for the keypad
  (`tools/help_nes.py --rows 12 --cols 20 --top 0 --left 0 --bas`), 1.8K words in the ROM. They draw over the screen
  (from the title too, which then comes back); the disc turns them, any button closes them. No QR code: the manual's
  address needs a version-2 code, 25 modules, and colored squares give 24 rows; the last page gives it as text.
- **Manual:** `docs/manual/intv/`, its captures from `PLATFORM=intv tools/capture.sh`: the autoplay ROM in jzIntv
  against the staged games, a dump at each new prompt (in a lobby the autoplay shows the seats, for the
  lobby-join capture), drawn by `render_intv.py`. The Controller is its own chapter, the ECS keyboard in it.
- **The lobby:** AppKey 11 holds the server's address, AppKey 0 the player's name, as on the other platforms.
  `fujinet-intv-auto-racing` registered key 11 as well: the lobby writes the key before it boots either game, but a
  SYN-ACK WIRE cartridge started on its own after Auto Racing was played from the lobby, on the same FujiNet, dials
  Auto Racing's relay. Moving Auto Racing to a free key fixes it.
- **Emulator** (`tools/run-intv.sh`): jzIntv (the FujiNet-patched build in `~/Workspace/jzintv-20200712-src`) with a
  private FujiNet-PC (an RS-232 build, `~/Workspace/fujinet-pc-rs232/build/dist`) listening on BoIP 9980 + INSTANCE,
  its web page on 8040 + n, CONFIG off. Headless, `tools/intvsim.py` steps jzIntv's debugger a frame at a time and
  plays a scenario: keypad keys, buttons and the disc through `input.bas`'s TEST_HOOK, ECS keys through `ecskbd.bas`'s
  ECS_HOOK, the screen as text and pictures, the autoplay build's pictures at each prompt (`dump_hook`), and the
  stack's high-water mark.
- **Tested** (2026-10-29, jzIntv a06a24a, FujiNet-PC rs232 1c5dcb807, on BoIP): the autoplay build played half an
  hour, two whole games to their TOP NETRUNNER, hosting the next each time, and rejoined its seat by its handle after
  being stopped (206 answers: 90 exploits, 87 buys, 16 hivejack answers, 8 brokers, a tie, 4 seat counts; the server
  rejected none, no resyncs). By hand,
  through TEST_HOOK: a dialled handle, the seat count, exploits, a buy, the three views, the instructions (from the
  title and mid-game), the options and log off. The ECS keyboard: a typed handle, `?`, ESC, letters at a buy. The
  lobby's AppKeys from the SD card (the name, and an address over the built-in URL), a taken handle, NO SIGNAL with
  the server stopped 40 s and the seat taken back on the redial. Not yet: a real Intellivision and cartridge, the
  lobby's own boot, FujiNet Go INTV, a tie (K), PAL.

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
| **adam** (2026-10-29, z88dk v25230-82c677065d, zsdcc `-SO3`) | | |
| `syn-ack-wire-adam.ddp` | 262144 | the whole tape image; the program in it is 25102 bytes from 0000h, its variables on to 7BF3h, 26 KB under the bootstrap at C800h |
| `display_text.c` | 4920 | + 1051 rodata (screen words, title art), 24 bss/data |
| `game.c` | 2974 | + 341 rodata, 319 bss/data (the game state) |
| `input.c` | 2078 | the keyboard, the controller's cursor on each prompt (the NES's), the title's dial, `ticks()` |
| `text.c` | 1561 | + 892 rodata (the font, 95 glyphs and 11 box glyphs one bit a pixel; palettes; sprite patterns), 3904 bss: the cells, their palettes, an instruction page's cells, the copy of what the VDP holds |
| fujinet-lib | 1369 | open, status, close, AppKey read, the AdamNet bus layer; + 16 rodata, 526 bss |
| `main.c` | 1273 | + 152 rodata |
| `net_fn.c` | 1013 | + 1377 bss: the 1K read buffer, the 256-byte `rx_ring` |
| `nmi.asm` | 325 | the frame, controller 1 and its event ring, the static; 302 bss (264 random bytes) |
| `sound.asm` | 297 | the chords and their envelope, from the NMI |
| `help.c` | 234 | + 5896 rodata: the 11 instruction pages |
| `proto.c` | 225 | + 26-byte length table, 128-byte `msg[]` |
| `vdp.asm`, `eoscall.asm` | 94 | paced VRAM writes; the keyboard through EOS |
| z88dk runtime | 348 | start-up (the restart vectors and the NMI's jump), memcpy/memset/memmove, multiply, divide; its console left out (2.5 KB) |
| **h89** (2026-10-29, z88dk zsdcc `-SO3`) | | |
| `SYNACK.COM` | 21729 | whole file (the public server built in), without its 16.9 KB of zeros; loads at $0100, runs to $97D7 with its variables |
| `display_text.c` | 5460 | + 2794 rodata (screen words, the board, the title: `screens.h`), 15407 bss: the screen, the H19's copy of it, the cells under the instructions (3840 each) and two help pages |
| `game.c` | 2966 | + 382 rodata, 310 bss (the game state) |
| `main.c` | 1316 | + 170 rodata; Esc and Y on the title quit to CP/M (`QUIT_TO_OS`) |
| `net_fn.c` | 616 | + 283 bss (the 256-byte `rx_ring`) |
| `proto.c` | 220 | + 26-byte length table, 128-byte `msg[]` |
| `input.c` | 194 | keys and the H19's escape sequences, `ticks()` from the BIOS's clock |
| `sound.c` | 83 | the bell |
| fujinet-lib | 6291 | open, status, read, write, close, AppKey read; FujiBus over SLIP to the card |
| **intv** (2026-10-29, IntyBASIC 1.5.2 9393a28, as1600; in 16-bit words) | | |
| `syn-ack-wire-intv.rom` | 16454 | words of ROM, of the 24K the map allows (`checkcfg.py` prints each segment); 33896 bytes of file |
| `prompt.bas` | 3292 | rows 10-11: what each prompt is about, its question, the refusals, the options, the messages |
| `display.bas` | 2760 | the three views, the Grid, the seats, the log and its ticker, numbers and cash |
| `game.bas` | 1927 | the frames and the keys: `game.c` |
| `main.bas` | 1817 | `main.c`, with IntyBASIC's runtime (its epilogue: PRINT, division, the frame handler's own share) |
| `help_pages.bas` | 1801 | the 17 instruction pages, two characters a word |
| `title.bas` | 1436 | + 617 `title_art.bas` (the logo's 87 cards, the static's 60 and its 256 random words) |
| `net.bas`, `fujinet.bas` | 969 | the mailbox, the stream's state machine, the AppKeys, the 1K ring |
| `input.bas`, `ecskbd.bas` | 774 | the controller, the key ring, the ECS's scanner (its decode tables 192) |
| `help.bas`, `sound.bas` | 608 | the pages drawn; the chords (11, 12 periods) |
| `proto.bas` | 200 | + 1K ring, 128-byte frame in the cartridge's RAM |
| cartridge RAM | 1824 | bytes of $8000-$9BFF used ($8000-$871F) (`memory.bas`): the ring 1024, the frame 128, the game state, the log, the key ring |
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
