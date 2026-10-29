# The Intellivision teaser: build/syn-ack-wire-intv-teaser.mp4 (2:00, 1920 x 1080, 60 fps, -16 LUFS).
#
#   make            the lot: the film ROM, the music, the clips, the video
#   make music      build/music.wav: song.py played on jzIntv's AY-3-8914 (psg.py), about 2 minutes
#   make clips      build/clips/: the client filmed in jzIntv against servers on 127.0.0.1:$(PORT) (clips.py;
#                   ports $(PORT), BoIP 9980+INSTANCE and web 8040+INSTANCE must be free)
#   make stills     PNGs at T seconds: make stills T="12.5 61.2"
#   make check      the captions' characters in the GROM, the clip pictures against render_intv.py's
#
# Needs jzIntv (JZINTV_DIR, default ~/Workspace/jzintv-20200712-src), FujiNet-PC (an RS-232 build; see
# ../client/tools/run-intv.sh), IntyBASIC's source for the client's build (IB_SRC), as1600, ffmpeg, and
# Python with numpy, scipy and PIL.
CLIENT  := ../client
IB      := $(CLIENT)/build/intybasic
PORT    ?= 6598
ROM     := build/rom/syn-ack-wire-intv.rom
OUT     := build/syn-ack-wire-intv-teaser.mp4

all: $(OUT)

$(ROM): $(wildcard $(CLIENT)/src/intv/*.bas)
	$(MAKE) -C $(CLIENT) intv INTV=$(abspath build/rom) URL=N:TCP://127.0.0.1:$(PORT)/ LOBBY_KEY=0

$(IB)/intybasic:
	$(MAKE) -C $(CLIENT) build/intybasic/intybasic build/intybasic/intybasic_prologue.asm \
	    build/intybasic/intybasic_epilogue.asm

build/psgidle.rom: psgidle.bas $(IB)/intybasic
	mkdir -p build
	cd build && $(abspath $(IB))/intybasic ../psgidle.bas psgidle.asm $(abspath $(IB))/
	cd build && as1600 -o psgidle.rom -s psgidle.sym psgidle.asm

build/music.wav: song.py psg.py build/psgidle.rom
	python3 psg.py

build/clips/.done: clips.py $(ROM)
	PORT=$(PORT) python3 clips.py
	touch $@

$(OUT): teaser.py vidlib.py song.py build/music.wav build/clips/.done
	python3 teaser.py --check
	python3 teaser.py $@

music: build/music.wav
clips: build/clips/.done
rom: $(ROM)

stills:
	python3 teaser.py --still $(T)

check:
	python3 teaser.py --check

clean:
	rm -rf build

.PHONY: all music clips rom stills check clean
