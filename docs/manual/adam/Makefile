ROOT  := ../../..
DOCS  := ../..
DOC   := manual
PDF   := syn-ack-wire-manual-adam
THEME := $(DOCS)/style/theme.tex
SHOTS := screens
# style/ (theme.tex) and docs/ itself (rules/*.tex) are on TeX's search path
LATEX := TEXINPUTS=.:$(DOCS)/style:$(DOCS): pdflatex -interaction=nonstopmode -halt-on-error -jobname=$(PDF)
# Captures drawn from screen dumps (make screens). A dump named after a scenario in docs/scenarios.toml comes from
# a staged game (server/stage.py); title and lobby-join from a lobby with two test clients waiting
# (client/tools/capture.sh, PLATFORM=adam: the Adam display on this PC, make pc-adam).
DUMPS := title lobby-join spike-early spike infiltration spread-buy tied-root nullroute game-over

$(PDF).pdf: $(DOC).tex platform.tex $(wildcard $(DOCS)/rules/*.tex $(DOCS)/style/*.sty $(SHOTS)/*.png) $(THEME)
	$(LATEX) $(DOC).tex >/dev/null
	$(LATEX) $(DOC).tex | grep -E '^!|Overfull|Output written' || true
	rm -f $(PDF).aux $(PDF).log $(PDF).out

$(THEME): $(ROOT)/theme/theme.toml $(ROOT)/tools/gen_theme.py
	python3 $(ROOT)/tools/gen_theme.py tex > $@

# Redraw the captures from their dumps, with the client's own tiles and palettes (client/tools/render_adam.py).
screens:
	for n in $(DUMPS); do \
	    python3 $(ROOT)/client/tools/render_adam.py $(SHOTS)/dumps/$$n.bin $(SHOTS)/$$n.png || exit 1; done

clean:
	rm -f $(PDF).pdf $(PDF).aux $(PDF).log $(PDF).out

.PHONY: screens clean
