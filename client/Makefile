# SYN-ACK WIRE client (Atari for now).
#   make                      build/atari/synack.xex (the game), build/atari/wiremon.xex, wiremon-auto.xex
#   make URL=N:TCP://192.168.1.20:6502/    point them at another server (default: the public one on fly.io)
#   make FANCY=1              use the hand-made screens instead of the text ones
#   make atr                  build/atari/syn-ack-wire-atari.atr: boots the game with PicoBoot, plus the help pages
#   make atrtest              build/atari/local/syn-ack-wire-atari.atr: the same disk dialing this machine (LOCAL_URL)
#   make atr BOOT=Dos25       the same disk booting DOS 2.5 (the game as AUTORUN.SYS); make clean first
#   make apple2_text          build/apple2_text/synack: the Apple II client, text mode (IIe, IIc, IIgs)
#   make apple2_text-po PRODOS=disk   build/apple2_text/syn-ack-wire-apple2.po: a ProDOS disk that boots it
#   make msdos WATCOM=dir     build/msdos/SYNACK.EXE: the MS-DOS client (8088 and up, FUJINET.SYS)
#   make msdos-img WATCOM=dir build/msdos/syn-ack-wire-msdos.img: a 360 KB floppy with SYNACK.EXE and its help pages
#   make msdos-wiremon WATCOM=dir     build/msdos/WIREMON.EXE, WMAUTO.EXE: the protocol monitor for MS-DOS
#   make clean
CC65    ?=
CC      = $(if $(CC65),$(CC65)/,)cc65
AS      = $(if $(CC65),$(CC65)/,)ca65
LD      = $(if $(CC65),$(CC65)/,)ld65
TARGET  = atari
CFLAGS  = -t $(TARGET) -O -Or -Cl -DDISPLAY_IDLE=1     # display_idle(): the last move's blink
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
HDRS    = src/common/proto_tab.h src/common/theme_tab.h src/common/game.h src/display.h src/platform.h src/config.h Makefile

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
# The disk is named for the game and the platform, as the manuals are, so platforms sharing an extension can't clash.
ATR     = $(B)/syn-ack-wire-atari.atr
$(ATR): $(B)/synack.xex $(HELP) tools/help_disk.py tools/qr.py $(if $(filter Dos25,$(BOOT)),$(B)/dos.sys)
	python3 tools/help_disk.py --qr "$(QR_URL)" --check $(HELP)
	rm -rf $(B)/disk && mkdir $(B)/disk
ifeq ($(BOOT),Dos25)
	cp $(B)/dos.sys $(B)/disk/DOS.SYS
	cp $< $(B)/disk/AUTORUN.SYS
else
	cp $< $(B)/disk/SYNACK.XEX
endif
	dir2atr -b $(BOOT) $@ $(B)/disk >/dev/null
	python3 tools/help_disk.py --qr "$(QR_URL)" $@ $(HELP_SECTOR) $(HELP)
atr: $(ATR)
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
PO      = $(A2)/syn-ack-wire-apple2.po
$(PO): $(A2)/synack $(A2)/synack.hlp tools/prodos_boot.py
	@test -n "$(PRODOS)" || { echo "the disk needs PRODOS=path/to/your/ProDOS-2.4-disk" >&2; exit 1; }
	rm -f $@
	$(AC) -pro140 $@ SYNACK
	python3 tools/prodos_boot.py "$(PRODOS)" $@
	$(AC) -g "$(PRODOS)" PRODOS $(A2)/prodos
	$(AC) -p $@ PRODOS SYS 0x2000 < $(A2)/prodos
	$(AC) -as $@ SYNACK.SYSTEM < $(A2)/synack
	$(AC) -p $@ SYNACK.HLP BIN 0 < $(A2)/synack.hlp
	$(AC) -ls $@
apple2_text-po: $(PO)

# --- MS-DOS (8088 and up): FujiNet RS-232 through fujinet-lib (a pinned copy in lib/), which calls FUJINET.SYS's
# INT F5h. Open Watcom v2 (WATCOM=its folder), 8086 code, small model. DOS names are 8.3: WMAUTO.EXE is
# wiremon's self-test (AUTOTEST), which also writes WIREMON.TXT.
WATCOM  ?= $(HOME)/openwatcom
DOSB    = build/msdos
DOSLIB  = lib/fujinet-lib/msdos-5.0.0
WCL     = env WATCOM=$(WATCOM) PATH=$(WATCOM)/binl64:$(WATCOM)/binl:$$PATH INCLUDE=$(WATCOM)/h wcl
WFLAGS  = -q -0 -ms -bt=dos -os -w4 -D__fastcall__= -I$(DOSLIB)/include
DOSNET  = src/common/proto.c src/msdos/net_fn.c $(DOSLIB)/fujinet-msdos-5.0.0.lib

