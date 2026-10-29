#!/usr/bin/env python3
"""IntyBASIC sources with the C headers' macros filled in, for the Intellivision client (src/intv/).

IntyBASIC has no preprocessor and no string constants, so the theme's words, the rules' numbers and the
protocol's codes reach the .bas files through this: every {NAME} becomes the value of NAME, a #define in
one of the headers (theme_tab.h, proto_tab.h: generated from theme.toml and proto.py) or a -D from the
Makefile. A string's text goes in without its quotes, a character constant as its code, a {...} list
as its items, names among them resolved, joined by commas. {#NAME} is the length of NAME's text.

Comments are left as they are. Lines from '#if NAME to '#else or '#endif (IntyBASIC comments, so a source still reads as BASIC) are
kept only if NAME is set and not 0; '#else's lines only if not.

A line ending in 'fit N must have at most N characters in its string literals, once filled in: a
renamed term that no longer fits its place on the 20-column screen fails the build, not the screen.

Usage:
    bas_pp.py [-H header.h]... [-D NAME=VALUE]... IN.bas OUT.bas
"""
import re
import sys

DEFINE = re.compile(r"#define\s+(\w+)\s+(.*)$")


def c_string(text):
    out, i = [], 0
    while i < len(text):
        ch = text[i]
        if ch == "\\":
            i += 1
            ch = text[i]
        out.append(ch)
        i += 1
    return "".join(out)


def read_header(path, macros):
    lines = open(path).read().split("\n")
    i = 0
    while i < len(lines):
        m = DEFINE.match(lines[i].strip())
        i += 1
        if not m:
            continue
        name, value = m.group(1), m.group(2)
        while value.endswith("\\"):             # continued
            value = value[:-1] + " " + lines[i].strip()
            i += 1
        value = re.sub(r"/\*.*?\*/", "", value).strip()
        if value.startswith("{"):
            macros[name] = [v.strip() for v in value.strip("{} ").split(",") if v.strip()]
        elif value.startswith('"'):
            macros[name] = c_string(re.fullmatch(r'"(.*)"', value).group(1))
        elif value.startswith("'"):
            macros[name] = ord(c_string(value[1:-1]))
        else:
            macros[name] = value


def resolve(name, macros, where):
    if name not in macros:
        sys.exit("%s: {%s} is not defined" % (where, name))
    v = macros[name]
    if isinstance(v, list):
        return ", ".join(str(resolve(x, macros, where)) if x in macros else str(int(x, 0)) for x in v)
    if isinstance(v, str) and v in macros and v != name:
        return resolve(v, macros, where)
    if isinstance(v, str) and re.fullmatch(r"0[xX][0-9a-fA-F]+", v):
        return int(v, 16)                       # IntyBASIC writes hex as $FE
    return v


def truthy(name, macros):
    v = macros.get(name)
    return v not in (None, "", "0", 0)


def fill(line, macros, where):
    def one(m):
        if m.group(1) == "#":
            v = resolve(m.group(2), macros, where)
            return str(len(v))
        v = str(resolve(m.group(2), macros, where))
        if '"' in v or "\\" in v:
            sys.exit('%s: {%s} has a " or a \\, which IntyBASIC strings can\'t hold' % (where, m.group(2)))
        return v
    return re.sub(r"\{(#?)(\w+)\}", one, line)


def split_comment(line):
    """The code, and the comment after it: from the first ' outside a string (comments are left as they are)."""
    quoted = False
    for i, ch in enumerate(line):
        if ch == '"':
            quoted = not quoted
        elif ch == "'" and not quoted:
            return line[:i], line[i:]
    return line, ""


def main():
    args = sys.argv[1:]
    macros = {}
    while args and args[0] in ("-H", "-D"):
        if args[0] == "-H":
            read_header(args[1], macros)
        else:
            name, _, value = args[1].partition("=")
            macros[name] = value[1:-1] if value[:1] == '"' else value
        args = args[2:]
    if len(args) != 2:
        sys.exit(__doc__)
    src, out = args
    keep = [True]
    result = []
    for n, line in enumerate(open(src).read().split("\n"), 1):
        where = "%s:%d" % (src, n)
        cond = re.match(r"\s*'#(if|else|endif)\b\s*(\w*)", line)
        if cond:
            if cond.group(1) == "if":
                keep.append(keep[-1] and truthy(cond.group(2), macros))
            elif cond.group(1) == "else":
                if len(keep) < 2:
                    sys.exit("%s: '#else without '#if" % where)
                keep[-1] = keep[-2] and not keep[-1]
            else:
                if len(keep) < 2:
                    sys.exit("%s: '#endif without '#if" % where)
                keep.pop()
            result.append("")                   # the line numbers stay the source's
            continue
        if not keep[-1]:
            result.append("")
            continue
        code, comment = split_comment(line)
        line = fill(code, macros, where) + comment
        fit = re.search(r"'fit\s+(\d+)\s*$", line)
        if fit:
            text = "".join(re.findall(r'"([^"]*)"', line[:fit.start()]))
            if len(text) > int(fit.group(1)):
                sys.exit('%s: "%s" is %d characters, its place %s' % (where, text, len(text), fit.group(1)))
        result.append(line)
    if len(keep) != 1:
        sys.exit("%s: '#if without '#endif" % src)
    with open(out, "w") as f:
        f.write("\n".join(result))


if __name__ == "__main__":
    main()
