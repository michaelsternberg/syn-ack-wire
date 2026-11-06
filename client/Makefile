# SYN-ACK WIRE client: Atari, Apple II, MS-DOS, CoCo, NES, Adam, H89, Intellivision.
#   make                      build/atari/synack.xex (the game), build/atari/wiremon.xex, wiremon-auto.xex
#   make URL=N:TCP://192.168.1.20:6502/    point them at another server (default: the public one on fly.io)
#   make FANCY=1              use the hand-made screens instead of the text ones
#   make atr                  build/atari/syn-ack-wire-atari.atr: boots the game with PicoBoot, plus the help pages
#   make atrtest              build/atari/local/syn-ack-wire-atari.atr: the same disk dialing this machine (LOCAL_URL)
#   make atr BOOT=Dos25       the same disk booting DOS 2.5 (the game as AUTORUN.SYS); make clean first
#   make atari-nio            build/atari-nio/syn-ack-wire-atari.atr for FujiNet-NIO (separate from the normal build)
#   make B=build/atari-nio-local NIO=1 URL=N:TCP://127.0.0.1:6502/ LOBBY_KEY=0 atr   NIO disk for a local server
#   make test-atari-nio       host tests for the NIO adapter; make test-atari-nio-6502 runs its soft65c02 test
#   NIO_LIB=/path             override the pinned package with a fujinet-nio-lib checkout or package
#   NIO_VERSION=version      select a released Atari library (downloaded if absent)
#                             In the NIO workspace, source scripts/env.sh before building; use a fresh B when URL changes.
#   make apple2_text          build/apple2_text/synack: the Apple II client, text mode (IIe, IIc, IIgs)
#   make apple2_text-po PRODOS=disk   build/apple2_text/syn-ack-wire-apple2.po: a ProDOS disk that boots it
#   make msdos WATCOM=dir     build/msdos/SYNACK.EXE: the MS-DOS client (8088 and up, FUJINET.SYS)
#   make msdos-img WATCOM=dir build/msdos/syn-ack-wire-msdos.img: a 360 KB floppy with SYNACK.EXE and its help pages
#   make msdos-wiremon WATCOM=dir     build/msdos/WIREMON.EXE, WMAUTO.EXE: the protocol monitor for MS-DOS
#   make coco                 build/coco/synack.bin: the CoCo 1/2 client (make coco-dsk: its disk)
#   make nes                  build/nes/syn-ack-wire-nes.nes: the NES client (the FujiNet NES cartridge)
#   make adam                 build/adam/syn-ack-wire-adam.ddp: the Coleco Adam client (z88dk, through defoogi)
#   make h89                  build/h89/SYNACK.COM: the Heathkit H89 client (CP/M 2.2.04, 64K, the H89 FujiNet card)
#   make h89-disk H89SYS=disk build/h89/syn-ack-wire-h89.h8d: an H17 disk that boots CP/M, with SYNACK.COM and its help
#   make h89-wiremon H89SYS=disk      build/h89/wiremon.h8d: the protocol monitor for the H89, on a CP/M disk that boots
#   make intv                 build/intv/syn-ack-wire-intv.rom: the Intellivision client (IntyBASIC, the FujiNet cartridge)
#   make clean
.DEFAULT_GOAL := all
CC65    ?=
HOSTCC  ?= cc
CC      = $(if $(CC65),$(CC65)/,)cc65
CL65    = $(if $(CC65),$(CC65)/,)cl65
AS      = $(if $(CC65),$(CC65)/,)ca65
LD      = $(if $(CC65),$(CC65)/,)ld65
TARGET  = atari
CFLAGS  = -t $(TARGET) -O -Or -Cl -DDISPLAY_IDLE=1 -DLOG_HISTORY=16  # the last move's blink; 16 log lines
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

NIO_VERSION ?= 0.1.2
NIO_PACKAGE = lib/fujinet-nio/atari-$(NIO_VERSION)
NIO_LIB ?= $(NIO_PACKAGE)
NIO_ARCHIVE = $(if $(wildcard $(NIO_LIB)/Makefile),$(NIO_LIB)/build/fujinet-nio-atari.lib,$(NIO_LIB)/fujinet-nio-atari.lib)
$(NIO_ARCHIVE):
	@if [ "$(NIO_LIB)" = "$(NIO_PACKAGE)" ]; then \
	    python3 tools/fetch_nio_lib.py "$(NIO_VERSION)" "$(NIO_PACKAGE)"; \
	elif [ -f "$(NIO_LIB)/Makefile" ]; then \
	    $(MAKE) -C "$(NIO_LIB)" atari; \
	else echo "NIO package or checkout missing: $(NIO_LIB)" >&2; exit 1; fi
ifeq ($(NIO),1)
CFLAGS += -DNIO_BUILD=1
NET     = $(B)/net_nio.o $(B)/net_nio_appkey.o $(B)/ring.o
CFLAGS += -I $(NIO_LIB)/include
NETLIB  = $(NIO_ARCHIVE)
else
NET     = $(B)/net_sio.o $(B)/ring.o
NETLIB  =
endif
COMMON  = $(B)/proto.o

ifeq ($(NIO),1)
# Track the selected package across command-line version changes, including when
# an older archive has an older timestamp than the existing build.
$(B)/nio-lib-selection: FORCE | $(B)
	@printf '%s\n' '$(abspath $(NIO_LIB))' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@
$(B)/net_nio.s: $(NIO_ARCHIVE) $(B)/nio-lib-selection
$(B)/synack.xex: $(B)/nio-lib-selection
endif
FORCE:

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

$(B)/synack.xex: $(GAME) $(COMMON) $(NET) $(NETLIB) src/atari/game.cfg
	$(LD) -C src/atari/game.cfg -m $(B)/synack.map -Ln $(B)/synack.lbl -o $@ $(GAME) $(COMMON) $(NET) $(NETLIB) $(TARGET).lib
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
	$(LD) -t $(TARGET) -m $(B)/$*.map -o $@ $^ $(NETLIB) $(TARGET).lib
	@echo "$@: $$(stat -c %s $@) bytes"

