"""Several servers in one process: servers.toml, checked strictly, and the servers it starts.
Run from server/:  python3 -m unittest -v"""
import argparse
import asyncio
import json
import pathlib
import subprocess
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import testclient
from acquire import proto, theme
from serve import list_all, load_config, lobby_client_arg, lobby_name, start, stop

HOST = "127.0.0.1"
HERE = pathlib.Path(__file__).resolve().parent.parent
REGION = "NEO-KANSAI-1"


def config(servers, lob=""):
    return lob + "".join(f"[[server]]\n{s}\n" for s in servers)


class ConfigTest(unittest.TestCase):
    def test_example_loads(self):
        specs, lob, host, _ = load_config((HERE / "servers.example.toml").read_text(), REGION)
        self.assertEqual([s.port for s in specs], [6502, 6503, 6504, 6505])
        self.assertEqual(lobby_name(specs[2].args["region"], specs[2].description), "OSAKA-SPRAWL - relaxed pace")
        self.assertEqual(specs[2].args["think"], {"P": (20.0, 10.0), "B": (12.0, 5.0)})
        self.assertEqual(specs[2].args["timeout"], 90.0)
        self.assertIsNone(lob)
        self.assertEqual(host, "0.0.0.0")

    def test_lobby_section_gives_each_server_its_address(self):
        specs, lob, _, _ = load_config(config(['port = 6502\nregion = "A"', 'port = 6503\nregion = "B"'],
                                           '[lobby]\naddress = "wire.example.com"\n'
                                           'client = "TNFS://t.example.com/syn-ack-wire-atari.atr"\n'), REGION)
        self.assertEqual([s.lobby_url for s in specs], ["TCP://wire.example.com:6502/", "TCP://wire.example.com:6503/"])
        # `client`: the Atari disk
        self.assertEqual(lob["clients"], {"atari": "TNFS://t.example.com/syn-ack-wire-atari.atr"})
        self.assertEqual(lob["key"], 11)
        self.assertEqual(lob["url"], "https://lobby.fujinet.online")

    def test_lobby_client_option(self):
        self.assertEqual(lobby_client_arg("TNFS://t.example.com/s.atr"), ("atari", "TNFS://t.example.com/s.atr"))
        self.assertEqual(lobby_client_arg("apple2=TNFS://t.example.com/s.po"), ("apple2", "TNFS://t.example.com/s.po"))
        with self.assertRaises(argparse.ArgumentTypeError):
            lobby_client_arg("Apple=TNFS://t.example.com/s.po")

    def test_lobby_clients_one_disk_a_platform(self):
        _, lob, _, _ = load_config(config(['port = 6502\nregion = "A"'],
                                          '[lobby]\naddress = "wire.example.com"\nclient = "TNFS://t.example.com/s.atr"\n'
                                          'clients = { apple2 = "TNFS://t.example.com/s.po" }\n'), REGION)
        self.assertEqual(lob["clients"], {"atari": "TNFS://t.example.com/s.atr", "apple2": "TNFS://t.example.com/s.po"})

    def test_proxy_protocol_reaches_every_server(self):
        specs, _, _, _ = load_config("proxy-protocol = true\n" + config(['port = 6502\nregion = "A"',
                                                                        'port = 6503\nregion = "B"']), REGION)
        self.assertEqual([s.args.get("proxy_protocol") for s in specs], [True, True])

    def test_mistakes_are_named(self):
        cases = {
            config(['port = 6502\nregion = "A"\ntimout = 9']): "unknown setting 'timout'",
            config(['port = 6502\nregion = "A"', 'port = 6502\nregion = "B"']): "used twice",
            config(['port = 6502\nregion = "A"', 'port = 6503\nregion = "a"']): "region A is also server 1's",
            config(['port = 6502', 'port = 6503']): f"region {REGION} is also",          # both on the default
            config(['port = 6502\nregion = "A"\ntimeout = 300']): "timeout: need a number of seconds from 1 to 255",
            config(['port = 6502\nregion = "A"\nthink-buy = "5"']): "think-buy",
            config(['port = 6502\nregion = "NEO-KANSAI-1"\ndescription = "a much too long description"']):
                "the lobby shows at most 32",
            config(['port = 6502\nregion = "A"'], '[lobby]\nclient = "TNFS://t.example.com/s.atr"\n'): "needs address",
            config(['port = 6502\nregion = "A"'], '[lobby]\naddress = "w.example.com"\n'): "needs clients",
            config(['port = 6502\nregion = "A"'], '[lobby]\naddress = "w.example.com"\n'
                   'clients = { "Apple II" = "TNFS://t.example.com/s.po" }\n'): "platform name",
            config(['port = 6502\nregion = "A"'], '[lobby]\naddress = "w.example.com"\nclient = "TNFS://t.example.com/s.atr"\n'
                   'clients = { atari = "TNFS://t.example.com/s.atr" }\n'): "both name the Atari disk",
            config(['port = 6502\nregion = "A"\njack-in = "no"']): "need true or false",
            "": "needs at least one [[server]]",
            '[web]\nport = 8080\n' + config(['port = 6502\nregion = "A"']): "[web]: needs manual",
            '[web]\nport = 8080\nmanual = "https://x.example/manual.pdf"\n' + config(['port = 6502\nregion = "A"']):
                "with {platform} in it",
            '[web]\nport = 6502\nmanual = "https://x/{platform}"\n' + config(['port = 6502\nregion = "A"']):
                "port 6502 is also a server's",
            "port = 6502\n": "unknown setting 'port'",
        }
        for text, want in cases.items():
            with self.subTest(want=want):
                with self.assertRaises(ValueError) as cm:
                    load_config(text, REGION)
                self.assertIn(want, str(cm.exception))

    def test_fly_services_match_the_servers(self):
        """fly.toml forwards each server's port with the PROXY header, and the servers expect it."""
        import tomllib
        fly = tomllib.loads((HERE.parent / "fly.toml").read_text())
        specs, _, host, web = load_config((HERE / "servers.fly.toml").read_text(), REGION)
        http = [s for s in fly["services"] if any("http" in p.get("handlers", ()) for p in s["ports"])]
        self.assertEqual([s["internal_port"] for s in http], [web["port"]])       # the QR codes' short links
        self.assertEqual(sorted(p["port"] for p in http[0]["ports"]), [80, 443])
        services = {s["internal_port"]: s for s in fly["services"] if s["protocol"] == "tcp" and s not in http}
        self.assertEqual(sorted(services), sorted(s.port for s in specs))
        for port, s in services.items():
            self.assertEqual([(p["port"], p["handlers"]) for p in s["ports"]], [(port, ["proxy_proto"])])
        self.assertTrue(all(s.args.get("proxy_protocol") for s in specs))
        self.assertEqual(host, "0.0.0.0")
        udp = [s for s in fly["services"] if s["protocol"] == "udp"]       # tnfsd, for the lobby: plain UDP
        self.assertEqual([(s["internal_port"], s["ports"]) for s in udp], [(16384, [{"port": 16384}])])

    def test_config_replaces_the_other_options(self):
        r = subprocess.run([sys.executable, "serve.py", "--config", "servers.example.toml", "--timeout", "30"],
                           cwd=HERE, capture_output=True, text=True, timeout=30)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("drop --timeout", r.stderr)


