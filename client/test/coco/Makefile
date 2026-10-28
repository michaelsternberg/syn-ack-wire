# The CoCo 1/2 screen test (screentest.c): build/coco/screentest.dsk, with SCRTEST.BIN.
#   make -C test/coco                 needs cmoc and lwtools, and toolshed's decb (DECB=path)
#   then in BASIC: LOADM"SCRTEST":EXEC
CMOC    ?= cmoc
DECB    ?= decb
B       = ../../build/coco
FONT    = ../../src/coco/font.h

all: $(B)/screentest.dsk

$(FONT): ../../src/coco/font.txt ../../tools/font_coco.py
	python3 ../../tools/font_coco.py $< > $@
# the game's title art and its static (the client's Makefile makes them the same way)
ART = ../../src/coco/title_art.h ../../src/coco/title_snow.h
../../src/coco/title_art.h: ../../src/atari/title4.atascii ../../src/coco/font.txt ../../tools/title_art.py
	python3 ../../tools/title_art.py k 0 $< ../../src/coco/font.txt > $@
../../src/coco/title_snow.h: ../../src/atari/title4.atascii ../../src/coco/font.txt ../../tools/title_art.py
	python3 ../../tools/title_art.py m 0 $< ../../src/coco/font.txt > $@

$(B):
	mkdir -p $@

# the PMODE 4 screen is at $0E00-$25FF, so the program starts above it
$(B)/scrtest.bin: screentest.c ../../src/coco/text.c ../../src/coco/text.h ../../src/coco/snow.c $(FONT) $(ART) | $(B)
	$(CMOC) --coco --org=2600 -O2 -o $@ screentest.c ../../src/coco/text.c ../../src/coco/snow.c
	@echo "$@: $$(stat -c %s $@) bytes"

$(B)/screentest.dsk: $(B)/scrtest.bin
	rm -f $@
	$(DECB) dskini $@
	$(DECB) copy -2 -b $< $@,SCRTEST.BIN

clean:
	rm -f $(B)/scrtest.bin $(B)/screentest.dsk

.PHONY: all clean