# Separate objects and disk: the ordinary Atari build remains unchanged.
atari-nio:
	$(MAKE) B=build/atari-nio NIO=1 atr

test-atari-nio: $(NIO_ARCHIVE)
	python3 -m unittest discover -s test -p test_fetch_nio_lib.py
	mkdir -p build/test-atari-nio
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -D__ATARI__ -D__fastcall__= -DNIO_TEST -DFN_MAX_URL_LEN=128 \
	    -I$(NIO_LIB)/include test/net_nio_test.c src/atari/net_nio.c -o build/test-atari-nio/net-test
	build/test-atari-nio/net-test

test-atari-nio-6502: $(NIO_ARCHIVE)
	mkdir -p build
	ln -sfn "$(abspath $(NIO_LIB)/include)" build/nio-test-include
	soft65c02_unit -b build/test-atari-nio-appkey -i test/atari_nio_appkey_soft65.yaml

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
src/msdos/margins.h: src/msdos/margins.txt $(TITLE) tools/margins_msdos.py tools/board80.py tools/title_art.py   # the circuit-board panels either side, and the 80-column title's
	python3 tools/margins_msdos.py $< $(TITLE) > $@
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

# --- NES: the FujiNet NES cartridge (its mailbox at $5000, its 8K WRAM at $6000) through fujinet-lib (a pinned
# copy in lib/). NROM-256: 32K of PRG, the tiles made in CHR-RAM at start-up, the instruction pages in the ROM
# (help_nes.py). nes-romstamp.py writes the cartridge's FUJI claim at $FFF0 and checks that nothing writes the
# mailbox's write-only pages with a read-modify-write instruction; a failing image is deleted.
NES     = build/nes
NESLIB  = lib/fujinet-lib/nes-a29e57f
NESFLAGS = -t nes -O -Or -Cl -I $(NESLIB)/include -I $(NES) -DDISPLAY_IDLE=1 -DNARROW_SAYS=1
NESPLAT = $(NES)/crt0.o $(NES)/ppu.o $(NES)/pad.o $(NES)/sound.o $(NES)/ring.o $(NES)/text.o $(NES)/input.o \
          $(NES)/net_fn.o
NESGAME = $(NES)/main.o $(NES)/game.o $(NES)/proto.o $(NES)/display_text.o $(NES)/help.o $(NESPLAT)
# the autoplay build (NESAUTO=1, by make nes-auto): plays by itself, its dumps saved by tools/nes.lua
ifneq ($(NESAUTO),)
NESFLAGS += -DAUTOPLAY -DDUMP_TICKS=240 -DAUTO_HANDLE='"NES"'
NESGAME += $(NES)/autoplay.o $(NES)/nes_dump.o
endif
NESHELP = $(sort $(wildcard help/nes/screen/HELP*.TXT))
# what a help page's [QR] marker links to: the NES manual, by the fly.io server's short link
NES_QR_URL ?= HTTPS://SYN-ACK-WIRE.FLY.DEV/NES
NESHDRS = src/nes/font.h src/nes/title_art.h src/nes/text.h

src/nes/font.h: src/nes/font.txt tools/font_nes.py
	python3 tools/font_nes.py $< > $@
# the Atari's title art in the box tiles, its letters' strokes in the static's tiles
src/nes/title_art.h: $(TITLE) tools/title_art.py
	python3 tools/title_art.py n 1 $< > $@
$(NES)/help_pages.h: $(NESHELP) tools/help_nes.py tools/qr.py | $(NES)
	python3 tools/help_nes.py --qr "$(NES_QR_URL)" $@ $(NESHELP)
$(NES)/help.s: $(NES)/help_pages.h

$(NES):
	mkdir -p $@
$(NES)/%.s: src/common/%.c $(HDRS) | $(NES)
	$(CC) $(NESFLAGS) $(DEFS) -o $@ $<
$(NES)/%.s: src/nes/%.c $(HDRS) $(NESHDRS) | $(NES)
	$(CC) $(NESFLAGS) $(DEFS) -o $@ $<
$(NES)/%.s: test/%.c $(HDRS) $(NESHDRS) | $(NES)
	$(CC) $(NESFLAGS) $(DEFS) -o $@ $<
$(NES)/%.o: src/nes/%.s | $(NES)
	$(AS) -t nes -o $@ $<
$(NES)/%.o: $(NES)/%.s
	$(AS) -t nes -o $@ $<

NESROM  = $(NES)/syn-ack-wire-nes.nes
$(NESROM): $(NESGAME) src/nes/game.cfg $(NESLIB)/fujinet-nes-a29e57f.lib
	$(LD) -C src/nes/game.cfg -m $(NES)/synack.map -Ln $(NES)/synack.lbl -o $@ $(NESGAME) \
	    $(NESLIB)/fujinet-nes-a29e57f.lib nes.lib
	python3 $(NESLIB)/nes-romstamp.py --stamp --map $(NES)/synack.map $@
	@echo "$@: $$(stat -c %s $@) bytes"
nes: $(NESROM)
# its own build folder, so the local and public builds never share objects
nestest:
	$(MAKE) NES=$(NES)/local URL=$(LOCAL_URL) nes
# plays by itself: make nes-auto URL=N:TCP://host:port/ LOBBY_KEY=0, then DUMPDIR=dir tools/run-nes.sh
nes-auto:
	$(MAKE) NES=$(NES)/auto NESAUTO=1 nes

