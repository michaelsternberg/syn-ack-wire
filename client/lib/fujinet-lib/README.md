# fujinet-lib, pinned

The FujiNet library the Apple II client uses for its network, AppKey and clock calls
(`src/apple2/net_fn.c`, `src/apple2/input.c`). A copy lives here, unchanged, so a change upstream can't break
the build. The Atari client doesn't use it (it talks raw SIO: `src/atari/net_sio.s`).

| | |
|---|---|
| Version | 5.0.0 (2026-10-16) |
| From | https://github.com/FujiNetWIFI/fujinet-lib/releases/tag/v5.0.0, `fujinet-lib-apple2-5.0.0.zip` |
| sha256 of the zip | `c87d1ba1706ba332237127935ca5572dd4114eb38bd447f691a525a0c386532c` |
| Licence | GPL-3.0, as this project |
| Built with | the release's own cc65; links with cc65 V2.19 (Git 8d86ca7) |

`apple2-5.0.0/` holds the zip's contents: `fujinet-apple2-5.0.0.lib`, its `Changelog.md`, and the headers in
`include/`. The linker takes only the modules the client calls: about 3.4 KB of the Apple client's 15 KB
(the general bus layer is 1.5 KB of that, linked whole).

To move to another version: download its release zip for each platform used, check the sha256 against the
release page, unpack it into a new `<platform>-<version>/` folder beside the old one, point `A2LIB` in
`client/Makefile` at it, build, play a game, then delete the old folder and update this table.
