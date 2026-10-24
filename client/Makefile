# SYN-ACK WIRE client (Atari for now).
#   make                      build/atari/synack.xex (the game), build/atari/wiremon.xex, wiremon-auto.xex
#   make URL=N:TCP://192.168.1.20:6502/    point them at another server (default: the public one on fly.io)
#   make FANCY=1              use the hand-made screens instead of the text ones
#   make atr                  build/atari/synack.atr: boots the game with PicoBoot, plus the help pages
#   make atrtest              build/atari/local/synack.atr: the same disk dialing a server on this machine (LOCAL_URL)
#   make atr BOOT=Dos25       the same disk booting DOS 2.5 (the game as AUTORUN.SYS); make clean first
#   make apple2_text          build/apple2_text/synack: the Apple II client, text mode (IIe, IIc, IIgs)
#   make apple2_text-po PRODOS=disk   build/apple2_text/synack.po: a ProDOS disk that boots it
#   make clean
CC65    ?=
CC      = $(if $(CC65),$(CC65)/,)cc65
AS      = $(if $(CC65),$(CC65)/,)ca65
LD      = $(if $(CC65),$(CC65)/,)ld65
TARGET  = atari
CFLAGS  = -t $(TARGET) -O -Or -Cl
URL     ?= N:TCP://syn-ack-wire.fly.dev:6502/
LOCAL_URL ?= N:TCP://localhost:6502/
FANCY   ?=
HINTS   ?=
DEFS    = -DURL='"$(URL)"' $(if $(FANCY),-DFANCY_TITLE=1 -DFANCY_PLAYFIELD=1,) $(if $(HINTS),-DHINTS=$(HINTS),) $(if $(LOBBY_KEY),-DLOBBY_KEY=$(LOBBY_KEY),)
B       = build/atari
# the disk's boot (dir2atr -b): PicoBoot406, no DOS; or Dos25
BOOT    ?= PicoBoot406
# your DOS 2.5 disk image, for BOOT=Dos25 (DOS.SYS comes from it)
DOS25   ?=
HELP    = $(sort $(wildcard help/atari/screen/HELP*.TXT))
# the help pages' first sector on the disk, 8 sectors a page (help.s and help_disk.py)
HELP_SECTOR ?= 600
# what a help page's [QR] marker links to: this platform's manual on the fly.io server (upper case keeps the
# QR code small)
QR_URL  ?= HTTPS://SYN-ACK-WIRE.FLY.DEV/ATARI

NET     = $(B)/net_sio.o $(B)/ring.o
COMMON  = $(B)/proto.o

GAME    = $(B)/main.o $(B)/game.o $(B)/display_text.o $(B)/input.o $(B)/help.o $(B)/sound.o $(B)/snow.o $(B)/dli.o
HDRS    = src/common/proto_tab.h src/common/theme_tab.h src/common/game.h src/display.h src/platform.h src/config.h

all: $(B)/synack.xex $(B)/wiremon.xex $(B)/wiremon-auto.xex

src/common/proto_tab.h: ../server/acquire/proto.py ../tools/gen_proto.py
	python3 ../tools/gen_proto.py h > $@
src/common/theme_tab.h: ../theme/theme.toml ../tools/gen_theme.py
	python3 ../tools/gen_theme.py c > $@

# the title art, drawn from screen row TITLE_ROW; the static fills its letters (snow.s)
TITLE   = src/atari/title4.atascii
TITLE_ROW = 1
src/atari/title_art.h: $(TITLE) tools/title_art.py
	python3 tools/title_art.py c $(TITLE_ROW) $< > $@
src/atari/title_art.inc: $(TITLE) tools/title_art.py
	python3 tools/title_art.py s $(TITLE_ROW) $< > $@
$(B)/display_text.s: src/atari/title_art.h
$(B)/snow.o: src/atari/title_art.inc

$(B)/synack.xex: $(GAME) $(COMMON) $(NET) src/atari/game.cfg
	$(LD) -C src/atari/game.cfg -m $(B)/synack.map -o $@ $(GAME) $(COMMON) $(NET) $(TARGET).lib
	@echo "$@: $$(stat -c %s $@) bytes"

$(B):
	mkdir -p $@

$(B)/%.s: src/common/%.c $(HDRS) | $(B)
	$(CC) $(CFLAGS) $(DEFS) -o $@ $<