# --- Coleco Adam: FujiNet on AdamNet through fujinet-lib (a pinned copy in lib/, built with zsdcc by
# tools/fnlib_adam.sh). z88dk, `zcc +coleco -subtype=adam` with zsdcc, from the defoogi container unless it is
# installed here (tools/z88dk): the container sees only this repository. zcc hands -D values on through a shell,
# so a string is quoted twice. The program runs from 0000h, its stack below D390h, EOS above; the image's
# bootstrap loads it from block 1 and sits at C800h, so the program must end below that (tools/adam_end.py
# checks; the variables are not in the image). Our own NMI (nmi.asm): CRT_ENABLE_NMI=2. Nothing prints, so the
# CRT sets no screen mode and z88dk's console goes (its output redirected to a bare ret): 2.5 KB with its font.
# syn-ack-wire-adam.ddp is a tape image, which the FujiNet mounts as a disk and the Adam boots.
ADAM    = build/adam
ADAMLIB = lib/fujinet-lib/adam-a29e57f
ZCC     = tools/z88dk zcc +coleco -subtype=adam -compiler=sdcc
ADAMFLAGS = -SO3 -I$(ADAMLIB)/include -I$(ADAM) -D__fastcall__= -DDISPLAY_IDLE=1 -DNARROW_SAYS=1 \
          -DSAY_MAXBUY='"\"3 PER TURN: RETURN TO BUY\""'
ADAMDEFS = -DURL='"\"$(URL)\""' $(if $(FANCY),-DFANCY_TITLE=1 -DFANCY_PLAYFIELD=1,) $(if $(HINTS),-DHINTS=$(HINTS),) \
          $(if $(LOBBY_KEY),-DLOBBY_KEY=$(LOBBY_KEY),)
ADAMGAME = $(ADAM)/main.o $(ADAM)/game.o $(ADAM)/proto.o $(ADAM)/display_text.o $(ADAM)/text.o $(ADAM)/input.o \
           $(ADAM)/net_fn.o $(ADAM)/help.o $(ADAM)/nmi.o $(ADAM)/vdp.o $(ADAM)/sound.o $(ADAM)/eoscall.o
# the autoplay build (ADAMAUTO=1, by make adam-auto): plays by itself; the emulator takes the pictures
ifneq ($(ADAMAUTO),)
ADAMFLAGS += -DAUTOPLAY -DDUMP_TICKS=0 -DAUTO_HANDLE='"\"ADAM\""'
ADAMGAME += $(ADAM)/autoplay.o
endif
ADAMHELP = $(sort $(wildcard help/adam/screen/HELP*.TXT))
# what a help page's [QR] marker links to: the Adam manual, by the fly.io server's short link
ADAM_QR_URL ?= HTTPS://SYN-ACK-WIRE.FLY.DEV/ADAM
ADAMHDRS = src/adam/text.h src/adam/vdp.h src/adam/title_art.h src/nes/font.h

# the Atari's title art in the NES's box glyphs (the Adam has the NES's font), from row 0
src/adam/title_art.h: $(TITLE) tools/title_art.py
	python3 tools/title_art.py n 0 $< > $@
$(ADAM)/help_pages.h: $(ADAMHELP) tools/help_nes.py tools/qr.py | $(ADAM)
	python3 tools/help_nes.py --rows 24 --top 0 --qr "$(ADAM_QR_URL)" $@ $(ADAMHELP)
$(ADAM)/help.o: $(ADAM)/help_pages.h

$(ADAM):
	mkdir -p $@
$(ADAM)/%.o: src/common/%.c $(HDRS) | $(ADAM)
	$(ZCC) $(ADAMFLAGS) $(ADAMDEFS) -c -o $@ $<
$(ADAM)/%.o: src/adam/%.c $(HDRS) $(ADAMHDRS) | $(ADAM)
	$(ZCC) $(ADAMFLAGS) $(ADAMDEFS) -c -o $@ $<
$(ADAM)/%.o: test/%.c $(HDRS) | $(ADAM)
	$(ZCC) $(ADAMFLAGS) $(ADAMDEFS) -c -o $@ $<
$(ADAM)/%.o: src/adam/%.asm | $(ADAM)
	$(ZCC) -c -o $@ $<

ADAMDDP = $(ADAM)/syn-ack-wire-adam.ddp
$(ADAMDDP): $(ADAMGAME) $(ADAMLIB)/fujinet-adam-a29e57f.lib tools/adam_end.py
	cd $(ADAM) && $(abspath tools/z88dk) zcc +coleco -subtype=adam -compiler=sdcc -m \
	    -pragma-define:CRT_ENABLE_NMI=2 -pragma-define:CRT_INITIALIZE_BSS=1 -pragma-define:CRT_ENABLE_STDIO=0 \
	    -pragma-define:CLIB_DEFAULT_SCREEN_MODE=-1 -pragma-define:CLIB_EXIT_SCREEN_MODE=-1 \
	    -pragma-redirect:fputc_cons=l_ret -create-app -o syn-ack-wire-adam \
	    $(notdir $(ADAMGAME)) -L$(abspath $(ADAMLIB)) -lfujinet-adam-a29e57f
	python3 tools/adam_end.py $(ADAM)/syn-ack-wire-adam.map
	@echo "$@: $$(stat -c %s $@) bytes"
adam: $(ADAMDDP)
# its own build folder, so the local and public builds never share objects
adamtest:
	$(MAKE) ADAM=$(ADAM)/local URL=$(LOCAL_URL) adam
# plays by itself: make adam-auto URL=N:TCP://host:port/ LOBBY_KEY=0, then tools/run-adam.sh
adam-auto:
	$(MAKE) ADAM=$(ADAM)/auto ADAMAUTO=1 adam