class ServersTest(unittest.IsolatedAsyncioTestCase):
    async def test_each_server_has_its_settings_and_all_share_a_guard(self):
        specs, _, _, _ = load_config(config(['port = 0\nregion = "FAST-1"\nbot-delay = 0',
                                          'port = 0\nregion = "SLOW-2"\ntimeout = 90\nthink-buy = "12,5"']), REGION)
        servers, srvs = await start(specs, HOST)
        try:
            self.assertIs(servers[0].guard, servers[1].guard)
            self.assertEqual((servers[0].bot_delay, servers[1].timeout), (0.0, 90.0))
            self.assertEqual(servers[1].think, {"B": (12.0, 5.0)})
            for srv, region in zip(srvs, ("FAST-1", "SLOW-2")):
                r, w = await asyncio.open_connection(HOST, srv.sockets[0].getsockname()[1])
                w.write(proto.info(0))
                c = testclient.Client()
                while c.status is None:
                    c.feed(await asyncio.wait_for(r.read(4096), 5))
                self.assertEqual(c.region, region)              # each title screen names its own server
                w.close()
        finally:
            for srv in srvs:
                srv.close()
                srv.close_clients()
                await srv.wait_closed()

    async def test_stop_closes_games_at_once(self):
        """SIGTERM (a cloud host redeploying, say) must not wait for the games to end."""
        specs, _, _, _ = load_config(config(['port = 0\nregion = "A"', 'port = 0\nregion = "B"']), REGION)
        servers, srvs = await start(specs, HOST)
        r, w = await asyncio.open_connection(HOST, srvs[0].sockets[0].getsockname()[1])
        w.write(proto.hello(0, bytes(4), "STAYER"))
        c = testclient.Client()
        while c.prompt is None:
            c.feed(await asyncio.wait_for(r.read(4096), 5))
        await asyncio.wait_for(stop(servers, srvs), 5)
        try:
            while await asyncio.wait_for(r.read(4096), 5):
                pass                                        # the connection ends, rather than waiting
        except ConnectionError:
            pass
        self.assertEqual(servers[0].writers, set())
        w.close()

    async def test_lobby_entries_name_region_and_description(self):
        posts = []

        class FakeLobby(BaseHTTPRequestHandler):
            def do_POST(self):
                posts.append(json.loads(self.rfile.read(int(self.headers["Content-Length"]))))
                self.send_response(201)
                self.end_headers()

            def do_DELETE(self):
                self.rfile.read(int(self.headers["Content-Length"]))
                self.send_response(204)
                self.end_headers()

            def log_message(self, *args):
                pass

        http = ThreadingHTTPServer((HOST, 0), FakeLobby)
        threading.Thread(target=http.serve_forever, daemon=True).start()
        specs, lob, _, _ = load_config(config(['port = 0\nregion = "OSAKA-SPRAWL"\ndescription = "relaxed pace"',
                                            'port = 0\nregion = "NEO-KANSAI-1"\ndescription = "fast pace"'],
                                           f'[lobby]\naddress = "wire.example.com"\n'
                                           f'clients = {{ atari = "TNFS://t.example.com/syn-ack-wire-atari.atr", '
                                           f'apple2 = "TNFS://t.example.com/syn-ack-wire-apple2.po" }}\n'
                                           f'url = "http://{HOST}:{http.server_port}"\n'), REGION)
        servers, srvs = await start(specs, HOST)
        try:
            list_all(servers, specs, lob)
            for _ in range(100):
                if len({p["server"] for p in posts}) == 2:
                    break
                await asyncio.sleep(0.05)
            self.assertEqual(sorted({p["server"] for p in posts}), ["NEO-KANSAI-1 - fast pace",
                                                                    "OSAKA-SPRAWL - relaxed pace"])
            self.assertEqual({p["serverurl"] for p in posts}, {"TCP://wire.example.com:0/"})   # port 0 in a test
            self.assertEqual({p["game"] for p in posts}, {theme.load().game["name"]})       # mixed case, like the rest
            self.assertEqual(posts[0]["clients"],
                             [{"platform": "atari", "url": "TNFS://t.example.com/syn-ack-wire-atari.atr"},
                              {"platform": "apple2", "url": "TNFS://t.example.com/syn-ack-wire-apple2.po"}])
            await asyncio.gather(*(s.lobby.close() for s in servers))
        finally:
            http.shutdown()
            for srv in srvs:
                srv.close()
                srv.close_clients()
                await srv.wait_closed()


if __name__ == "__main__":
    unittest.main()