DOSGAME = src/common/main.c src/common/game.c src/msdos/display_text.c src/msdos/input.c src/msdos/sound.c $(DOSNET)

src/msdos/title_art.h: $(TITLE) tools/title_art.py      # the Atari's logo, in code page 437's box characters
	python3 tools/title_art.py d 0 $< > $@
src/msdos/margins.h: src/msdos/margins.txt tools/margins_msdos.py     # the circuit-board panels either side
	python3 tools/margins_msdos.py $< > $@
DOSHDRS = src/msdos/title_art.h src/msdos/margins.h

$(DOSB):
	mkdir -p $@
$(DOSB)/SYNACK.EXE: $(DOSGAME) $(HDRS) $(DOSHDRS) | $(DOSB)
	$(WCL) $(WFLAGS) $(DEFS) -DDISPLAY_IDLE=1 -DQUIT_TO_OS=1 -fo=$(DOSB)/ -fe=$@ $(DOSGAME)
	@echo "$@: $$(stat -c %s $@) bytes"
msdos: $(DOSB)/SYNACK.EXE $(DOSB)/SYNACK.HLP
# the instruction pages; a help page's [QR] marker links to the MS-DOS manual, by the fly.io server's short link
DOSHELP = $(sort $(wildcard help/msdos/screen/HELP*.TXT))
DOS_QR_URL ?= HTTPS://SYN-ACK-WIRE.FLY.DEV/MSDOS
$(DOSB)/SYNACK.HLP: $(DOSHELP) tools/help_msdos.py tools/qr.py | $(DOSB)
	python3 tools/help_msdos.py --qr "$(DOS_QR_URL)" $@ $(DOSHELP)
# the release floppy: 360 KB, so the first PCs' drives read it; not bootable, and no FUJINET.SYS, which comes with
# the player's FujiNet (mtools: mformat, mcopy). The FujiNet lobby mounts it, changes to its root and runs its
# AUTOEXEC.BAT (fujinet-lobby, clients/src/msdos/platform.c): one line, no @ECHO OFF, which DOS 2 doesn't know.
DOSIMG  = $(DOSB)/syn-ack-wire-msdos.img
$(DOSIMG): $(DOSB)/SYNACK.EXE $(DOSB)/SYNACK.HLP
	rm -f $@
	mformat -i $@ -C -f 360 -v SYNACK ::
	mcopy -i $@ $(DOSB)/SYNACK.EXE $(DOSB)/SYNACK.HLP ::
	printf 'SYNACK\r\n' | mcopy -i $@ - ::AUTOEXEC.BAT
	mdir -i $@ ::
msdos-img: $(DOSIMG)
# the autoplay build: plays by itself and saves the screen (SCRnnn.BIN) at each new prompt and every 4 seconds
DOSAUTO = test/autoplay.c test/dos_dump.c $(DOSGAME)
$(DOSB)/SYNAUTO.EXE: $(DOSAUTO) $(HDRS) $(DOSHDRS) | $(DOSB)
	$(WCL) $(WFLAGS) $(DEFS) -DDISPLAY_IDLE=1 -DQUIT_TO_OS=1 -DAUTOPLAY -DDUMP_TICKS=72 -DKEY_TICKS=6 -DAUTO_HANDLE='"PC"' -fo=$(DOSB)/ -fe=$@ $(DOSAUTO)
	@echo "$@: $$(stat -c %s $@) bytes"
msdos-test: $(DOSB)/SYNAUTO.EXE
$(DOSB)/WIREMON.EXE: test/wiremon.c $(DOSNET) $(HDRS) | $(DOSB)
	$(WCL) $(WFLAGS) $(DEFS) -fo=$(DOSB)/ -fe=$@ test/wiremon.c $(DOSNET)
	@echo "$@: $$(stat -c %s $@) bytes"
$(DOSB)/WMAUTO.EXE: test/wiremon.c $(DOSNET) $(HDRS) | $(DOSB)
	$(WCL) $(WFLAGS) $(DEFS) -DAUTOTEST -fo=$(DOSB)/ -fe=$@ test/wiremon.c $(DOSNET)
	@echo "$@: $$(stat -c %s $@) bytes"
msdos-wiremon: $(DOSB)/WIREMON.EXE $(DOSB)/WMAUTO.EXE