# --- Heathkit H89 under Heath/Zenith CP/M 2.2.04 (64K): the H19 terminal, FujiNet through the H89 FujiNet card
# and fujinet-lib, built from source with zsdcc (lib/fujinet-lib/README.md). z88dk's zcc and zsdcc on the PATH.
# A disk boots: H89SYS is a 64K CP/M 2.2.04 system disk for the H17 (fujinet-go-h89-desktop's
# tools/disks/boot/cpm-2.2.04-64k.h8d); its development tools come off, the program goes on (cpmtools, with
# tools/h89/diskdefs).
H89B    = build/h89
H89LIB  = lib/fujinet-lib/h89-194824b
H89SYS  ?=
H89ZCC  = zcc +cpm -compiler=sdcc -SO3 -pragma-define:CRT_INITIALIZE_BSS=1     # variables start at 0, as C says
H89FLAGS = -D__H89__ -D__fastcall__= -I$(H89LIB)/include
H89NET  = src/common/proto.c src/h89/net_fn.c
H89DEFS = $(filter-out -DURL=%,$(DEFS)) -DURL='"\"$(URL)\""'     # zcc hands a define on through a shell: quoted twice
H89TOOLS = ASM.COM DDT.COM ED.COM LOAD.COM MOVCPM17.COM SUBMIT.COM XSUB.COM CONFIGUR.COM ASSIGN.COM
# h89-disk OUT FILES...: H89SYS less its tools, with FILES (paths from here) on it
define h89-disk
	@test -n "$(H89SYS)" || { echo "H89SYS=<a 64K CP/M 2.2.04 H17 system disk> is needed"; exit 1; }
	cp $(H89SYS) $(1).tmp
	cd tools/h89 && cpmrm -f h17 $(CURDIR)/$(1).tmp $(foreach f,$(H89TOOLS),0:$(f))
	cd tools/h89 && cpmcp -f h17 $(CURDIR)/$(1).tmp $(foreach f,$(2),$(CURDIR)/$(f)) 0:
	cd tools/h89 && cpmls -f h17 -D $(CURDIR)/$(1).tmp
	mv $(1).tmp $(1)
endef

H89GAME = src/common/main.c src/common/game.c src/h89/display_text.c src/h89/input.c src/h89/sound.c $(H89NET)

src/h89/screens.h: $(TITLE) src/h89/margins.txt tools/h89/screens.py tools/board80.py tools/title_art.py   # the board and the title
	python3 tools/h89/screens.py $(TITLE) src/h89/margins.txt > $@

$(H89B):
	mkdir -p $@
$(H89B)/SYNACK.COM: $(H89GAME) $(H89LIB)/fujinet-h89.lib $(HDRS) src/h89/h89.h src/h89/screens.h | $(H89B)
	$(H89ZCC) $(H89FLAGS) $(H89DEFS) -DDISPLAY_IDLE=1 -DQUIT_TO_OS=1 -DQUIT_ASK='"\"QUIT TO CP/M? Y/N\""' \
	    -m -o $@ $(H89GAME) -L$(H89LIB) -lfujinet-h89
	python3 tools/h89/trimcom.py $(@:.COM=.map) $@
h89: $(H89B)/SYNACK.COM $(H89B)/SYNACK.HLP
# the instruction pages; a help page's [QR] marker links to the H89 manual, by the fly.io server's short link
H89HELP = $(sort $(wildcard help/h89/screen/HELP*.TXT))
H89_QR_URL ?= HTTPS://SYN-ACK-WIRE.FLY.DEV/H89
$(H89B)/SYNACK.HLP: $(H89HELP) tools/h89/help.py tools/help_msdos.py tools/qr.py | $(H89B)
	python3 tools/h89/help.py --qr "$(H89_QR_URL)" $@ $(H89HELP)
$(H89B)/syn-ack-wire-h89.h8d: $(H89B)/SYNACK.COM $(H89B)/SYNACK.HLP
	$(call h89-disk,$@,$^)
h89-disk: $(H89B)/syn-ack-wire-h89.h8d
$(H89B)/WIREMON.COM: test/wiremon.c $(H89NET) $(H89LIB)/fujinet-h89.lib $(HDRS) src/h89/h89.h | $(H89B)
	$(H89ZCC) $(H89FLAGS) $(H89DEFS) -m -o $@ test/wiremon.c $(H89NET) -L$(H89LIB) -lfujinet-h89
	python3 tools/h89/trimcom.py $(@:.COM=.map) $@
$(H89B)/wiremon.h8d: $(H89B)/WIREMON.COM
	$(call h89-disk,$@,$<)
h89-wiremon: $(H89B)/wiremon.h8d
# --- Intellivision: the FujiNet Intellivision cartridge (its mailbox at $9C00, its RAM at $8000-$9BFF). There is no
# C compiler for the CP1610, so src/intv/ is the common code again in IntyBASIC 1.5, built here from its source
# (IB_SRC, as M.U.L.E.'s client builds it: the IntyBASIC on PATH may be 1.4, which has no OPTION MAP). tools/bas_pp.py
# fills the theme's and the protocol's {NAME}s in from theme_tab.h and proto_tab.h, and keeps or drops the '#if lines;
# as1600 (on PATH) makes the .rom, and tools/checkcfg.py fails a ROM that would land on the ECS's ROM, the cartridge's
# RAM or a GRAM mirror. The ROM uses an ECS keyboard when one is there.
INTV    = build/intv
IB_SRC  ?= $(HOME)/Workspace/IntyBASIC
IB      = build/intybasic
AS1600  ?= as1600
INTVBAS = main memory fujinet net proto input ecskbd sound isr display prompt title help game autoplay
INTVHELP = $(sort $(wildcard help/intv/screen/HELP*.TXT))
INTVDEFS = -D URL="$(URL)" -D LOBBY_KEY=$(if $(LOBBY_KEY),$(LOBBY_KEY),11) -D TABLE=0 -D HINTS=$(if $(HINTS),$(HINTS),1) \
           -D AUTOPLAY=$(INTVAUTO) -D AUTO_HANDLE=INTV
INTVGEN = $(addprefix $(INTV)/,$(addsuffix .bas,$(INTVBAS)) title_art.bas help_pages.bas ecskbd.asm)

