# fujinet-lib, pinned

The FujiNet library the Apple II, MS-DOS and CoCo clients use for their network, AppKey and clock calls
(`src/apple2/net_fn.c`, `src/apple2/input.c`; `src/msdos/`; `src/coco/net_fn.c`). A copy lives here, unchanged, so a change upstream
can't break the build. The Atari client doesn't use it (it talks raw SIO: `src/atari/net_sio.s`).

| | Apple II | MS-DOS | CoCo |
|---|---|---|---|
| Version | 5.0.0 (2026-10-16) | 5.0.0 (2026-10-16) | 5.0.0 (2026-10-16) |
| From | https://github.com/FujiNetWIFI/fujinet-lib/releases/tag/v5.0.0, `fujinet-lib-apple2-5.0.0.zip` | the same release, `fujinet-lib-msdos-5.0.0.zip` | the same release, `fujinet-lib-coco-5.0.0.zip` |
| sha256 of the zip | `c87d1ba1706ba332237127935ca5572dd4114eb38bd447f691a525a0c386532c` | `182f1ad1826f0a6482f14fa5cbe972fb44f3dd6469584f1c5e377b5526a05b27` | `75151d41364815832feae3087ad4a944634b526212e77a1b59277a6f7835f152` |
| Licence | GPL-3.0, as this project | GPL-3.0, as this project | GPL-3.0, as this project |
| Built with | the release's own cc65; links with cc65 V2.19 (Git 8d86ca7) | Open Watcom v2 (`wcc -0 -bt=dos`: 8086 code); calls FujiNet through `FUJINET.SYS`'s INT F5h | CMOC; links with CMOC 0.1.100 and lwtools 4.25; DriveWire, over the serial port or a Becker port |

`apple2-5.0.0/` holds the zip's contents: `fujinet-apple2-5.0.0.lib`, its `Changelog.md`, and the headers in
`include/`. The linker takes only the modules the client calls: about 3.4 KB of the Apple client's 15 KB
(the general bus layer is 1.5 KB of that, linked whole). `msdos-5.0.0/` is laid out the same way, with
`fujinet-msdos-5.0.0.lib`; its zip has the files at the top level, so the headers were moved into `include/`. `coco-5.0.0/` likewise:
`fujinet-coco-5.0.0.lib`, `Changelog.md`, and the headers (and `fujinet-err.inc`) moved into `include/`.

The H89 (`h89-194824b/`) is the exception: no release has an H89 library yet, so it is built from source,
fujinet-lib's `master` at commit `194824ba1fb6713752cbc0af9e14f6febc59b6fa` (2026-10-28), by
`tools/h89/fnlib.sh SRC h89-194824b` with z88dk's zsdcc, the client's compiler (fujinet-lib's own `h89-cpm`
target uses sccz80, and the two don't mix). The script makes two fixes to its copies, both from FujiNet's
M.U.L.E. client: a duplicate table in `bus/h89/fuji_bus_call.c` goes, and `network_open` sends the URL with
its NUL. The folder holds `fujinet-h89.lib`, the headers in `include/`, and the commit in `VERSION`.
When a release has the H89, it moves to that like the others.

To move to another version: download its release zip for each platform used, check the sha256 against the
release page, unpack it into a new `<platform>-<version>/` folder beside the old one, point `A2LIB` (or the
MS-DOS or CoCo build's library path) in `client/Makefile` at it, build, play a game, then delete the old folder and
update this table.
