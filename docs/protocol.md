# SYN-ACK WIRE — Wire Protocol (v3, draft)

Binary protocol between the 8-bit client (via FujiNet `N:TCP://host:port/`) and the Python server.
Goals, in order: **tiny client code**, **few bytes on the wire**, **few SIO transactions**.

The server is authoritative. It enforces every rule, runs the bots, and tells the client exactly what to draw.
The client stores state it is given, draws it, and answers **prompts**. It never computes legality, prices, payouts or
mergers.

---

## 1. Conventions

| Item | Encoding |
|---|---|
| Byte order | Little-endian (6502 native) |
| Message type | One ASCII letter. **Client → server: UPPERCASE. Server → client: lowercase.** Readable in a hex dump or `nc`. |
| Node | 1 byte, `row*12 + col`, 0–107. `C4` = 4*12+2 = 50 (`$32`). `$FF` = none. |
| Syndicate id | 1 byte, 0–6, in tier order (table below). 7 = none. |
| Seat | 1 byte, 0–5. |
| Money | 2 bytes, unsigned, in units of **¥100**. ¥6,000 = `$3C $00`. |
| Coin count | 1 byte, 0–25. |
| Price | 1 byte, in ¥100 units (2–12). 0 = syndicate not on the Grid. |
| Text | ASCII `$20–$5F` only (uppercase, digits, punctuation). `$5C` (`\`) is drawn as **¥** by the client font. |

### Syndicate ids
Ids are the order of the `[[syndicate]]` entries in `theme/theme.toml`; names, letters and coins come from there.
The protocol only ever carries the id, so renaming a syndicate never changes the protocol.

| Id | Letter | Tier |
|---|---|---|
| 0 | G | low |
| 1 | R | low |
| 2 | D | mid |
| 3 | I | mid |
| 4 | B | mid |
| 5 | L | high |
| 6 | O | high |

### Board cell values (4 bits)
| Value | Meaning |
|---|---|
| 0 | Empty node |
| 1 | Unaffiliated spike |
| 2–8 | Hive of syndicate (value − 2) |

---

## 2. Framing

TCP gives ordering and delivery, so there are no sequence numbers, checksums or acks.

- **Fixed-length messages:** `[type][payload]`. Each side knows the payload length from the type.
- **Variable-length messages:** `[type][len][payload]`, where `len` counts payload bytes only.

Only four server types are variable: `a`, `l`, `p`, `x`. Every client message is fixed-length.
The client parser therefore needs one small lookup table: payload length per type, with a marker for "variable".

Largest frame is the snapshot `s` at 117 bytes. Every frame fits in 128 bytes.

**Client receive buffer (recommendation):** a 256-byte, page-aligned ring buffer indexed by an 8-bit pointer, so
wrap-around is free. Each poll, ask FujiNet for bytes waiting and read *all* of them in one SIO call, then parse as many
complete frames as are in the buffer. Frames can arrive split across reads; leave a partial frame in place until the rest
arrives. The SIO transaction count matters far more than the byte count.

---

## 3. Client → server (UPPERCASE, all fixed length)

| Type | Payload | Bytes | Meaning |
|---|---|---|---|
| `I` | ver, table | 3 | Before `H` only: watch the table's status from the title screen. The server answers `a` (the region) and `i`, and sends a new `i` whenever it changes, until `H`. A wrong `ver` gets no answer (its `H` gets fault 1). |
| `H` | ver, table, token[4], handle[8] | 15 | Hello. `ver` = 3. `token` all zero = new player, unless `handle` matches a seat that is away (kind 2): that seat is reclaimed. Otherwise the token reclaims its seat. A new player joining a game in progress takes over a bot seat (kind 3) unless the server runs with `--no-jack-in`; with no bot seat it gets fault 2, and with a handle taken there (a player's or a bot's, in the lobby too) fault 5. After fault 2 or 5 the connection stays open, watching as after `I`, and `H` can be sent again. Handle is space-padded. |
| `S` | seats | 2 | Start the game (lobby only). `seats` = total seats 2–6; empty seats get bots. |
| `P` | node | 2 | Answer to prompt `P`: run the exploit for this node. |
| `F` | syn | 2 | Answer to prompt `F`: broker the new hive to this syndicate. |
| `K` | syn | 2 | Answer to prompt `K`: pick one syndicate from the offered set. |
| `D` | sell, swap | 3 | Answer to prompt `D`: cash out `sell` coins, swap `swap` coins (even number, 2:1). HODL the rest. |
| `B` | b0, b1, b2 | 4 | Answer to prompt `B`: up to three coin purchases, each a syndicate id or 7 for none. **Bit 7 of `b0`** = "call the null-route" (declare game end). |
| `R` | — | 1 | Resync: please resend the full state. |
| `Q` | — | 1 | Leave for good (the Atari's Esc, then Y). In a game, the seat's bot takes over at once under a bot's name, and everyone gets a log line (theme `logoff`); the token is forgotten, so an `H` with it gets fault 3, and the seat is a bot seat a newcomer can jack in for. In the lobby the seat is freed. The server then closes the connection. |

**Prompt answers use the same letter as the prompt kind.** The client echoes the kind letter as the message type, so
it needs no mapping table.

---

## 4. Server → client (lowercase)

| Type | Payload | Bytes | Sent to | Meaning |
|---|---|---|---|---|
| `w` | seat, token[4] | 6 | you | Welcome. Save `token` (e.g. in a FujiNet AppKey) to reclaim the seat later. |
| `n` | seat, kind, handle[8] | 11 | all | Seat info. `kind`: 0 empty, 1 human, 2 human away (bot driving), 3 bot. |
| `s` | see below | 117 | you | Full snapshot. |
| `h` | node[6] | 7 | you | Your exploit kit. `$FF` = empty slot. **Bit 7 set = can't be run right now** (it would found an 8th hive, or it is patched out). |
| `t` | seat | 2 | all | It is now this seat's turn. |
| `x` | len, val, node… | 3+n | all | Set these nodes to cell value `val`. Covers new spikes, founding and spreading. |
| `r` | from, to | 3 | all | Hivejack recolor: every node with cell value `from` becomes `to`. |
| `z` | syn, size | 3 | all | Hive size. 0 = syndicate is off the Grid. |
| `m` | seat, money[2] | 4 | all | Set a seat's cash. |
| `k` | seat, syn, count | 4 | all | Set a seat's coin holding. |
| `p` | len, kind, timeout, params… | 4+n | you | Prompt. See section 5. |
| `l` | len, text… | 2+n | all | Log line, ≤ 38 chars. Server-written flavor text. |
| `a` | len, text… | 2+n | all | Region name, e.g. `NEO-KANSAI-1`. |
| `i` | len, code, text… | 3+n | you | Table status, before joining (see `I`): what `H` would do now. `code`: 0 lobby, a seat for you; 1 game in progress, you'd take over a bot; 2 full (a dropped player can still rejoin by token or handle). `text` ≤ 28 chars, from theme `[status]`. |
| `e` | winners | 2 | all | Game over. `winners` = bitmask of seats tied for first. Final cash arrives as `m` messages before this. |
| `f` | code | 2 | you | Fault. 1 bad version, 2 table full, 3 bad token, 4 illegal answer (a fresh prompt follows), 5 that handle is taken at this table (a player's or a bot's). |
| `u` | — | 1 | you | Keepalive, every 10 s (`serve.py --keepalive`). Means nothing beyond "the link is up". |

### Snapshot `s` (116-byte payload)
| Offset | Size | Field |
|---|---|---|
| 0 | 54 | Board, two nodes per byte: node 2i in the **low** nibble, node 2i+1 in the **high** nibble |
| 54 | 7 | Hive size per syndicate |
| 61 | 12 | Cash per seat (6 × 2 bytes) |
| 73 | 42 | Coins per seat per syndicate (seat-major: seat 0 syn 0–6, seat 1 syn 0–6, …) |
| 115 | 1 | Seat whose turn it is (`$FF` = lobby / not started) |

Coin holdings are public (Acquire's rules don't hide them). A server option could send zeros for other seats to play
with hidden holdings; the format doesn't change.

---

## 5. Prompts (`p`)

`p` frame: `'p', len, kind, timeout, params…`
`timeout` = seconds before the seat's bot answers for you (0 = no limit). The client may show a countdown.

| Kind | Params | Reply | When |
|---|---|---|---|
| `P` | — | `P node` | Your turn: spike a node from your kit (`h`). |
| `F` | avail | `F syn` | Your spike founded a hive. `avail` = bitmask of syndicates you may broker to. |
| `K` | ctx, mask | `K syn` | Choose one from `mask`. `ctx` 0 = which syndicate survives a tied hivejack; 1 = which tied hive gets absorbed next. |
| `D` | absorbed, absorber, held, supply | `D sell swap` | Dispose of coin from a hivejacked syndicate. `supply` = absorber coins left in the Darkpool (caps swaps at 2×supply). |
| `B` | cash[2], flags, price[7], supply[7] | `B b0 b1 b2` | Buy up to 3 coins. `flags` bit 0 = you may call the null-route (end the game); bit 1 = nothing you can afford (only sent with bit 0: otherwise the turn passes without the prompt). |
| `S` | — | `S seats` | Lobby: you are the host; start when ready. Timeout 0 while you are alone; with others waiting it counts down, and at 0 the host role (and this prompt) passes to the next player. The others get an `l` naming the host and the time left, when the countdown starts and when they arrive. The host gets an `l` whenever the number of clients at the title screen (watching with `I`, not joined) changes, joining included. |

Prompts go only to the seat that must answer. Every other client just sees the results (`x`, `k`, `m`, `l`, …).

**Order of `D` prompts in a hivejack:** for each absorbed syndicate in turn, payouts (`m` + `l`) go out first, then a `D`
prompt to each holder in turn order, starting with the player who caused the hivejack. This matches the rulebook.

**Illegal answer:** the server sends the reason as an `l` log line to that player only (e.g. `[-] NOT ENOUGH YEN`),
then `f 4`, then the same prompt again. An answer that arrives after the shot clock ran out gets
`[-] TOO LATE: YOUR BOT ANSWERED` and `f 4`. The client needs no extra code: it already shows log lines.
A careful client rarely sees this, but the server never trusts the client. Reason wording lives in
`theme.toml` `[errors]`.

---

## 6. Session flow

### Title screen
```
C: I 03 tbl
S: i 00 "LOBBY OPEN: 1 WAITING"
S: i 01 "IN PROGRESS: TAKE OVER A BOT"   (whenever it changes)
```
Two players can see the same last bot seat; the first `H` to arrive gets it, the other gets fault 2 and a fresh `i`.
The Atari client remembers what the status showed when Return was pressed, so it can say it was too late.

### New player
```
C: H 03 tbl 00000000 "NEONMAMB"
S: w seat token[4]
S: a "NEO-KANSAI-1"
S: n …  (one per seat)
S: s …
S: (host only) p 'S'
```

### Reconnect
The client sends `H` with its saved token, or, having lost it, a zero token and the same handle (that works only
while the seat is away). The server replies `w`, `a`, `n`×6, `s`, `h`, `t`, then the pending `p` if
one is waiting for this seat. That is the entire resync; `R` triggers the same burst without reconnecting. After a game has ended, the burst
also carries `e` with the last game's winners.

### Bots, timeouts and dropouts
- Prompt timeout expires → the seat's bot answers *that one prompt*. The seat stays human. After the server's
  `--afk` timeouts in a row (default 2) the seat is announced as `kind 2` (away) but keeps getting prompts, with a
  shorter timeout; an answer in time announces it as `kind 1` again. Clients need nothing special: they show the
  kind they are sent and the clock in each `p`.
- TCP connection drops → the seat becomes `kind 2` (away) and the bot plays every prompt until the token holder returns.
- `Q` → the seat becomes `kind 3` (bot) for the rest of the game.
- No human connected → the game pauses until one reconnects. After the server's `--abandon` time (default 10
  minutes) the game is dropped and the table returns to an empty lobby; an old token then gets fault 3.
- Every connected human on autopilot for that long → the game is dropped with no `e`. Each connected client gets a
  lobby resync (`w` with its new seat number, `n`×6, an empty `s`, `S` for the host) and then an `l` saying why.
- A seat change is announced to everyone with `n`.

The server sends `u` every 10 seconds. A link can die without either end being told: the server's machine loses
power, or a router drops the session, and FujiNet still reports the connection as open. So the client counts silence
instead: after 20 seconds with no frame of any kind it shows NO SIGNAL, and at 30 it hangs up and reconnects with its
token. The server finds dead connections when its keepalives go unacknowledged for its `--link-timeout` (60 s by
default; it sets `TCP_USER_TIMEOUT`), and the prompt timeout covers the turn in the meantime. v2 added `u`; a v1 client is refused with fault 1. v3 added `I`, `i` and fault 5, and keeps the
connection open after fault 2.

### Kit management (server-side, no protocol impact)
- A **patched out** exploit (it could only join two black-ICE hives) is shown with bit 7 set in `h`.
  Whether it is replaced at end of turn is a server rule option.
- If none of your exploits can run, the server **burns the kit**: it deals 6 new exploits, sends `h` and a log line,
  then sends prompt `P`.

---

## 7. Example turn

Seat 2 (NEON_MAMBA) runs the exploit for `C4` (node 50, `$32`), next to a lone spike on `C3` (node 38, `$26`), and brokers
the new hive to Lotus-12 (syn 5, cell value 7). Hex bytes, with letters shown as characters:

```
S→all : t 02
S→s2  : p 02 P 3C                      prompt: spike a node, 60 s
C     : P 32                           run exploit C4
S→all : x 02 01 32                     C4 = unaffiliated spike
S→s2  : p 03 F 3C 7F                   prompt: broker to whom? (all 7 free)
C     : F 05                           Lotus-12
S→all : x 03 07 32 26                  C4, C3 = Lotus-12 hive
S→all : z 05 02                        Lotus-12 size 2
S→all : k 02 05 01                     finder's cut: seat 2 holds 1 LTS
S→all : l 1D "[*] HIVE BROKERED TO LOTUS-12"
S→s2  : p 13 B 3C 3C 00 00             prompt: buy; 60 s; cash ¥6,000; may not end
        00 00 00 00 00 04 00           prices: only LTS on the Grid, ¥400
        19 19 19 19 19 18 19           Darkpool supply (LTS 24)
C     : B 05 05 07                     buy 2 LTS
S→all : k 02 05 03                     seat 2 holds 3 LTS
S→all : m 02 34 00                     cash ¥5,200
S→s2  : h …                            kit with the new exploit
S→all : t 03
```

That is about 110 bytes for a whole founding turn, including the log text. A plain growth turn is about 60.

---

## 8. Open questions
- Table selection: a table byte in `H` (current draft), or one TCP port per table, or the FujiNet Lobby.
- Should `l` be sent at all to clients that want to save bandwidth? (e.g. a "terse" flag in `H`)
- Spectator seats (`kind 4`)?
- Replace patched-out exploits at end of turn (common house rule), or only on a kit burn (rulebook)?
- Hidden coin holdings as a table option?