$(IB)/intybasic: $(wildcard $(IB_SRC)/*.cpp $(IB_SRC)/*.h)
	@mkdir -p $(IB)
	$(CXX) -O2 -o $@ $(IB_SRC)/node.cpp $(IB_SRC)/microcode.cpp $(IB_SRC)/code.cpp $(IB_SRC)/IntyBASIC.cpp
# its prologue and epilogue beside it; 1.5.2's prologue has a comment written with ' (line 251), which as1600 rejects
$(IB)/intybasic_prologue.asm: $(IB_SRC)/intybasic_prologue.asm
	@mkdir -p $(IB)
	sed "s/  ' Max internal index/  ; Max internal index/" $< > $@
$(IB)/intybasic_epilogue.asm: $(IB_SRC)/intybasic_epilogue.asm
	@mkdir -p $(IB)
	cp $< $@

$(INTV):
	mkdir -p $@
$(INTV)/%.bas: src/intv/%.bas tools/bas_pp.py src/common/theme_tab.h src/common/proto_tab.h Makefile | $(INTV)
	python3 tools/bas_pp.py -H src/common/theme_tab.h -H src/common/proto_tab.h $(INTVDEFS) $< $@
$(INTV)/ecskbd.asm: src/intv/ecskbd.asm | $(INTV)
	cp $< $@
# the Atari's title art in colored squares, its letters' strokes the static
$(INTV)/title_art.bas: $(TITLE) tools/title_art.py | $(INTV)
	python3 tools/title_art.py i 0 $< > $@
$(INTV)/help_pages.bas: $(INTVHELP) tools/help_nes.py | $(INTV)
	python3 tools/help_nes.py --rows 12 --cols 20 --top 0 --left 0 --bas $@ $(INTVHELP)

INTVROM = $(INTV)/syn-ack-wire-intv.rom
$(INTV)/synack.asm: $(INTVGEN) $(IB)/intybasic $(IB)/intybasic_prologue.asm $(IB)/intybasic_epilogue.asm
	cd $(INTV) && $(abspath $(IB))/intybasic main.bas synack.asm $(abspath $(IB))/ 2>&1 | grep -v '^Warning' | tee intybasic.log
	@! grep -q '^Error' $(INTV)/intybasic.log || { rm -f $@; exit 1; }
# the .bin and .cfg only for checkcfg (the .cfg is the memory map); the .rom is the cartridge's, and jzIntv's
$(INTVROM): $(INTV)/synack.asm tools/checkcfg.py
	cd $(INTV) && $(AS1600) -o synack.bin -l synack.lst synack.asm
	python3 tools/checkcfg.py $(INTV)/synack.cfg
	cd $(INTV) && $(AS1600) -o syn-ack-wire-intv.rom -s syn-ack-wire-intv.sym synack.asm
	@echo "$@: $$(stat -c %s $@) bytes"
intv: $(INTVROM)
# its own build folder, so the local and public builds never share files
intvtest:
	$(MAKE) INTV=$(INTV)/local URL=$(LOCAL_URL) intv
# plays by itself: make intv-auto URL=N:TCP://host:port/ LOBBY_KEY=0, then tools/run-intv.sh
intv-auto:
	$(MAKE) INTV=$(INTV)/auto INTVAUTO=1 intv

# --- GEOS on the Atari: a GEOS application for geos-atari (GEOS 2.0 on a 130XE), which runs well-behaved GEOS 64
# programs, so cc65's geos-cbm target. The Atari's title, network and chords (src/atari/ built with __GEOS__), the
# title and the playfield GEOS's own way, panels and a menu bar (src/geos/display_gui.c); keys and the pointer from
# GEOS (src/geos/). The program starts at $0500, after the receive ring; the C stack is 512 bytes, not cc65's 1K,
# for the room (nothing here recurses). The header's file and class name are the theme's title. memset is our own
# (src/geos/memset.s): geos-cbm.lib's is GEOS's FillRam, whose arguments differ.
GEOS    = build/geos
GEOSFLAGS = -t geos-cbm -O -Or -Cl -DDISPLAY_IDLE=1 -DFANCY_TITLE=1 -DFANCY_PLAYFIELD=1 -DLOG_HISTORY=16 \
            -DQUIT_TO_OS=1 -DQUIT_ASK='"QUIT TO GEOS? Y/N"' -DHELP_CLASS='"$(GEOSHELPCLASS)"' -DREGION_PICK=1
GEOSOBJ = $(addprefix $(GEOS)/,header.o main.o game.o proto.o display_gui.o draw.o sig.o input.o screen.o \
          frame.o net_sio.o sound.o ring.o memset.o help.o region.o)
GEOSNAME = $(shell sed -n 's/^\#define TH_TITLE "\(.*\)"/\1/p' src/common/theme_tab.h)
# The instruction pages, as a GEOS file on the GEOS disk (tools/help_geos.py): the Atari's pages, its QR code's
# URL. The game finds the file by its class (at most 12 characters), so the name (16) is only what the DeskTop shows.
GEOSHELP ?= $(HELP)
GEOSHELPNAME ?= $(GEOSNAME) ?
GEOSHELPCLASS = $(word 1,$(GEOSNAME)) HELP

$(GEOS):
	mkdir -p $@
$(GEOS)/%.s: src/common/%.c $(HDRS) | $(GEOS)
	$(CC) $(GEOSFLAGS) $(DEFS) -o $@ $<
$(GEOS)/display_gui.s: src/atari/title_art.h
# The link: on geos-atari, GEOS's background screen is in a 130XE bank (beside its kernal's drawing code), not in main
# memory at $6000-$7FFF as on a C64, so that is the program's too (__BACKBUFSIZE__=0): the C stack goes to $7E00.
GEOSLD ?= -D __BACKBUFSIZE__=0
# The region panel's list (src/geos/region.c), from the servers fly.io runs (tools/gen_regions.py). A test build
# gives its own: REGIONS="--config FILE --host localhost --seats-port N" (servers on this machine; no [lobby] in it).
REGIONS ?=
$(GEOS)/regions_tab.h: ../server/servers.fly.toml ../fly.toml ../tools/gen_regions.py Makefile | $(GEOS)
	python3 ../tools/gen_regions.py c $(REGIONS) > $@
$(GEOS)/region.s: src/geos/region.c $(GEOS)/regions_tab.h $(HDRS) | $(GEOS)
	$(CC) $(GEOSFLAGS) $(DEFS) -I $(GEOS) -o $@ $<
$(GEOS)/%.s: src/geos/%.c $(HDRS) | $(GEOS)
	$(CC) $(GEOSFLAGS) $(DEFS) -o $@ $<
$(GEOS)/net_sio.o $(GEOS)/sound.o: $(GEOS)/%.o: src/atari/%.s | $(GEOS)
	$(AS) -t geos-cbm -o $@ $<
$(GEOS)/%.o: src/geos/%.s | $(GEOS)
	$(AS) -t geos-cbm -o $@ $<
$(GEOS)/%.o: $(GEOS)/%.s
	$(AS) -t geos-cbm -o $@ $<
$(GEOS)/header.grc: src/common/theme_tab.h Makefile | $(GEOS)
	printf '%s\n' 'HEADER APPLICATION "$(GEOSNAME)" "$(GEOSNAME)" "V1.0" {' 'dostype USR' 'mode 40only' \
	    'author "Michael Sternberg"' 'info "Online, through a FujiNet. OPTION: hints, SELECT: sound, Esc: quit."' \
	    '}' > $@
$(GEOS)/header.s: $(GEOS)/header.grc
	grc65 -s $@ $<
$(GEOS)/synack.cvt: $(GEOSOBJ)
	$(LD) -t geos-cbm -S 0x0500 -D __STACKSIZE__=0x200 $(GEOSLD) -m $(GEOS)/synack.map -Ln $(GEOS)/synack.lbl -o $@ $(GEOSOBJ) geos-cbm.lib
	@echo "$@: $$(stat -c %s $@) bytes"
geos: $(GEOS)/synack.cvt
# The disks, from a geos-atari checkout (GEOS_ATARI) and its SIO build for D2: (make -C $(GEOS_ATARI)
# DRIVE=siodrv_atari SIO_UNIT=2). syn-ack-wire-geos-boot.atr boots that GEOS in D1: with MyPicoDos 4.06 in autorun
# mode, which loads it at high speed when the FujiNet's HSIO index is set (58 s at 19200, 27 s at index 8, 15 s at
# index 3 in atari800), and at 19200 when it isn't (62 s: a few more reads than PicoBoot's); geos-atari's disk
# driver then reads D2: at that speed too (the game loads in 4.3 s at index 3, 13.4 s at 19200);
# syn-ack-wire-geos.atr, in D2:, is a 180K GEOS disk (geos-atari's tools/mkramdisk.py) with geos-atari's file
# selector as its DESK TOP, which opens GEOS's file dialog on the applications: this one; and the help file.
GEOS_ATARI ?= ../../geos-atari
GEOSXEX = $(GEOS_ATARI)/build/atari_130_sio_d2/GEOS_ATARI_130XE_SIO.XEX
GEOSDESK = $(GEOS_ATARI)/ramdisk/cvt-128k/filesel-as-DESK TOP.cvt
$(GEOS)/synack-help.cvt: $(GEOSHELP) tools/help_geos.py tools/help_disk.py tools/qr.py Makefile | $(GEOS)
	python3 tools/help_geos.py --qr "$(QR_URL)" -o $@ --name "$(GEOSHELPNAME)" --class "$(GEOSHELPCLASS)" \
	    --parent "$(GEOSNAME)" --info "The instructions for $(GEOSNAME): ? on its title screen." $(GEOSHELP) >/dev/null
$(GEOS)/syn-ack-wire-geos.atr: $(GEOS)/synack.cvt $(GEOS)/synack-help.cvt
	python3 $(GEOS_ATARI)/tools/mkramdisk.py --atr 717 -o $@ "$(GEOSDESK)" $^ >/dev/null
$(GEOS)/syn-ack-wire-geos-boot.atr: Makefile $(wildcard $(GEOSXEX)) | $(GEOS)
	@test -f $(GEOSXEX) || { echo "no $(GEOSXEX): make -C $(GEOS_ATARI) DRIVE=siodrv_atari SIO_UNIT=2"; exit 1; }
	rm -rf $(GEOS)/boot && mkdir $(GEOS)/boot
	cp $(GEOSXEX) $(GEOS)/boot/GEOS.XEX
	dir2atr -a -b MyPicoDos406 $@ $(GEOS)/boot >/dev/null
geos-atr: $(GEOS)/syn-ack-wire-geos.atr $(GEOS)/syn-ack-wire-geos-boot.atr
# plays by itself and BRKs for screen dumps (test/autoplay.c): tools/run-geos.sh
$(GEOS)/main-auto.s: src/common/main.c $(HDRS) | $(GEOS)
	$(CC) $(GEOSFLAGS) $(DEFS) -DAUTOPLAY -o $@ $<
$(GEOS)/autoplay.s: test/autoplay.c $(HDRS) | $(GEOS)
	$(CC) $(GEOSFLAGS) $(DEFS) -DAUTO_HANDLE='"GEOS"' -o $@ $<
GEOSAUTO = $(GEOS)/autoplay.o $(GEOS)/main-auto.o $(filter-out $(GEOS)/main.o,$(GEOSOBJ))
$(GEOS)/synack-auto.cvt: $(GEOSAUTO)
	$(LD) -t geos-cbm -S 0x0500 -D __STACKSIZE__=0x200 $(GEOSLD) -m $(GEOS)/synack-auto.map -Ln $(GEOS)/synack-auto.lbl -o $@ $(GEOSAUTO) geos-cbm.lib

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

# --- the same for the NES display (src/nes/display_text.c, over test/nes_text_pc.c): build/pc/pc-nes [host] [port]
pc-nes: $(HDRS) src/nes/title_art.h src/nes/text.h
	mkdir -p build/pc
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -DAUTOPLAY -DDUMP_TICKS=240 -DDISPLAY_IDLE=1 \
	    -DNARROW_SAYS=1 -DPC_SCREEN=txt_cells -DPC_NES -DAUTO_HANDLE='"NES"' -o build/pc/pc-nes \
	    src/common/main.c src/common/game.c src/common/proto.c src/nes/display_text.c src/nes/input.c \
	    test/nes_text_pc.c test/autoplay.c test/pc_harness.c

# --- the same for the Adam display (src/adam/display_text.c, over test/adam_text_pc.c): build/pc/pc-adam [host] [port]
pc-adam: $(HDRS) src/adam/title_art.h src/adam/text.h
	mkdir -p build/pc
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -DAUTOPLAY -DDUMP_TICKS=240 -DDISPLAY_IDLE=1 \
	    -DNARROW_SAYS=1 -DSAY_MAXBUY='"3 PER TURN: RETURN TO BUY"' -DPC_SCREEN=txt_cells -DPC_ADAM -DAUTO_HANDLE='"ADAM"' \
	    -o build/pc/pc-adam src/common/main.c src/common/game.c src/common/proto.c src/adam/display_text.c \
	    src/adam/input.c test/adam_text_pc.c test/autoplay.c test/pc_harness.c
# --- the same for the H89 display (src/h89/display_text.c): build/pc/pc-h89 [host] [port]
pc-h89: $(HDRS) src/h89/screens.h
	mkdir -p build/pc
	gcc -g -std=gnu99 -Wall -Wno-main -Wno-pointer-to-int-cast -funsigned-char -D__fastcall__= -DAUTOPLAY \
	    -DDUMP_TICKS=240 -DDISPLAY_IDLE=1 -DPC_SCREEN=pc_screen -DPC_H89 -DAUTO_HANDLE='"H89"' \
	    -o build/pc/pc-h89 \
	    src/common/main.c src/common/game.c src/common/proto.c src/h89/display_text.c test/autoplay.c test/pc_harness.c

# --- Amiga (stock A500, Kickstart/Workbench 1.3): the common code with src/amiga/, and fujinet-nio-lib (a prebuilt
# build/fujinet-nio-amiga.a in the fujinet-nio workspace, read only: make -C $(FUJINET_NIO_LIB) amiga). nix13, not
# clib2: clib2's start-up opens utility.library, which Kickstart 1.3 doesn't have. tools/run-amiga.sh runs a program
# on an emulated A500 against a FujiNet NIO process, all on spare ports.
AMIGA_B         = build/amiga
AMIGA_BIN      ?= /opt/amiga/bin
FUJINET_NIO_WS ?= $(HOME)/dev/fujinet-nio-workspace
FUJINET_NIO_LIB ?= $(FUJINET_NIO_WS)/repos/fujinet-nio-lib
AMIGA_CC        = $(AMIGA_BIN)/m68k-amigaos-gcc
AMIGA_FLAGS     = -Os -Wall -mcpu=68000 -msoft-float -mcrt=nix13 -funsigned-char -D__fastcall__= -D__KICK13__ \
                  -I$(FUJINET_NIO_LIB)/include -I$(FUJINET_NIO_WS)/repos/fujinet-nio-driver/amiga/include \
                  -I$(AMIGA_B)
AMIGA_NIOLIB    = $(FUJINET_NIO_LIB)/build/fujinet-nio-amiga.a

$(AMIGA_NIOLIB):
	@echo "$@ is missing: build it with  make -C $(FUJINET_NIO_LIB) amiga  (or set FUJINET_NIO_LIB)" >&2; false

$(AMIGA_B):
	mkdir -p $@

$(AMIGA_B)/dial: test/amiga/dial.c $(AMIGA_NIOLIB) | $(AMIGA_B)
	$(AMIGA_CC) $(AMIGA_FLAGS) test/amiga/dial.c $(AMIGA_NIOLIB) -o $@

# the drawing primitives on the rig: tools/run-amiga.sh build/amiga/gfxdemo, then amiga_guest.py shot
$(AMIGA_B)/gfxdemo: test/amiga/gfxdemo.c src/amiga/gfx.c src/amiga/layout.c $(wildcard src/amiga/*.h) | $(AMIGA_B)
	$(AMIGA_CC) $(AMIGA_FLAGS) test/amiga/gfxdemo.c src/amiga/gfx.c src/amiga/layout.c -o $@
	@echo "$@: $$(stat -c %s $@) bytes"

# the game: the common code and src/amiga/, which draws it on the A500's screen (display_gfx.c)
AMIGASRC = src/common/main.c src/common/game.c src/common/proto.c \
           $(addprefix src/amiga/,net_fn.c input.c keys.c layout.c ui.c gfx.c pointer.c dialog.c display_gfx.c help.c menudef.c menus.c start.c sound.c sndlogic.c prefs.c prefs_io.c palcycle.c title_art.c)
AMIGADEFS = -DFANCY_TITLE=1 -DFANCY_PLAYFIELD=1 -DDISPLAY_IDLE=1 -DTITLE_IDLE=1 -DQUIT_TO_OS=1 -DQUIT_ASK='"QUIT? Y/N"' -Wno-main $(AMIGA_XDEFS)   # AMIGA_XDEFS: extras for one build, e.g. -DDRAWLOG
$(AMIGA_B)/syn-ack-wire-amiga: $(AMIGASRC) $(HDRS) $(wildcard src/amiga/*.h) $(AMIGA_B)/help_tab.h $(AMIGA_B)/regions_tab.h $(AMIGA_NIOLIB) | $(AMIGA_B)
	$(AMIGA_CC) $(AMIGA_FLAGS) $(DEFS) $(AMIGADEFS) $(AMIGASRC) $(AMIGA_NIOLIB) -o $@
	@echo "$@: $$(stat -c %s $@) bytes"
amiga: $(AMIGA_B)/syn-ack-wire-amiga
# the autoplay build: plays by itself (make amiga-test URL=N:TCP://127.0.0.1:6599/), no screen dumps
$(AMIGA_B)/synack-amiga-auto: $(AMIGASRC) test/autoplay.c test/amiga_dump.c $(HDRS) $(wildcard src/amiga/*.h) $(AMIGA_B)/help_tab.h $(AMIGA_B)/regions_tab.h $(AMIGA_NIOLIB) | $(AMIGA_B)
	$(AMIGA_CC) $(AMIGA_FLAGS) $(DEFS) $(AMIGADEFS) -DAUTOPLAY -DDUMP_TICKS=0 -DKEY_TICKS=6 -DAUTO_HANDLE='"AMIGA"' \
	    $(AMIGASRC) test/autoplay.c test/amiga_dump.c $(AMIGA_NIOLIB) -o $@
	@echo "$@: $$(stat -c %s $@) bytes"
amiga-test: $(AMIGA_B)/synack-amiga-auto
# the same, answering by mouse: every key the autoplayer would press for a prompt is a click on its widget
$(AMIGA_B)/synack-amiga-mouse: $(AMIGASRC) test/autoplay.c test/amiga_autoclick.c test/amiga_dump.c $(HDRS) $(wildcard src/amiga/*.h) $(AMIGA_B)/help_tab.h $(AMIGA_B)/regions_tab.h $(AMIGA_NIOLIB) | $(AMIGA_B)
	$(AMIGA_CC) $(AMIGA_FLAGS) $(DEFS) $(AMIGADEFS) -DAUTOPLAY -DAUTOCLICK -DDUMP_TICKS=0 -DKEY_TICKS=6 -DAUTO_HANDLE='"AMIGA"' \
	    $(AMIGASRC) test/autoplay.c test/amiga_autoclick.c test/amiga_dump.c $(AMIGA_NIOLIB) -o $@
	@echo "$@: $$(stat -c %s $@) bytes"
amiga-test-mouse: $(AMIGA_B)/synack-amiga-mouse

# the Amiga client's look: the mockup generator is the source of the palette (src/amiga/palette.h), and draws the PNGs
src/amiga/palette.h: tools/amiga_mock.py ../theme/theme.toml
	python3 tools/amiga_mock.py --header > $@
# the instruction pages (help/amiga/screen), compiled in from build/amiga/help_tab.h (generated, not committed); the QR code links to the Amiga manual by the fly.io short link
AMIGAHELP = $(sort $(wildcard help/amiga/screen/HELP*.TXT))
AMIGA_QR_URL ?= HTTPS://SYN-ACK-WIRE.FLY.DEV/AMIGA
$(AMIGA_B)/help_tab.h: $(AMIGAHELP) tools/help_amiga.py tools/help_msdos.py tools/qr.py | $(AMIGA_B)
	python3 tools/help_amiga.py --qr "$(AMIGA_QR_URL)" $@ $(AMIGAHELP)
test-amiga-help:
	python3 tools/test_help_amiga.py
amiga-mock: | $(AMIGA_B)
	mkdir -p $(AMIGA_B)/mock && python3 tools/amiga_mock.py src/nes/font.txt $(AMIGA_B)/mock
test-amiga-mock:
	python3 -m unittest tools/test_amiga_mock.py

test-amiga-net: test/amiga_net_test.c src/amiga/net_fn.c src/platform.h
	mkdir -p build/test
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -Itest/amiga_fake -o build/test/amiga_net_test \
	    test/amiga_net_test.c src/amiga/net_fn.c
	build/test/amiga_net_test

test-amiga-layout: test/amiga_layout_test.c src/amiga/layout.c src/amiga/layout.h src/common/theme_tab.h
	mkdir -p build/test
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -o build/test/amiga_layout_test \
	    test/amiga_layout_test.c src/amiga/layout.c
	build/test/amiga_layout_test

test-amiga-ui: test/amiga_ui_test.c src/amiga/ui.c src/amiga/ui.h src/amiga/layout.c src/amiga/layout.h src/amiga/keys.h src/common/game.h
	mkdir -p build/test
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -o build/test/amiga_ui_test \
	    test/amiga_ui_test.c src/amiga/ui.c src/amiga/layout.c
	build/test/amiga_ui_test

test-amiga-keys: test/amiga_keys_test.c src/amiga/keys.c src/amiga/keys.h src/platform.h
	mkdir -p build/test
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -o build/test/amiga_keys_test \
	    test/amiga_keys_test.c src/amiga/keys.c
	build/test/amiga_keys_test

test-amiga-sound: test/amiga_sound_test.c src/amiga/sndlogic.c src/amiga/sndlogic.h src/platform.h
	mkdir -p build/test
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -o build/test/amiga_sound_test \
	    test/amiga_sound_test.c src/amiga/sndlogic.c
	build/test/amiga_sound_test

# the Region menu's servers, from the same file as the desktop client's: generated, not committed
$(AMIGA_B)/regions_tab.h: ../server/servers.fly.toml ../fly.toml ../tools/gen_regions.py | $(AMIGA_B)
	python3 ../tools/gen_regions.py h > $@
test-amiga-menus: test/amiga_menus_test.c src/amiga/menudef.c src/amiga/menudef.h src/amiga/keys.h $(AMIGA_B)/regions_tab.h
	mkdir -p build/test
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -D__fastcall__= -I$(AMIGA_B) -o build/test/amiga_menus_test \
	    test/amiga_menus_test.c src/amiga/menudef.c
	build/test/amiga_menus_test
test-amiga-palcycle: test/amiga_palcycle_test.c src/amiga/palcycle.c src/amiga/palcycle.h
	mkdir -p build/test
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -o build/test/amiga_palcycle_test \
	    test/amiga_palcycle_test.c src/amiga/palcycle.c
	build/test/amiga_palcycle_test
test-amiga-prefs: test/amiga_prefs_test.c src/amiga/prefs.c src/amiga/prefs.h
	mkdir -p build/test
	gcc -g -std=gnu99 -Wall -Wno-main -funsigned-char -o build/test/amiga_prefs_test \
	    test/amiga_prefs_test.c src/amiga/prefs.c
	build/test/amiga_prefs_test

clean:
	rm -rf build

.PHONY: FORCE all atr atrtest atari-nio test-atari-nio test-atari-nio-6502 test pc pc-apple2 pc-msdos pc-coco pc-nes pc-adam pc-h89 apple2_text apple2_text-po msdos msdos-img msdos-test msdos-wiremon coco coco-dsk coco-auto nes nestest nes-auto h89 h89-disk h89-wiremon adam adamtest adam-auto intv intvtest intv-auto geos geos-atr test-amiga-net test-amiga-keys test-amiga-sound test-amiga-prefs amiga amiga-test test-amiga-mock amiga-mock test-amiga-layout test-amiga-ui amiga-test-mouse test-amiga-help test-amiga-menus test-amiga-palcycle clean
.PRECIOUS: $(B)/%.s $(B)/%.o $(A2)/%.s $(A2)/%.o $(COCO)/%.o $(NES)/%.s $(NES)/%.o $(ADAM)/%.o
.DELETE_ON_ERROR:
