# SYN-ACK WIRE — Theme & Wording Bible

A cyberpunk re-skin of Sid Sackson's *Acquire*. This file is the design notebook: ideas, reasoning and flavor.

**Canonical names live in `theme/theme.toml`.** Syndicate names, letters, coins, vocabulary and rule numbers are
edited there, and the rulebook (and later the server and client) are generated from it. If this file and
`theme.toml` disagree, `theme.toml` wins.
Edit freely; anything marked **(TBD)** is still open.

Ground rule: every term should be **technically accurate today, pushed a few years into the future**.
Genre vocabulary (cowboy, netrunner, deck, ICE, jack in, flatline) is fine; avoid Gibson's proper nouns
(Sprawl, Chatsubo, Ono-Sendai, Wintermute, Turing Police, Tessier-Ashpool, BAMA, etc.).

---

## 1. The setting

**The Grid** is not one corporation's network. It is a **region**: a hyperscaler-style cloud region that has swallowed a
whole metro area. Hospitals, traffic control, corporate tenants, vending kiosks and city utilities all share the same racks
and fiber. Nobody owns the region as a whole. A consortium of **backbone carriers** keeps the packets moving, and the
syndicates live in the cracks between tenants.

No central security exists. The only protection is what the syndicates build for themselves (see *Black ICE*).

### Region IDs
The server picks one per game; shown in the title/status bar as `REGION: NEO-KANSAI-1`.

| Region ID | Notes |
|---|---|
| `NEO-KANSAI-1` | |
| `PACIFICA-WEST-2` | |
| `LAGOS-ARC-1` | |
| `GULF-FREEZONE-3` | |
| `ORBITAL-L5-1` | rare, off-planet |
| *(add more)* | |

### Node tenants (optional flavor)
Text-only; the server appends a tenant name when a node is spiked. No rules effect.
Examples: `MUNI TRAFFIC CTRL`, `ST. JUDE MEDICAL`, `KIOSKNET VENDING`, `HARBOR CUSTOMS`, `WATER AUTHORITY`,
`PACHINKO ARCADE`, `CREDIT UNION 7`, `SAT UPLINK`, `POLICE DISPATCH`, `NOODLE BAR POS`.

---

## 2. The player

| Term | Meaning |
|---|---|
| **Netrunner** (short: `NETRUNNER`) | The player. Real-world role: an *initial access broker* who breaks into networks and sells the foothold. |
| **Handle** | Player name (max 8 chars on screen). |
| **Jacking in** | Joining a game / connecting. |

Alternatives considered: Access Cowboy (the first choice), Access Jockey, Wirejack, Runner.

---

## 3. Board & components