# --- CoCo 1/2: 40x24 text drawn on the PMODE 4 screen (src/coco/text.c), FujiNet over DriveWire through
# fujinet-lib (a pinned copy in lib/). CMOC and lwtools; toolshed's decb (DECB=path) makes the disk. The program
# loads at $2600, above the PMODE 4 screen at $0E00 (HDB-DOS keeps the memory under that), and must end below $7B00:
# above it, a 32K CoCo's stack and Disk BASIC's strings. The disk is syn-ack-wire-coco.dsk, as the others are named;
# the program on it is SYNACK.BIN. (The FujiNet lobby boots a CoCo game by RUNM of the mounted disk's own name,
# fujinet-lobby clients/src/coco/platform.c, and Disk BASIC takes 8 letters: RUNM"SYN-ACK-WIRE-COCO" is ?FN ERROR,
# so the lobby needs the disk served under a name of at most 8: the server image has it as synack.dsk too.) AUTOEXEC.BAS runs it for a disk booted by hand, after PCLEAR1:
# with BASIC's 4 graphics pages its program starts at $2601, where LOADM would write over the running line.
CMOC    ?= cmoc
DECB    ?= decb
COCO    = build/coco
COCOLIB = lib/fujinet-lib/coco-5.0.0
COCOFLAGS = --coco -O2 -Isrc/coco/include -I$(COCOLIB)/include -D__fastcall__= -DDISPLAY_IDLE=1 -DTITLE_IDLE=1
COCOGAME = $(COCO)/main.o $(COCO)/game.o $(COCO)/proto.o $(COCO)/display_text.o $(COCO)/text.o $(COCO)/input.o \
           $(COCO)/net_fn.o $(COCO)/sound.o $(COCO)/help.o $(COCO)/snow.o
COCOHDRS = src/coco/font.h src/coco/title_art.h src/coco/title_snow.h src/coco/text.h

src/coco/font.h: src/coco/font.txt tools/font_coco.py
	python3 tools/font_coco.py $< > $@
# the Atari's title art in the font's box glyphs, and which of its pixels get the static (snow.c)
src/coco/title_art.h: $(TITLE) src/coco/font.txt tools/title_art.py tools/font_coco.py
	python3 tools/title_art.py k 0 $< src/coco/font.txt > $@
src/coco/title_snow.h: $(TITLE) src/coco/font.txt tools/title_art.py tools/font_coco.py
	python3 tools/title_art.py m 0 $< src/coco/font.txt > $@

$(COCO):
	mkdir -p $@
$(COCO)/%.o: src/common/%.c $(HDRS) | $(COCO)
	$(CMOC) $(COCOFLAGS) $(DEFS) -c -o $@ $<
$(COCO)/%.o: src/coco/%.c $(HDRS) $(COCOHDRS) | $(COCO)
	$(CMOC) $(COCOFLAGS) $(DEFS) -c -o $@ $<
$(COCO)/synack.bin: $(COCOGAME) $(COCOLIB)/fujinet-coco-5.0.0.lib
	$(CMOC) --coco --org=2600 --limit=7B00 -o $@ $(COCOGAME) -L$(COCOLIB) -l:fujinet-coco-5.0.0.lib
	@echo "$@: $$(stat -c %s $@) bytes"
coco: $(COCO)/synack.bin $(COCO)/synack.hlp
# the instruction pages, as PMODE 4 screen images; a help page's [QR] marker links to the CoCo manual
COCOHELP = $(sort $(wildcard help/coco/screen/HELP*.TXT))
COCO_QR_URL ?= HTTPS://SYN-ACK-WIRE.FLY.DEV/COCO
$(COCO)/synack.hlp: $(COCOHELP) tools/help_coco.py tools/font_coco.py tools/qr.py src/coco/font.txt | $(COCO)
	python3 tools/help_coco.py --qr "$(COCO_QR_URL)" $@ $(COCOHELP)
COCODSK = $(COCO)/syn-ack-wire-coco.dsk
$(COCODSK): $(COCO)/synack.bin $(COCO)/synack.hlp
	rm -f $@
	$(DECB) dskini $@
	printf '10 PCLEAR1:LOADM"SYNACK":EXEC\n' > $(COCO)/autoexec.bas
	$(DECB) copy -0 -a -l $(COCO)/autoexec.bas $@,AUTOEXEC.BAS
	$(DECB) copy -2 -b $(COCO)/synack.bin $@,SYNACK.BIN
	$(DECB) copy -1 -b $(COCO)/synack.hlp $@,SYNACK.HLP
	$(DECB) dir $@
