# SYN-ACK WIRE: notes for Claude

A cyberpunk *Acquire* for the Atari 8-bit and FujiNet: a Python server that runs the game and an 8-bit client
that draws what it is told. Read `README.md` first. This file says where the truth lives and how to work here;
it points rather than repeats, so keep it short and fix it when it goes stale.

## Where each thing is decided

| Topic | Authoritative | Generated from it / follows it |
|---|---|---|
| Every name, term, message, rule number | `theme/theme.toml` | `client/src/common/theme_tab.h`, `docs/style/theme.tex` (`tools/gen_theme.py`) |
| The rules engine | `server/acquire/rules.py` (pure: actions in, events out) | `server/tests/test_rules.py` |
| Wire protocol | `server/acquire/proto.py`, described in `docs/protocol.md` | `client/src/common/proto_tab.h` (`tools/gen_proto.py`) |
| Tables: lobby, host clock, joining, dropping, rejoining, autopilot, jack-in, log off, abandon | `server/acquire/table.py`, summarised in `server/README.md` ("Seats and tables") | `server/tests/test_server.py` (test names read as use cases) |
| Client behaviour, keys, screen layout, sizes | `client/src/`, summarised in `client/README.md` | |
| Rules text for players | `docs/rules/*.tex` (shared) | the rulebook (`docs/rulebook/`) and each platform manual (`docs/manual/<platform>/`) |
| Manual captures | `docs/scenarios.toml` (staged games, the same for every platform) | `server/stage.py`, `client/tools/capture.sh`, `docs/manual/atari/screens/` |

When behaviour changes, change the code, its tests and its summary together. The manuals are written for
players: a summary, not a specification.

## Rules of the project

- Names and terms come only from `theme.toml`, never typed into code or docs (the help pages and the title art
  are the exceptions). LaTeX uses `\V{}`, `\Say{}{}` and the other macros in `docs/style/synack.sty`.
- Keep the 6502 code minimal: measure the size change of client work and say it (`client/README.md` has the
  sizes table).
- A new platform manual is `docs/manual/<platform>/` with its own `platform.tex`, Jack-in, Screen and Keys
  chapters, and captures; everything else is shared.

## Building and testing

```sh
cd server && python3 -m unittest -q && python3 sim.py 2000   # rules and server; soak test
cd client && make && make atr                                # build/synack.xex, build/synack.atr
make -C docs                                                 # rulebook and manuals (pdflatex)
client/tools/capture.sh                                      # redo the staged captures (atari800 + FujiNet-PC)
```

## Working with the owner

- Experiments go on a branch. Commit or push only when asked; "commit and merge" alone means don't push.
- Stage files by name. `client/tools/run-test.sh` usually holds the owner's local changes: never commit it.
- Before committing, look for Nextcloud "conflicted copy" files and vim swap files.
- The owner may have uncommitted work in `theme/theme.toml` and the rulebook. Commit only your own lines (build
  the blob from `HEAD` plus your lines: `git hash-object -w`, `git update-index --cacheinfo`).
- The repo is private until development is done, then public under GPL-3.0.

## Testing safely next to the owner's own setup

- The owner runs their own `serve.py` on port 6502 and their own emulators. Test on port 6599 with
  `INSTANCE=7` (NetSIO 10004, web 8007), and check all three are free first; if not, stop.
- Kill only the PIDs you started. Never `pkill -f` or a broad `pgrep -f`.
- atari800 rewrites `~/.atari800.cfg` on exit: run it with `-config` on a scratch copy and check the real file's
  checksum is unchanged afterwards (`capture.sh` does this).
- Bind scratch servers to this machine (`--host 127.0.0.1`).
- Never post to the FujiNet lobby, public or QA, unless asked. Never echo a token or other secret.
- AltirraOS problems under NetSIO are not to be worked around.
- Screen dumps: atari800's monitor silently drops a dump whose path is long, so dump into a short, fresh,
  empty directory. With the BASIC ROM in (as on an 800 with the cartridge), screen memory is at `$9C40`, else
  `$BC40` (`run-test.sh` `BASIC=1`).