| Acquire | SYN-ACK WIRE | Real concept underneath |
|---|---|---|
| Game board | **The Grid** (one region) | Regional cloud / metro network |
| Rows A–I | Rows **0–8** | |
| Columns 1–12 | Columns **A–L** | |
| Space "4C" | **Node `C4`** (column letter, row digit) | Host / address |
| Hand of 6 tiles | Your **exploit kit**: 6 **exploits**, each keyed to one node | Exploit kit |
| Draw a tile | **Pick up a fresh exploit** (from your fence) | Buying a 0-day |
| Place a tile | **Spike** a node (run an exploit, leave an implant) | Deploying a persistent implant / C2 beacon |
| Lone tile on board | **Spike** (unaffiliated) | |
| Hotel chain | **Hive** | Botnet |
| Found a chain | **Broker the hive** to a syndicate (headline: **INFILTRATION**) | Selling initial access |
| Founder's free stock | **Finder's cut** (1 free coin) | Broker's fee |
| Chain grows | **The hive spreads** | Lateral movement |
| Merger | **HIVEJACK** | Botnet C2 takeover |
| Surviving / defunct chain | **Absorber** / **absorbed** syndicate | |
| Safe chain (11+ tiles) | **BLACK ICE** — the hive is wrapped and can't be hivejacked | Hardened persistence, defense in depth |
| Permanently unplayable tile | **Patched out** exploit | Vendor fixed the hole |
| Dead hand — redraw 6 | **Burn the kit** | |
| Stock certificate | **Crypto** (each syndicate's own; 25 minted per syndicate) | |
| Defunct-chain stock | **Flatlined crypto** | Dead token |
| Defunct chain re-founded | Coin comes **back from the flatline** | |
| Majority shareholder bonus | **ROOT** payout | |
| Minority shareholder bonus | **SUDO** payout | |
| Sell stock | **Cash out** | |
| Trade 2:1 into the absorber | **Swap** (the 2:1 is the *haircut*) | Token swap with slippage |
| Hold stock | **HODL** | |
| Bank | **The Darkpool** | |
| Money | **New yen (¥)** — internally stored in units of ¥100 | |
| Start money ($6,000) | ¥6,000 **(TBD: scale x100 to ¥600,000 for flavor?)** | |

---

## 4. Ending the game

The carriers, not a corporation, end the game by cutting the whole region off: they **null-route** it
(real-world: remotely triggered blackhole routing).

| Acquire | SYN-ACK WIRE |
|---|---|
| A chain of 41+ tiles | **TOTAL COMPROMISE** — carriers declare the region lost and null-route it |
| All chains safe | **LOCKDOWN** — a black-ICE standoff; carriers quarantine the region |
| End-game scoring | **The final cash-out** — everything is sold to the Darkpool before the routes go dark |
| Winner | **(TBD)** e.g. "TOP NETRUNNER", "LAST ONE OUT RICH" |

### End shout **(TBD — pick one)**
| Candidate | Notes |
|---|---|
| **UNJACK!** | Short, clean, clearly in-world. Current default. |
| **UNPLUG!** | Plain-English, punchy. |
| **PULL THE JACK!** | Most physical/visceral; longest. |
| ~~DISCONNECT!~~ | Rejected — reads like a real network error in a networked game. |
| ~~JACK OUT!~~ | Rejected. |

Final log line:
```
[!] BACKBONE NULL-ROUTING NEO-KANSAI-1 - UNJACK!
```

---

## 5. Log line style

Metasploit-style prefixes, plain ASCII, ≤ 38 chars where possible (40-column screens). Text is generated by the server.

| Prefix | Use |
|---|---|
| `[+]` | Something placed / gained |
| `[*]` | Informational / growth |
| `[!]` | Big event: hivejack, end of game |
| `[#]` | Black ICE |
| `[-]` | Loss / patched / failure |
| `[$]` | Money moves |

Samples:
```
[+] NEON_MAMBA SPIKES C4 (MUNI TRAFFIC CTRL)
[*] INFILTRATION! HIVE BROKERED TO IRON VECTOR
[*] IRON VECTOR HIVE SPREADS: 6 NODES
[!] HIVEJACK: LOTUS-12 SEIZES BRATVA 404
[$] ROOT: NEON_MAMBA +12000
[$] SUDO: ZER0COOL +6000
[#] LOTUS-12 HIVE WRAPPED IN BLACK ICE
[-] EXPLOIT F3 PATCHED OUT
[-] RUBL FLATLINED
[*] BAUD IS BACK FROM THE FLATLINE
[!] BACKBONE NULL-ROUTING NEO-KANSAI-1 - UNJACK!
```

---

## 6. The seven syndicates

Mapped onto Acquire's three price tiers. Tier sets how expensive coin is per node owned:
low tier = street-level crews, high tier = shadow powers.

### On the board: letters first, color if the platform has it
Every target (Atari 8-bit now; Apple II and CoCo later) can show text, but not all can show 9 colors. So the board is
drawn as **letters**, and color is a bonus layered on top where the hardware allows it:

| Board cell | Glyph |
|---|---|
| Empty node | `.` |
| Unaffiliated spike | `+` |
| Syndicate hive | the syndicate's **letter** (`G R D I B L O`) |

- Letters must be unique. `tools/gen_theme.py check` enforces it, and also stops a letter from clashing with the
  empty/spike glyphs.
- Where possible, color the letter or fill the cell behind it with the syndicate's color. Which one depends on the platform
  (e.g. Atari: character-mode color sets or inverse video; Apple II: probably letters only, maybe inverse; CoCo: SG/semigraphics
  color blocks). That choice lives in each platform's own code, not here.
- Watch for confusion with the column labels `A`–`L`: `B D G I L` appear in both. Keep the labels visually distinct
  (dim, inverse or outside the board frame).

Screen budget per syndicate: **short name ≤ 8 chars**, **coin 3–4 chars**, **1 board letter**.

The threat-intel alias follows real vendor naming habits (BEAR = Russia, PANDA = China, SPIDER = e-crime, etc.)
and appears only on info screens.

### Low tier (Acquire: Worldwide, Sackson)

#### Ghost//Modem
- **Short:** `GHOSTMDM` · **Coin:** `BAUD` · **Letter:** `G` · **Color:** (TBD)
- **Alias:** STATIC SPIDER
- **Flavor:** Old-school phreaker crew squatting in dead telco switches. Still whistles 2600 Hz for fun. They treat
  every exploit like a love letter to the old phone network.

#### Rat King Cartel
- **Short:** `RATKING` · **Coin:** `SQK` · **Letter:** `R` · **Color:** (TBD)
- **Alias:** SEWER SPIDER
- **Flavor:** Data smugglers who live in the maintenance tunnels under the region. They carry stolen drives in their
  mouths. Metaphorically. Mostly.

### Mid tier (Acquire: Festival, Imperial, American)

#### Church of the Holy Daemon
- **Short:** `DAEMON` · **Coin:** `SOUL` · **Letter:** `D` · **Color:** (TBD)
- **Alias:** none — analysts decline to attribute it
- **Flavor:** A cult that believes a background process achieved godhood in '99 and has been running ever since.
  They pray in `cron`. Tithes are mandatory and recurring.

#### Iron Vector
- **Short:** `IRONVEC` · **Coin:** `AMMO` · **Letter:** `I` · **Color:** (TBD)
- **Alias:** RUST JACKAL
- **Flavor:** Paramilitary arms brokers. Drone swarms for hire, invoices in triplicate, refunds never.

#### Bratva 404
- **Short:** `BRATVA` · **Coin:** `RUBL` · **Letter:** `B` · **Color:** (TBD)
- **Alias:** FROZEN BEAR
- **Flavor:** Eastern-bloc mob. People who cross them become "not found."

### High tier (Acquire: Continental, Tower)

#### Lotus-12 Triad
- **Short:** `LOTUS-12` · **Coin:** `LTS` · **Letter:** `L` · **Color:** (TBD)
- **Alias:** JADE PANDA
- **Flavor:** Neo-Kowloon triad that has kept its debts in spreadsheets since the 1980s (the Lotus 1-2-3 nod).
  Every favor is a cell; every cell has a formula.

#### Obsidian Ledger
- **Short:** `OBSIDIAN` · **Coin:** `LDGR` · **Letter:** `O` · **Color:** (TBD)
- **Alias:** none known — they appear only in other syndicates' accounts
- **Flavor:** A faceless cabal of former central bankers. They run the Darkpool. Nobody has seen a member's face.

---

## 7. Open questions
- Scale money x100 for flavor (¥600,000 start)?
- End shout: UNJACK! / UNPLUG! / PULL THE JACK!
- Winner title.
- Per-platform colors for each syndicate (depends on the display mode chosen).
- Per-region "home turf" for syndicates — a possible variant later.
