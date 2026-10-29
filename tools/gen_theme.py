#!/usr/bin/env python3
"""Generate per-target files from theme/theme.toml, the single source of names and vocabulary.

Usage:
    gen_theme.py check          validate theme.toml only
    gen_theme.py tex            print LaTeX macros (docs/rulebook/theme.tex)
    gen_theme.py c              print the 8-bit client's C header (client/src/common/theme_tab.h)
"""
import re
import sys
import tomllib
from pathlib import Path

THEME = Path(__file__).resolve().parent.parent / "theme" / "theme.toml"
TIERS = ("low", "mid", "high")
SCREEN_NAME = 15        # client hive panel width for a syndicate name


def load():
    with THEME.open("rb") as f:
        t = tomllib.load(f)
    validate(t)
    return t


def validate(t):
    errs = []
    syns = t["syndicate"]
    if len(syns) != 7:
        errs.append(f"need 7 syndicates, found {len(syns)}")
    letters = [s["letter"] for s in syns]
    for s in syns:
        n = s["name"]
        if len(s["letter"]) != 1 or not s["letter"].isupper():
            errs.append(f"{n}: letter must be one uppercase char")
        if len(s.get("screen", n)) > SCREEN_NAME:
            errs.append(f"{n}: too long for the client's hive panel ({SCREEN_NAME}); add a shorter `screen` name")
        if len(s["short"]) > 8:
            errs.append(f"{n}: short name '{s['short']}' longer than 8")
        if len(s["coin"]) > 4:
            errs.append(f"{n}: coin '{s['coin']}' longer than 4")
        if s["tier"] not in TIERS:
            errs.append(f"{n}: tier must be one of {TIERS}")
    dupes = {c for c in letters if letters.count(c) > 1}
    if dupes:
        errs.append(f"duplicate board letters: {sorted(dupes)}")
    for g in t["glyphs"].values():
        if g in letters:
            errs.append(f"glyph '{g}' clashes with a syndicate letter")
    for key in t["terms"]:
        if not key.isalpha():
            errs.append(f"term key '{key}' must be letters only")
    if errs:
        sys.exit("theme.toml: " + "; ".join(errs))


# ---------------------------------------------------------------- LaTeX

def tex(s):
    s = str(s)
    for a, b in (("\\", r"\textbackslash{}"), ("&", r"\&"), ("%", r"\%"), ("$", r"\$"),
                 ("#", r"\#"), ("_", r"\_"), ("{", r"\{"), ("}", r"\}"),
                 ("~", r"\textasciitilde{}"), ("^", r"\textasciicircum{}"), ("¥", r"\textyen{}")):
        s = s.replace(a, b)
    # "quoted" -> ``quoted''
    out, open_q = [], True
    for ch in s:
        if ch == '"':
            out.append("``" if open_q else "''")
            open_q = not open_q
        else:
            out.append(ch)
    return "".join(out)


def cap(s):
    return s[:1].upper() + s[1:]


def setcs(name, value):
    return f"\\expandafter\\def\\csname {name}\\endcsname{{{value}}}"


