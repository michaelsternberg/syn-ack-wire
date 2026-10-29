# SYN-ACK WIRE for the desktop

The game for Windows, macOS and Linux, for players without a vintage machine. It dials the same servers as the
Atari, Apple II, MS-DOS, CoCo, NES, Adam, Intellivision and H89 clients and sits at the same tables: the server can't tell it from an 8-bit
machine, so desktop and 8-bit players take seats, see each other and play each other (and the bots).

It **is** the 8-bit client: `client/src/common/` (main loop, game state, frame assembler) built unchanged for a
PC, with this folder supplying `platform.h` and `display.h` as each machine's folder does. The screen is the
MS-DOS client's 80 columns: the 40-column game between two circuit-board panels.

## Build

CMake 3.24 or newer, a C/C++17 compiler, Python 3.11 or newer (for the generated headers and the help pages).
SDL3 and FreeType come from the system when installed, else they are fetched and built in; Dear ImGui is always
fetched.

```sh
cmake -S desktop -B desktop/build -DCMAKE_BUILD_TYPE=Release
cmake --build desktop/build                # desktop/build/syn-ack-wire, with data/ beside it
ctest --test-dir desktop/build             # the colour themes' contrast test
desktop/build/syn-ack-wire                 # dials CHIBA-GRID-7 on syn-ack-wire.fly.dev
desktop/build/syn-ack-wire --server 127.0.0.1:6599     # a server of your own
```

`-DSAW_FETCH_DEPS=ON` builds SDL3 and FreeType in even when the system has them: a program that runs on
machines without them (the release builds use it). Like the client Makefile, the build writes the client's
generated headers (`theme_tab.h`, `proto_tab.h`, the MS-DOS `title_art.h` and `margins.h`) into `client/src/`.

Releases: pushing a tag `desktop-v*` runs `.github/workflows/desktop.yml`, which builds Windows (zip), macOS (a
zipped app, for Apple silicon and Intel; it isn't signed, so the first time it needs right-click, Open) and Linux
(tar.gz) and attaches them to a GitHub release, titled "SYN-ACK WIRE desktop v0.1.0" for the tag
`desktop-v0.1.0`, with `RELEASE.md` (the install notes for players) as its description. The same notes go in
each download as `README.txt`; keep the tag and the version in `CMakeLists.txt` (Help, About) the same. A desktop
release is never marked "Latest" on GitHub: that stays the 8-bit release (`docs/releases/`), whose notes link to
the desktop releases.

## How it works

| File | What |
|---|---|
| `src/desk.h` | Between the window (C++, main thread) and the game (C, its own thread): the screen's cells, the key queue, Region changes |
| `src/desk.c` | The game thread: `client/src/common/main.c`'s `main()` as `game_main()`; `in_key()`, `ticks()` (60 a second); a Region change hangs up, logs off if asked and starts it again from the title (longjmp) |
| `src/display_text.c` | The MS-DOS display (`client/src/msdos/display_text.c`), writing a role per cell instead of a colour; the help pages from `SYNACK.HLP` |
| `src/seats.c` | The seats free at each region for the Region menu: `GET /SEATS` (`server/acquire/web.py`) on a thread of its own |
| `src/net.c` | Plain TCP to the server; `net_appkey(0)` gives back the handle typed last time, as the lobby gives an 8-bit machine the player's name |
| `src/sound.c` | The MS-DOS AdLib chords, made with two-operator FM on SDL audio |
| `src/app.cpp` | The menu bar, the dialogs, the keys, and the cells drawn with the chosen colour theme and font |
| `src/theme.cpp` | The colour themes: colours for each role, and the syndicates' hive colours (the MS-DOS ones unless a theme needs others) |
| `src/fonts.cpp`, `src/glyphs.cpp` | The font themes, fitted to the cells; box drawing, the chips and pads, blocks and ¥ drawn as shapes, so the art joins up in any font |
| `src/crt.cpp` | The CRT effect from `../4AD_SDL`: glow, scanlines (8 to a text row), curvature, vignette, flicker |
| `src/settings.cpp` | `synack.ini` in the per-user folder (`SDL_GetPrefPath`): options, the server, the handle, a seat's token |
| `../tools/gen_regions.py` | The Region menu, generated from `server/servers.fly.toml` |

**Menus.** File: Quit. Region: the four servers on fly.io, each with its seats free ("4 of 6 seats free", as the
FujiNet lobby counts them: `GET /SEATS` from the servers' web page, `src/seats.c`), read when the menu opens and
again with Refresh Seats (which leaves the menu open); then up to four custom servers used before, then Custom
Server... (a host name or LAN address and a port). Options: Colour Theme, Font, CRT Effects
(Off, Low, Medium, High), Full Screen (F11), Auto-Hide Menu Bar (the bar shows over the screen while the mouse is at
the window's top edge), Move Hints (F2), Sound (F3). Every option is remembered. Help: Instructions (F1), Keys, About.

**Keys** are the MS-DOS client's: F1 or ? the instructions (arrow keys turn pages), F2 move hints, F3 sound,
Esc then Y logs off in a game or quits from the title; the game's own keys as in `client/README.md`. F11 is
full screen.

**Seats.** Choosing another region while seated asks first, then logs off (`Q`, as Esc, Y does: a bot takes the
seat for good) and dials the new one's title screen. Quitting while seated asks first, then just hangs up: the
seat is away and its bot plays it, and the token is saved, so the next start takes the seat back with Enter.

**Colour themes:** DOS Colour (the MS-DOS client's F5 colours), Green Phosphor, Amber Phosphor, Neon, Synthwave,
Dark, Light. The hive letters keep the MS-DOS colours where they stand out (G is black on its cyan, not white)
and are drawn bold; `tests/theme_test.cpp` checks every hive's background against the backgrounds it sits on,
and its letter's contrast (at least 4.5:1). Phosphor themes show hives in reverse, as a monochrome
DOS screen does.

**Font themes:** Normal (JetBrains Mono), Share Tech Mono, Orbitron, Oxanium, VT323 Terminal, Chakra Petch
(`fonts/README.md`).

**Help pages:** `client/help/desktop/screen/HELPnn.TXT` once there are any; until then the MS-DOS pages (which
mention F4 and F5, which the desktop doesn't have). Made into `SYNACK.HLP` by `client/tools/help_msdos.py`.

## Testing

For scripted runs (see `src/main.cpp`): `--settings <file>` keeps your own settings out of it, `--option
theme=neon`, `--keys "BOB/||3"` types into the game (`/` Enter, `^` Esc, `!` F1, `|` a second's pause),
`--pick 5:127.0.0.1:6598` chooses a server as the Region menu does (`!` before it says yes to logging off), and
`--screenshot out.bmp --wait 20 --every 5` saves the window.

```sh
cd server && python3 serve.py --host 127.0.0.1 --port 6599 --bot-delay 0.3     # a scratch server
desktop/build/syn-ack-wire --server 127.0.0.1:6599 --settings /tmp/t.ini --keys "ANN/||3" &
desktop/build/syn-ack-wire --server 127.0.0.1:6599 --settings /tmp/u.ini --keys "CAT/"     # two at one table
```
