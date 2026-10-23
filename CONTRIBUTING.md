# Contributing

Thanks for your interest. SYN-ACK WIRE is still in development; issues and ideas are welcome, and for anything
larger than a fix, please open an issue first so we can talk it over.

## Before sending a change
- **Server:** `cd server && python3 -m unittest -v` passes, and `python3 sim.py 500` finishes without errors.
- **Client:** `cd client && make && make pc` builds with no warnings. If you changed the Atari code, run it in
  atari800 with FujiNet-PC (`tools/run-test.sh`) and say what you tried.
- **Rulebook:** `make -C docs/rulebook` builds.

## Conventions
- **Names come from `theme/theme.toml`.** Syndicates, terms, log lines and rule numbers are edited there, never
  typed into the server, client or rulebook. (The Atari help pages and the title art are hand-made and are the
  exception.)
- **The server decides.** The client checks answers only to give the player a quick reason; every rule is
  enforced in `server/acquire/rules.py`, and a rule change comes with a test in `server/tests/`.
- **Keep the 6502 side small.** Every byte counts on an 8-bit machine: prefer the smaller solution, and update
  the sizes table in `client/README.md` when they change. Common client code is C written for cc65 (unsigned
  char globals, one operation per statement); platform code lives in `client/src/<platform>/`.
- **Protocol changes** go in `docs/protocol.md` and `server/acquire/proto.py` together; the client's tables are
  generated from the latter. Keep older clients working where possible.

## License
By contributing, you agree that your contributions are licensed under the GNU General Public License v3 or
later, like the rest of the project (see `LICENSE`).
