#!/usr/bin/env python3
"""Generate the desktop client's Region menu from the servers fly.io runs, so the two can't drift.

Usage:
    gen_regions.py h      print C header (desktop/src/regions_tab.h)

Each [[server]] in server/servers.fly.toml is one entry: its region, its pace (description) and its port, all
dialled at the [lobby] address, the public host name. REGION_SEATS_PORT is where that host answers GET /SEATS
(the seats taken at each server) in plain HTTP: the fly.toml port, without TLS, of the service for [web]."""
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLY = ROOT / "server" / "servers.fly.toml"
FLY_APP = ROOT / "fly.toml"


def seats_port(web_port):
    for service in tomllib.loads(FLY_APP.read_text()).get("services", []):
        if service.get("internal_port") == web_port:
            for p in service.get("ports", []):
                if p.get("handlers") == ["http"] and not p.get("force_https"):
                    return p["port"]
    sys.exit("fly.toml: no plain http port for the [web] service")


def gen_h():
    conf = tomllib.loads(FLY.read_text())
    host = conf["lobby"]["address"]
    L = ["/* GENERATED from server/servers.fly.toml and fly.toml by tools/gen_regions.py -- do not edit. */",
         "#ifndef REGIONS_TAB_H", "#define REGIONS_TAB_H", "",
         f'#define REGION_HOST "{host}"',
         f"#define REGION_SEATS_PORT {seats_port(conf['web']['port'])}",
         f"#define NREGIONS {len(conf['server'])}", "",
         "struct region_entry { const char *name, *pace; unsigned short port; };",
         "static const struct region_entry regions[NREGIONS] = {"]
    for s in conf["server"]:
        L.append(f'    {{ "{s["region"]}", "{s.get("description", "")}", {s["port"]} }},')
    L += ["};", "", "#endif", ""]
    return "\n".join(L)


if __name__ == "__main__":
    if sys.argv[1:] != ["h"]:
        sys.exit(__doc__)
    sys.stdout.write(gen_h())
