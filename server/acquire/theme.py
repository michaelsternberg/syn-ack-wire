"""Load theme/theme.toml: syndicate list, rule numbers and log wording.

The rules engine takes every name and number from here, so renames and rule tweaks happen in theme.toml only.
"""
import tomllib
from dataclasses import dataclass
from pathlib import Path

THEME_PATH = Path(__file__).resolve().parents[2] / "theme" / "theme.toml"
TIER_OFFSET = {"low": 0, "mid": 1, "high": 2}


@dataclass(frozen=True)
class Syndicate:
    id: int
    name: str
    short: str
    letter: str
    coin: str
    tier: str


@dataclass(frozen=True)
class Theme:
    game: dict
    rules: dict
    glyphs: dict
    terms: dict
    log: dict
    errors: dict
    status: dict
    bots: tuple
    syndicates: tuple

    def say(self, key, **kw):
        """Format a log line from the [log] templates."""
        return self.log[key].format(region=self.game["region"], shout=self.game["endshout"], **kw)

    def error(self, key, **kw):
        """Format a rejection reason from the [errors] templates."""
        return self.errors[key].format(**kw)

    def money(self, units):
        """Format money given in units of 100."""
        return f"{self.game['currency']}{units * 100}"


def load(path=THEME_PATH):
    with open(path, "rb") as f:
        t = tomllib.load(f)
    syns = tuple(Syndicate(i, s["name"], s["short"], s["letter"], s["coin"], s["tier"])
                 for i, s in enumerate(t["syndicate"]))
    return Theme(t["game"], t["rules"], t["glyphs"], t["terms"], t["log"], t["errors"],
                 t["status"], tuple(t["bots"]["names"]), syns)