$(B)/%.s: src/atari/%.c $(HDRS) | $(B)
	$(CC) $(CFLAGS) $(DEFS) -o $@ $<
$(B)/%.o: src/atari/%.s | $(B)
	$(AS) -t $(TARGET) -D HELP_SECTOR=$(HELP_SECTOR) -o $@ $<
$(B)/%.o: $(B)/%.s
	$(AS) -t $(TARGET) -o $@ $<

$(B)/wiremon.s: test/wiremon.c src/common/proto_tab.h | $(B)
	$(CC) $(CFLAGS) -DURL='"$(URL)"' -o $@ $<
$(B)/wiremon-auto.s: test/wiremon.c src/common/proto_tab.h | $(B)
	$(CC) $(CFLAGS) -DURL='"$(URL)"' -DAUTOTEST -o $@ $<

$(B)/%.xex: $(B)/%.o $(COMMON) $(NET)
	$(LD) -t $(TARGET) -m $(B)/$*.map -o $@ $^ $(TARGET).lib
	@echo "$@: $$(stat -c %s $@) bytes"

# --- the game disk. PicoBoot406 (MyPicoDos's 3-sector boot, standard-speed SIO) loads the
# first file on the disk and runs it; no DOS. BOOT=Dos25 boots DOS 2.5 instead, which runs
# AUTORUN.SYS; DOS.SYS comes from your own DOS 2.5 disk image (DOS25=...), extracted with `atr`.
# dir2atr writes the image, then help_disk.py writes the help pages as raw sectors from
# HELP_SECTOR (help.s reads them).
$(B)/dos.sys: | $(B)
	@test -n "$(DOS25)" || { echo "BOOT=Dos25 needs DOS25=path/to/your/DOS-2.5-disk.atr" >&2; exit 1; }
	atr "$(DOS25)" get dos.sys $@
$(B)/%.atr: $(B)/%.xex $(HELP) tools/help_disk.py tools/qr.py $(if $(filter Dos25,$(BOOT)),$(B)/dos.sys)
	python3 tools/help_disk.py --qr "$(QR_URL)" --check $(HELP)
	rm -rf $(B)/disk-$* && mkdir $(B)/disk-$*
ifeq ($(BOOT),Dos25)
	cp $(B)/dos.sys $(B)/disk-$*/DOS.SYS
	cp $< $(B)/disk-$*/AUTORUN.SYS
else
	cp $< $(B)/disk-$*/SYNACK.XEX
endif
	dir2atr -b $(BOOT) $@ $(B)/disk-$* >/dev/null
	python3 tools/help_disk.py --qr "$(QR_URL)" $@ $(HELP_SECTOR) $(HELP)
atr: $(B)/synack.atr
# its own build folder, so the local and public builds never share objects
atrtest:
	$(MAKE) B=$(B)/local URL=$(LOCAL_URL) atr


# --- unattended test build: plays by itself and BRKs for screen dumps (see test/autoplay.c)
$(B)/main-auto.s: src/common/main.c $(HDRS) | $(B)
	$(CC) $(CFLAGS) $(DEFS) -DAUTOPLAY -o $@ $<
$(B)/autoplay.s: test/autoplay.c $(HDRS) | $(B)
	$(CC) $(CFLAGS) $(DEFS) -o $@ $<
AUTO    = $(B)/main-auto.o $(B)/autoplay.o $(filter-out $(B)/main.o,$(GAME))
$(B)/synack-auto.xex: $(AUTO) $(COMMON) $(NET) src/atari/game.cfg
	$(LD) -C src/atari/game.cfg -m $(B)/synack-auto.map -o $@ $(AUTO) $(COMMON) $(NET) $(TARGET).lib
test: $(B)/synack-auto.xex

# --- the common code on a PC (gcc), against a real server: make pc && build/pc/pc-client
pc: $(HDRS)
	mkdir -p build/pc
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -DAUTOPLAY -DDUMP_TICKS=0 -o build/pc/pc-client \
	    src/common/main.c src/common/game.c src/common/proto.c test/autoplay.c test/pc_harness.c

