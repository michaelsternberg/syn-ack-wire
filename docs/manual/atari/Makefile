ROOT  := ../../..
DOCS  := ../..
DOC   := manual
PDF   := syn-ack-wire-manual-atari
THEME := $(DOCS)/style/theme.tex
SHOTS := screens
# style/ (theme.tex) and docs/ itself (rules/*.tex) are on TeX's search path
LATEX := TEXINPUTS=.:$(DOCS)/style:$(DOCS): pdflatex -interaction=nonstopmode -halt-on-error -jobname=$(PDF)
# Captures drawn from screen dumps (make screens): name, then render_screen.py's options.
# A dump named after a scenario in docs/scenarios.toml comes from a staged game (server/stage.py).
DUMPS := spike-early:--playfield:--ask spike:--playfield:--ask infiltration:--playfield:--ask \
         spread-buy:--playfield:--ask tied-root:--playfield:--ask lobby-join:--playfield \
         nullroute:--playfield:--ask game-over:--playfield:--ask
ROM   ?= $(HOME)/rom/ATARIXL.ROM

$(PDF).pdf: $(DOC).tex $(wildcard $(DOCS)/rules/*.tex $(DOCS)/style/*.sty $(SHOTS)/*.png) $(THEME)
	$(LATEX) $(DOC).tex >/dev/null
	$(LATEX) $(DOC).tex | grep -E '^!|Overfull|Output written' || true
	rm -f $(PDF).aux $(PDF).log $(PDF).out

$(THEME): $(ROOT)/theme/theme.toml $(ROOT)/tools/gen_theme.py
	python3 $(ROOT)/tools/gen_theme.py tex > $@

# Redraw the captures from their dumps; needs an XL/XE OS ROM for the font (ROM=...).
screens:
	for d in $(DUMPS); do set -- $$(echo $$d | tr : ' '); n=$$1; shift; \
	    python3 $(ROOT)/client/tools/render_screen.py $(ROM) $(SHOTS)/dumps/$$n.bin $(SHOTS)/$$n.png "$$@" || exit 1; done

clean:
	rm -f $(PDF).pdf $(PDF).aux $(PDF).log $(PDF).out

.PHONY: screens clean
