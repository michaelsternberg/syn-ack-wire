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
cd client && make && make atr                                # build/atari/synack.xex, build/atari/synack.atr (public server)
cd client && make atrtest                                    # build/atari/local/synack.atr, dialing localhost:6502
cd client && make apple2_text-po PRODOS=<ProDOS disk>        # build/apple2_text/synack.po: Apple II, text mode
make -C docs                                                 # rulebook and manuals (pdflatex)
client/tools/capture.sh                                      # redo the staged captures (atari800 + FujiNet-PC)
```

## Working here

- Stage files by name; commit only what the task changed.
- Someone may run their own server and emulators on this machine. Don't disturb what you didn't start: test on a
  spare port (6599, say) and `INSTANCE` (NetSIO 9997+n, FujiNet web page 8000+n), check they're free first, and
  stop if they aren't.
- Kill only the PIDs you started. Never `pkill -f` or a broad `pgrep -f`.
- atari800 rewrites `~/.atari800.cfg` on exit: run it with `-config` on a scratch copy and check the real file's
  checksum is unchanged afterwards (`capture.sh` does this).
- Bind scratch servers to this machine (`--host 127.0.0.1`).
- Never post to the FujiNet lobby, public or QA, unless asked. Never echo a token or other secret.
- `fly deploy` restarts the server machine and ends the games in progress: deploy only when asked, and with
  `deploy/deploy.sh`, which first checks no one is playing. The disks it serves are `client/build/atari/synack.atr`
  and `client/build/apple2_text/synack.po` as they are at deploy time (`server/README.md`, "Deploying to fly.io").
- AltirraOS problems under NetSIO are not to be worked around.
- Screen dumps: atari800's monitor silently drops a dump whose path is long, so dump into a short, fresh,
  empty directory. With the BASIC ROM in (as on an 800 with the cartridge), screen memory is at `$9C40`, else
  `$BC40` (`run-test.sh` `BASIC=1`).

The code is GPL-3.0 (`LICENSE`). A `CLAUDE.local.md` next to this file, if there is one, holds one person's own
habits and setup; it isn't in the repository.
