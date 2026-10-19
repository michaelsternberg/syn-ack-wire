# SYN-ACK WIRE client (Atari for now).
#   make                      build/synack.xex (the game), build/wiremon.xex, build/wiremon-auto.xex
#   make URL=N:TCP://192.168.1.20:6502/    point them at another server
#   make FANCY=1              use the hand-made screens instead of the text ones
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

NET     = $(B)/net_sio.o $(B)/ring.o
COMMON  = $(B)/proto.o

GAME    = $(B)/main.o $(B)/game.o $(B)/display_text.o $(B)/input.o
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
	$(AS) -t $(TARGET) -o $@ $<
$(B)/%.o: $(B)/%.s
	$(AS) -t $(TARGET) -o $@ $<

$(B)/wiremon.s: test/wiremon.c src/common/proto_tab.h | $(B)
	$(CC) $(CFLAGS) -DURL='"$(URL)"' -o $@ $<
$(B)/wiremon-auto.s: test/wiremon.c src/common/proto_tab.h | $(B)
	$(CC) $(CFLAGS) -DURL='"$(URL)"' -DAUTOTEST -o $@ $<

$(B)/%.xex: $(B)/%.o $(COMMON) $(NET)
	$(LD) -t $(TARGET) -m $(B)/$*.map -o $@ $^ $(TARGET).lib
	@echo "$@: $$(stat -c %s $@) bytes"

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

.PHONY: all bakeoff test pc clean
.PRECIOUS: $(B)/%.s $(B)/%.o
