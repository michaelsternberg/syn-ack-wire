ROOT  := ../../..
DOCS  := ../..
DOC   := manual
PDF   := syn-ack-wire-manual-msdos
THEME := $(DOCS)/style/theme.tex
SHOTS := screens
# style/ (theme.tex) and docs/ itself (rules/*.tex) are on TeX's search path
LATEX := TEXINPUTS=.:$(DOCS)/style:$(DOCS): pdflatex -interaction=nonstopmode -halt-on-error -jobname=$(PDF)
# Captures drawn from screen dumps (make screens). A dump named after a scenario in docs/scenarios.toml comes from
# a staged game (server/stage.py); title and lobby-join from a lobby with two test clients waiting
# (client/tools/capture.sh, PLATFORM=msdos: the PC's display code on this PC).
DUMPS := title lobby-join spike-early spike infiltration spread-buy tied-root nullroute game-over
# The IBM MDA/CGA character ROM (5788005.u33), or a MAME isa_ibm_mda.zip holding it, for the font
ROM   ?= $(HOME)/Nextcloud/Projects/mame/roms/isa_ibm_mda.zip

$(PDF).pdf: $(DOC).tex platform.tex $(wildcard $(DOCS)/rules/*.tex $(DOCS)/style/*.sty $(SHOTS)/*.png) $(THEME)
	$(LATEX) $(DOC).tex >/dev/null
	$(LATEX) $(DOC).tex | grep -E '^!|Overfull|Output written' || true
	rm -f $(PDF).aux $(PDF).log $(PDF).out

$(THEME): $(ROOT)/theme/theme.toml $(ROOT)/tools/gen_theme.py
	python3 $(ROOT)/tools/gen_theme.py tex > $@

# Redraw the captures from their dumps; needs the IBM character ROM for the font (ROM=...).
screens:
	for n in $(DUMPS); do \
	    python3 $(ROOT)/client/tools/render_msdos.py $(ROM) $(SHOTS)/dumps/$$n.bin $(SHOTS)/$$n.png || exit 1; done

clean:
	rm -f $(PDF).pdf $(PDF).aux $(PDF).log $(PDF).out

.PHONY: screens clean
