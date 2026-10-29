ROOT  := ../../..
DOCS  := ../..
DOC   := manual
PDF   := syn-ack-wire-manual-h89
THEME := $(DOCS)/style/theme.tex
SHOTS := screens
# style/ (theme.tex) and docs/ itself (rules/*.tex) are on TeX's search path
LATEX := TEXINPUTS=.:$(DOCS)/style:$(DOCS): pdflatex -interaction=nonstopmode -halt-on-error -jobname=$(PDF)
# Captures drawn from screen dumps (make screens). A dump named after a scenario in docs/scenarios.toml comes from
# a staged game (server/stage.py); title and lobby-join from a lobby with two test clients waiting
# (client/tools/capture.sh, PLATFORM=h89: the H89's display code on this PC).
DUMPS := title lobby-join spike-early spike infiltration spread-buy tied-root nullroute game-over
# The H19's character ROM (2716_444-29_h19font.u420), or MAME's heath_tlb.zip holding it, for the font
ROM   ?= $(HOME)/Nextcloud/Projects/mame/roms/heath_tlb.zip

$(PDF).pdf: $(DOC).tex platform.tex $(wildcard $(DOCS)/rules/*.tex $(DOCS)/style/*.sty $(SHOTS)/*.png) $(THEME)
	$(LATEX) $(DOC).tex >/dev/null
	$(LATEX) $(DOC).tex | grep -E '^!|Overfull|Output written' || true
	rm -f $(PDF).aux $(PDF).log $(PDF).out

$(THEME): $(ROOT)/theme/theme.toml $(ROOT)/tools/gen_theme.py
	python3 $(ROOT)/tools/gen_theme.py tex > $@

# Redraw the captures from their dumps; needs the H19's character ROM for the font (ROM=...).
screens:
	for n in $(DUMPS); do \
	    python3 $(ROOT)/client/tools/h89/render.py $(ROM) $(SHOTS)/dumps/$$n.bin $(SHOTS)/$$n.png || exit 1; done

clean:
	rm -f $(PDF).pdf $(PDF).aux $(PDF).log $(PDF).out

.PHONY: screens clean