# --- Apple II, text mode (apple2_text): FujiNet through fujinet-lib (a pinned copy in lib/).
# The disk takes PRODOS (the system file) and its boot block from your own ProDOS 2.4 disk image
# (PRODOS=path), the game as SYNACK.SYSTEM (a ProDOS system file: ProDOS runs the first one it finds), and
# the help pages. AC runs AppleCommander 1.6 or later.
A2      = build/apple2_text
A2LIB   = lib/fujinet-lib/apple2-5.0.0
A2FLAGS = -t apple2 -O -Or -Cl -I $(A2LIB)/include -DDISPLAY_IDLE=1
PRODOS  ?=
AC      ?= ac
A2HELP  = $(sort $(wildcard help/apple2/screen/HELP*.TXT))
# what an Apple help page's [QR] marker links to: the Apple II manual, by the fly.io server's short link
A2_QR_URL ?= HTTPS://SYN-ACK-WIRE.FLY.DEV/APPLE2
A2GAME  = $(A2)/main.o $(A2)/game.o $(A2)/proto.o $(A2)/display_text.o $(A2)/input.o $(A2)/net_fn.o \
          $(A2)/help.o $(A2)/sound.o $(A2)/ring.o

src/apple2/title_art.h: src/apple2/title.txt tools/title_text.py
	python3 tools/title_text.py $< > $@
$(A2)/display_text.s: src/apple2/title_art.h

$(A2):
	mkdir -p $@
$(A2)/%.s: src/common/%.c $(HDRS) | $(A2)
	$(CC) $(A2FLAGS) $(DEFS) -o $@ $<
$(A2)/%.s: src/apple2/%.c $(HDRS) | $(A2)
	$(CC) $(A2FLAGS) $(DEFS) -o $@ $<
$(A2)/%.o: src/apple2/%.s | $(A2)
	$(AS) -t apple2 -o $@ $<
$(A2)/%.o: $(A2)/%.s
	$(AS) -t apple2 -o $@ $<

$(A2)/synack: $(A2GAME) src/apple2/game.cfg $(A2LIB)/fujinet-apple2-5.0.0.lib
	$(LD) -C src/apple2/game.cfg -m $(A2)/synack.map -o $@ $(A2GAME) $(A2LIB)/fujinet-apple2-5.0.0.lib apple2.lib
	@echo "$@: $$(stat -c %s $@) bytes"
apple2_text: $(A2)/synack

$(A2)/synack.hlp: $(A2HELP) tools/help_apple2.py | $(A2)
	python3 tools/help_apple2.py --qr "$(A2_QR_URL)" $@ $(A2HELP)
$(A2)/synack.po: $(A2)/synack $(A2)/synack.hlp tools/prodos_boot.py
	@test -n "$(PRODOS)" || { echo "the disk needs PRODOS=path/to/your/ProDOS-2.4-disk" >&2; exit 1; }
	rm -f $@
	$(AC) -pro140 $@ SYNACK
	python3 tools/prodos_boot.py "$(PRODOS)" $@
	$(AC) -g "$(PRODOS)" PRODOS $(A2)/prodos
	$(AC) -p $@ PRODOS SYS 0x2000 < $(A2)/prodos
	$(AC) -as $@ SYNACK.SYSTEM < $(A2)/synack
	$(AC) -p $@ SYNACK.HLP BIN 0 < $(A2)/synack.hlp
	$(AC) -ls $@
apple2_text-po: $(A2)/synack.po

# --- the Apple II display (src/apple2/display_text.c) on a PC, playing by itself and saving its screen at each
# new prompt: the manual's captures (tools/capture.sh with PLATFORM=apple2). build/pc/pc-apple2 [host] [port]
pc-apple2: $(HDRS) src/apple2/title_art.h
	mkdir -p build/pc
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -DAUTOPLAY -DDUMP_TICKS=240 -DDISPLAY_IDLE=1 \
	    -DPC_SCREEN=pc_screen -DAUTO_HANDLE='"APPLE"' -o build/pc/pc-apple2 \
	    src/common/main.c src/common/game.c src/common/proto.c src/apple2/display_text.c test/autoplay.c test/pc_harness.c

clean:
	rm -rf build

.PHONY: all atr atrtest test pc pc-apple2 apple2_text apple2_text-po clean
.PRECIOUS: $(B)/%.s $(B)/%.o $(A2)/%.s $(A2)/%.o
.DELETE_ON_ERROR:
