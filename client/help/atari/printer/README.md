# Printed instructions (planned)

A version of the instructions for an 80-column printer, which a title-screen option will
send to `P:` so the player has the rules at hand during play. The plan:

- The target is FujiNet's virtual printer, set to an emulated Epson 80 or Atari 825; the
  player prints the PDF FujiNet makes.
- The source is an AtariWriter document, kept here with the printer-ready file it prints to
  disk. Both are committed because `make` can't rebuild the printer file.
- The printer file goes on the game disk as raw sectors, like the screen pages in `../screen/`.

Nothing builds from this directory yet.
