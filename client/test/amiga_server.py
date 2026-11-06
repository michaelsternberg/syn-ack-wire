#!/usr/bin/env python3
"""serve.py, saying GAME OVER in its log when a table's game ends (the server logs nothing then): for test/amiga_accept.sh."""
import asyncio, logging, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "server"))
import serve
from acquire import table

_game_over = table.Table._game_over


def game_over(self):
    logging.getLogger().info("GAME OVER table %d", self.tid)
    _game_over(self)


table.Table._game_over = game_over
sys.argv[0] = "serve.py"
try:
    asyncio.run(serve.main())
except (KeyboardInterrupt, asyncio.CancelledError):
    pass