def gen_tex(t):
    L = ["% GENERATED from theme/theme.toml by tools/gen_theme.py -- do not edit.", ""]
    for k, v in t["game"].items():
        L.append(setcs(f"th@game@{k}", tex(v)))
    for k, v in t["rules"].items():
        L.append(setcs(f"th@rule@{k}", f"{v:,}" if isinstance(v, int) else tex(v)))
    for k, v in t["glyphs"].items():
        L.append(setcs(f"th@glyph@{k}", tex(v)))
    for k, v in t["terms"].items():
        s = v["s"]
        p = v.get("p", s + "s")
        L.append(setcs(f"th@v@{k}", tex(s)))
        L.append(setcs(f"th@vp@{k}", tex(p)))
        L.append(setcs(f"th@vc@{k}", tex(cap(s))))
        L.append(setcs(f"th@vcp@{k}", tex(cap(p))))
    for i, s in enumerate(t["syndicate"]):
        L.append(f"\\definecolor{{syn{i}}}{{HTML}}{{{s['color']}}}")
        for f in ("name", "short", "letter", "coin", "tier", "alias", "flavor"):
            L.append(setcs(f"th@syn@{i}@{f}", tex(cap(s[f]) if f == "tier" else s[f])))
    for tier in TIERS:
        coins = [f"\\textcolor{{syn{i}}}{{\\textbf{{{tex(s['letter'])}}}}}"
                 for i, s in enumerate(t["syndicate"]) if s["tier"] == tier]
        L.append(setcs(f"th@tiercoins@{tier}", " ".join(coins)))
    rows = []
    for i, s in enumerate(t["syndicate"]):
        rows.append(f"\\cellcolor{{syn{i}}}\\textcolor{{white}}{{\\textbf{{{tex(s['letter'])}}}}} & "
                    f"{tex(s['name'])} & {tex(cap(s['tier']))} & "
                    f"{tex(s['flavor'])} \\\\")
    L.append("\\newcommand{\\SynTableRows}{%\n" + "\n".join(rows) + "}")
    for i, b in enumerate(t["bots"]["names"]):
        L.append(setcs(f"th@bot@{i}", tex(b)))
    # Messages as the players see them: one macro argument per placeholder, in order of first use.
    for sect in ("log", "status", "errors"):
        for k, v in t[sect].items():
            parts, names, out = re.split(r"\{(\w+)\}", v), [], ""
            for i, part in enumerate(parts):
                if i % 2 == 0:
                    out += tex(part)
                else:
                    if part not in names:
                        names.append(part)
                    out += f"#{names.index(part) + 1}"
            params = "".join(f"#{i + 1}" for i in range(len(names)))
            L.append(f"\\expandafter\\def\\csname th@{sect}@{k}\\endcsname{params}{{{out}}}")
    L.append("")
    return "\n".join(L)


# ---------------------------------------------------------------- C (8-bit client)

def wire(s):
    """Client text: uppercase ASCII $20-$5F, the currency sign as $5C, as a C string literal."""
    s = str(s).upper().replace("¥", "\\")
    bad = [ch for ch in s if not 0x20 <= ord(ch) <= 0x5F]
    if bad:
        sys.exit(f"theme.toml: {s!r} has characters the client can't show: {bad}")
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cname(k):
    return k.upper().replace("-", "_")


def gen_c(t):
    syns = t["syndicate"]
    L = ["/* GENERATED from theme/theme.toml by tools/gen_theme.py -- do not edit.",
         " * Strings are in the wire character set: uppercase ASCII $20-$5F, \\ is the currency sign. */",
         "#ifndef THEME_TAB_H", "#define THEME_TAB_H", ""]
    for k, v in t["game"].items():
        L.append(f"#define TH_{cname(k)} {wire(v)}")
    L.append("")
    for k, v in t["rules"].items():
        L.append(f"#define RULE_{cname(k)} {v}")
    L.append("")
    for k, v in t["glyphs"].items():
        L.append(f"#define GLYPH_{cname(k)} '{v}'")
    L.append("")
    for k, v in t["terms"].items():
        s = v["s"]
        L.append(f"#define T_{cname(k)} {wire(s)}")
        L.append(f"#define T_{cname(k)}_P {wire(v.get('p', s + 's'))}")
    L += ["", f"#define NSYN {len(syns)}",
          f"#define SYN_LETTERS {wire(''.join(s['letter'] for s in syns))}",
          "#define SYN_NAMES { /* for the screen: `screen`, else `name` */ \\"]
    for s in syns:
        L.append(f"    {wire(s.get('screen', s['name']))}, \\")
    L += ["}", "#define SYN_SHORTS { /* `short`, for a narrow screen's hive panel (nes) */ \\"]
    for s in syns:
        L.append(f"    {wire(s['short'])}, \\")
    L += ["}", "", "#endif", ""]
    return "\n".join(L)


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("check", "tex", "c"):
        sys.exit(__doc__)
    t = load()
    if sys.argv[1] == "tex":
        sys.stdout.write(gen_tex(t))
    elif sys.argv[1] == "c":
        sys.stdout.write(gen_c(t))


if __name__ == "__main__":
    main()