coco-dsk: $(COCODSK)
# the autoplay build on a disk (the game disk's layout, so AUTOEXEC.BAS runs it): plays by itself; a debugger
# stopping on pc_dump (test/coco_dump.c) saves the screen at each new prompt and every 4 seconds
# (tools/xroar_dump.py). Its link map is build/coco/auto/synack.map.
COCOAUTO = $(COCO)/auto
COCOAUTOSRC = src/common/main.c src/common/game.c src/common/proto.c src/coco/display_text.c src/coco/text.c \
              src/coco/input.c src/coco/net_fn.c src/coco/sound.c src/coco/help.c src/coco/snow.c test/autoplay.c \
              test/coco_dump.c
$(COCOAUTO)/synack.bin: $(COCOAUTOSRC) $(HDRS) $(COCOHDRS) $(COCOLIB)/fujinet-coco-5.0.0.lib
	mkdir -p $(COCOAUTO)
	$(CMOC) $(COCOFLAGS) $(DEFS) -DAUTOPLAY -DDUMP_TICKS=240 -DAUTO_HANDLE='"COCO"' --org=2600 --limit=7B00 \
	    -i --intdir=$(COCOAUTO) -o $@ $(COCOAUTOSRC) -L$(COCOLIB) -l:fujinet-coco-5.0.0.lib
	@echo "$@: $$(stat -c %s $@) bytes"
$(COCOAUTO)/syn-ack-wire-coco.dsk: $(COCOAUTO)/synack.bin $(COCO)/synack.hlp
	rm -f $@
	$(DECB) dskini $@
	printf '10 PCLEAR1:LOADM"SYNACK":EXEC\n' > $(COCOAUTO)/autoexec.bas
	$(DECB) copy -0 -a -l $(COCOAUTO)/autoexec.bas $@,AUTOEXEC.BAS
	$(DECB) copy -2 -b $(COCOAUTO)/synack.bin $@,SYNACK.BIN
	$(DECB) copy -1 -b $(COCO)/synack.hlp $@,SYNACK.HLP
coco-auto: $(COCOAUTO)/syn-ack-wire-coco.dsk

# --- the Apple II display (src/apple2/display_text.c) on a PC, playing by itself and saving its screen at each
# new prompt: the manual's captures (tools/capture.sh with PLATFORM=apple2). build/pc/pc-apple2 [host] [port]
pc-apple2: $(HDRS) src/apple2/title_art.h
	mkdir -p build/pc
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -DAUTOPLAY -DDUMP_TICKS=240 -DDISPLAY_IDLE=1 \
	    -DPC_SCREEN=pc_screen -DAUTO_HANDLE='"APPLE"' -o build/pc/pc-apple2 \
	    src/common/main.c src/common/game.c src/common/proto.c src/apple2/display_text.c test/autoplay.c test/pc_harness.c

# --- the same for the MS-DOS display (src/msdos/display_text.c): build/pc/pc-msdos [host] [port]
pc-msdos: $(HDRS) $(DOSHDRS)
	mkdir -p build/pc
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -DAUTOPLAY -DDUMP_TICKS=240 -DDISPLAY_IDLE=1 \
	    -DPC_SCREEN=pc_screen -DPC_MSDOS -DAUTO_HANDLE='"PC"' -o build/pc/pc-msdos \
	    src/common/main.c src/common/game.c src/common/proto.c src/msdos/display_text.c test/autoplay.c test/pc_harness.c

# --- the same for the CoCo display (src/coco/display_text.c, over test/coco_text_pc.c): build/pc/pc-coco [host] [port]
pc-coco: $(HDRS) src/coco/title_art.h
	mkdir -p build/pc
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -DAUTOPLAY -DDUMP_TICKS=240 -DDISPLAY_IDLE=1 \
	    -DPC_SCREEN=txt_cells -DPC_COCO -DAUTO_HANDLE='"COCO"' -o build/pc/pc-coco \
	    src/common/main.c src/common/game.c src/common/proto.c src/coco/display_text.c test/coco_text_pc.c \
	    test/autoplay.c test/pc_harness.c

clean:
	rm -rf build

.PHONY: all atr atrtest test pc pc-apple2 pc-msdos pc-coco apple2_text apple2_text-po msdos msdos-img msdos-test msdos-wiremon coco coco-dsk coco-auto clean
.PRECIOUS: $(B)/%.s $(B)/%.o $(A2)/%.s $(A2)/%.o $(COCO)/%.o
.DELETE_ON_ERROR:
