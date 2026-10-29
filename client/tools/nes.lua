-- nes.lua -- MAME's autoboot script for tools/run-nes.sh: drives the NES client
-- in MAME's FujiNet NES cartridge (-nes_slot fujinet) without a window.
--
--   NES_PRESS   "secs:Button ..." presses (A B Select Start Up Down Left Right,
--               held 6 frames); "secs:Select+A" holds SELECT around it
--   NES_AT      seconds: print the screen as text, save a snapshot and stop
--   NES_EVERY   seconds: also print the screen this often
--   NES_SHOTS   "secs secs ...": also save a snapshot at each (MAME numbers them)
--   NES_DUMPDIR the autoplay build's dumps (test/nes_dump.c): each time it
--               counts one, txt_cells (960 bytes), the game's attributes (64)
--               and the sprites (256) go to NES_DUMPDIR/scrNNN.bin, the format
--               pc-nes writes (tools/render_nes.py draws them)
--   NES_LBL     the build's label file (ld65 -Ln), for the dumps' addresses
--
-- The screen as text is the name table shown, as the PPU has it (inverse marked
-- with ^ on the line under it); the dumps are the program's own copy.

local function env(name)                -- unset and empty are the same
  local v = os.getenv(name)
  if v ~= "" then return v end
end
local at = tonumber(env("NES_AT") or "0")
local every = tonumber(env("NES_EVERY") or "0")
local dumpdir = env("NES_DUMPDIR")
local shots = {}
for t in string.gmatch(env("NES_SHOTS") or "", "[%d%.]+") do shots[#shots + 1] = tonumber(t) end
local presses = {}
for t, b in string.gmatch(env("NES_PRESS") or "", "([%d%.]+):([%w+]+)") do
  presses[#presses + 1] = { t = tonumber(t), b = b, state = 0 }
end

local port = manager.machine.ioport.ports[":ctrl1:joypad:JOYPAD"]
local cpu = manager.machine.devices[":maincpu"].spaces["program"]
local ppu = manager.machine.devices[":ppu"].spaces["videoram"]

local labels = {}
local lbl = env("NES_LBL")
if lbl then
  for a, name in io.open(lbl):read("a"):gmatch("al (%x+) %.([%w_]+)") do
    labels[name] = tonumber(a, 16)
  end
end

-- the tiles below $20 (src/nes/font.txt, text.c): box lines, the static, quarter blocks
local function ch(v)
  local c = v & 0x7F
  if c >= 0x20 and c < 0x7F then return string.char(c) end
  if c >= 0x01 and c <= 0x0B then return "#" end
  if c >= 0x0C and c <= 0x0F then return ":" end
  if c >= 0x10 and c <= 0x1F then return c == 0x1F and " " or "%" end
  return "?"
end

local function screen()
  local base = (cpu:read_u8(labels["_ppu_ctrl"] or 0) & 1) == 1 and 0x2400 or 0x2000
  local out = {}
  for r = 0, 29 do
    local t, inv = {}, {}
    for c = 0, 31 do
      local v = ppu:readv_u8(base + r * 32 + c)
      t[#t + 1] = ch(v)
      inv[#inv + 1] = (v >= 0xA0) and "^" or " "
    end
    out[#out + 1] = string.format("%2d|%s|", r, table.concat(t))
    local s = table.concat(inv)
    if s:find("%^") then out[#out + 1] = "  |" .. s .. "|" end
  end
  return table.concat(out, "\n")
end

local function dump(n)
  local f = io.open(string.format("%s/scr%03d.bin", dumpdir, n), "wb")
  local bytes = {}
  local function add(addr, len)
    for i = 0, len - 1 do bytes[#bytes + 1] = string.char(cpu:read_u8(addr + i)) end
  end
  add(labels["_txt_cells"], 960)
  add(labels["_attr"], 64)
  add(0x0200, 256)
  f:write(table.concat(bytes))
  f:close()
end

-- NES_PADBOT=1: play with the pad, as a player would, from what the client's RAM says is open (its labels):
-- type a handle with the dial and join, take the seat count, run exploits, pick syndicates, count a
-- hivejack's crypto up and down, buy and undo; once in a game, open the instructions and turn their
-- pages, and ask to log off and stay. Every answer goes through input.c's cursor.
local padbot = env("NES_PADBOT")
local plan, last_kind, wait, did_help, did_esc = {}, nil, 0, false, false
local function ram(name, i) return cpu:read_u8(labels[name] + (i or 0)) end

local function replan()
  local kind, step = ram("_pr_kind"), ram("_step")
  local rnd = math.random
  plan = {}
  local function add(b, n) for _ = 1, n or 1 do plan[#plan + 1] = b end end
  if ram("_help_up") ~= 0 then
    add("Right", 2) add("Left") add("B")
  elseif ram("_field") == 0 then
    if ram("_edit_len") < 4 then add("Up", 1 + rnd(0, 3)) add("Right") else add("Start") end
  elseif ram("_leaving") ~= 0 then
    add("B")                               -- stay
  elseif kind == 0 then
    if not did_help then did_help = true add("Select")
    elseif not did_esc then did_esc = true add("Select+Start") end
  elseif kind == string.byte("S") then add("Up") add("A")
  elseif kind == string.byte("P") then add("Right", rnd(0, 5)) add("A")
  elseif kind == string.byte("F") or kind == string.byte("K") then add("Right", rnd(0, 3)) add("A")
  elseif kind == string.byte("D") then add("Up", rnd(0, 4)) add("Down", rnd(0, 1)) add("A")
  elseif kind == string.byte("B") then
    if step == 1 then add(rnd(0, 1) == 0 and "B" or "A")
    else
      for _ = 1, rnd(0, 3) do add(rnd(0, 1) == 0 and "Down" or "Up", rnd(0, 2)) add("A") end
      if rnd(0, 3) == 0 then add("B") end
      add("Start")
    end
  end
  return kind * 256 + step
end

local seq, dumps, next_print, done = nil, 0, every, false
_G.__nes_sub = emu.add_machine_frame_notifier(function()
  if done then return end
  local now = manager.machine.time:as_double()
  if padbot and labels["_pr_kind"] and now > 5 then
    local key = ram("_pr_kind") * 256 + ram("_step") + ram("_field") * 65536 + ram("_leaving") * 131072
        + ram("_help_up") * 262144
    if key ~= last_kind or (#plan == 0 and wait <= -120) then
      last_kind = key
      replan()
      wait = 20
    end
    wait = wait - 1
    if wait <= 0 and #plan > 0 then
      presses[#presses + 1] = { t = now, b = table.remove(plan, 1), state = 0 }
      wait = 12
    end
  end
  for _, p in ipairs(presses) do
    local sel = p.b:match("^Select%+(%w+)$")
    if p.state == 0 and now >= p.t then
      if sel then port.fields["P1 Select"]:set_value(1) end
      port.fields["P1 " .. (sel or p.b)]:set_value(1)
      p.state, p.frames = 1, 6
    elseif p.state == 1 then
      p.frames = p.frames - 1
      if p.frames <= 0 then
        port.fields["P1 " .. (sel or p.b)]:set_value(0)
        if sel then port.fields["P1 Select"]:set_value(0) end
        p.state = 2
      end
    end
  end
  if dumpdir and labels["_dump_seq"] then
    local s = cpu:read_u8(labels["_dump_seq"])
    if seq == nil then seq = s end
    if s ~= seq then
      seq = s
      dump(dumps)
      dumps = dumps + 1
      cpu:write_u8(labels["_dump_ack"], s)
    end
  end
  if #shots > 0 and now >= shots[1] then
    table.remove(shots, 1)
    manager.machine.screens[":screen"]:snapshot()
  end
  if every > 0 and now >= next_print then
    print(string.format("-- %.1f s\n%s", now, screen()))
    next_print = next_print + every
  end
  if at > 0 and now >= at then
    done = true
    print(string.format("-- %.1f s\n%s", now, screen()))
    local scr = manager.machine.screens[":screen"]
    if scr then scr:snapshot() end
    manager.machine:exit()
  end
end)
