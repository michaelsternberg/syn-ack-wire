# SYN-ACK WIRE client (Atari for now).
#   make                      build/synack.xex (the game), build/wiremon.xex, build/wiremon-auto.xex
#   make URL=N:TCP://192.168.1.20:6502/    point them at another server
#   make FANCY=1              use the hand-made screens instead of the text ones
#   make atr                  build/synack.atr: boots the game with PicoBoot, plus the help pages
#   make atr BOOT=Dos25       the same disk booting DOS 2.5 (the game as AUTORUN.SYS); make clean first
#   make bakeoff              fujinet-lib size comparison (needs cc65 newer than 2.19)
#   make clean
CC65    ?=
CC      = $(if $(CC65),$(CC65)/,)cc65
AS      = $(if $(CC65),$(CC65)/,)ca65
LD      = $(if $(CC65),$(CC65)/,)ld65
TARGET  = atari
CFLAGS  = -t $(TARGET) -O -Or -Cl
URL     ?= N:TCP://localhost:6502/
FANCY   ?=
HINTS   ?=
DEFS    = -DURL='"$(URL)"' $(if $(FANCY),-DFANCY_TITLE=1 -DFANCY_PLAYFIELD=1,) $(if $(HINTS),-DHINTS=$(HINTS),)
B       = build
FNLIB   = vendor/fujinet-lib-5.0.0
# the disk's boot (dir2atr -b): PicoBoot406, no DOS; or Dos25
BOOT    ?= PicoBoot406
DOS25   ?= $(HOME)/Nextcloud/Projects/computers/atari8/Source/plato2/disks/DOS_v2.5_1984_Atari.atr
HELP    = $(sort $(wildcard help/HELP*.TXT))
# the help pages' first sector on the disk, 8 sectors a page (help.s and help_disk.py)
HELP_SECTOR ?= 600

NET     = $(B)/net_sio.o $(B)/ring.o
COMMON  = $(B)/proto.o

GAME    = $(B)/main.o $(B)/game.o $(B)/display_text.o $(B)/input.o $(B)/help.o
HDRS    = src/common/proto_tab.h src/common/theme_tab.h src/common/game.h src/display.h src/platform.h src/config.h

all: $(B)/synack.xex $(B)/wiremon.xex $(B)/wiremon-auto.xex

src/common/proto_tab.h: ../server/acquire/proto.py ../tools/gen_proto.py
	python3 ../tools/gen_proto.py h > $@
src/common/theme_tab.h: ../theme/theme.toml ../tools/gen_theme.py
	python3 ../tools/gen_theme.py c > $@

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
	atr "$(DOS25)" get dos.sys $@
$(B)/%.atr: $(B)/%.xex $(HELP) tools/help_disk.py $(if $(filter Dos25,$(BOOT)),$(B)/dos.sys)
	python3 tools/help_disk.py --check $(HELP)
	rm -rf $(B)/disk-$* && mkdir $(B)/disk-$*
ifeq ($(BOOT),Dos25)
	cp $(B)/dos.sys $(B)/disk-$*/DOS.SYS
	cp $< $(B)/disk-$*/AUTORUN.SYS
else
	cp $< $(B)/disk-$*/SYNACK.XEX
endif
	dir2atr -b $(BOOT) $@ $(B)/disk-$* >/dev/null
	python3 tools/help_disk.py $@ $(HELP_SECTOR) $(HELP)
atr: $(B)/synack.atr

# --- size bake-off: raw SIO vs fujinet-lib (fujinet-lib v5 needs cc65 newer than 2.19)
$(B)/bakeoff.s: test/bakeoff.c | $(B)
	$(CC) $(CFLAGS) -o $@ $<
$(B)/net_fnlib.s: test/net_fnlib.c | $(B)
	$(CC) $(CFLAGS) -I $(FNLIB) -o $@ $<
bakeoff: $(B)/bakeoff.o $(COMMON) $(NET) $(B)/net_fnlib.o
	$(LD) -t $(TARGET) -o $(B)/bake_sio.xex $(B)/bakeoff.o $(COMMON) $(NET) $(TARGET).lib
	$(LD) -t $(TARGET) -o $(B)/bake_fn.xex $(B)/bakeoff.o $(COMMON) $(B)/net_fnlib.o $(B)/ring.o \
	    $(FNLIB)/fujinet-atari-5.0.0.lib $(TARGET).lib
	@echo "raw SIO:    $$(stat -c %s $(B)/bake_sio.xex) bytes"
	@echo "fujinet-lib: $$(stat -c %s $(B)/bake_fn.xex) bytes"

# --- unattended test build: plays by itself and BRKs for screen dumps (see test/autoplay.c)
$(B)/main-auto.s: src/common/main.c $(HDRS) | $(B)
	$(CC) $(CFLAGS) $(DEFS) -DAUTOPLAY -o $@ $<
$(B)/autoplay.s: test/autoplay.c $(HDRS) | $(B)
	$(CC) $(CFLAGS) $(DEFS) -o $@ $<
AUTO    = $(B)/main-auto.o $(B)/autoplay.o $(filter-out $(B)/main.o,$(GAME))
$(B)/synack-auto.xex: $(AUTO) $(COMMON) $(NET) src/atari/game.cfg
	$(LD) -C src/atari/game.cfg -m $(B)/synack-auto.map -o $@ $(AUTO) $(COMMON) $(NET) $(TARGET).lib
test: $(B)/synack-auto.xex

# --- the common code on a PC (gcc), against a real server: make pc && build/pc-client
pc: $(HDRS) | $(B)
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -DAUTOPLAY -DDUMP_TICKS=0 -o $(B)/pc-client \
	    src/common/main.c src/common/game.c src/common/proto.c test/autoplay.c test/pc_harness.c

clean:
	rm -rf $(B)

.PHONY: all atr bakeoff test pc clean
.PRECIOUS: $(B)/%.s $(B)/%.o
.DELETE_ON_ERROR:
