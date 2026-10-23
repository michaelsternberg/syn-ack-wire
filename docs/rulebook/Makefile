ROOT  := ../..
DOCS  := ..
DOC   := rulebook
PDF   := syn-ack-wire-rulebook
THEME := $(DOCS)/style/theme.tex
# style/ (theme.tex) and docs/ itself (rules/*.tex) are on TeX's search path
LATEX := TEXINPUTS=.:$(DOCS)/style:$(DOCS): pdflatex -interaction=nonstopmode -halt-on-error -jobname=$(PDF)

$(PDF).pdf: $(DOC).tex $(wildcard $(DOCS)/rules/*.tex) $(THEME)
	$(LATEX) $(DOC).tex >/dev/null
	$(LATEX) $(DOC).tex | grep -E '^!|Overfull|Output written' || true
	rm -f $(PDF).aux $(PDF).log $(PDF).out

$(THEME): $(ROOT)/theme/theme.toml $(ROOT)/tools/gen_theme.py
	python3 $(ROOT)/tools/gen_theme.py tex > $@

clean:
	rm -f $(PDF).pdf $(PDF).aux $(PDF).log $(PDF).out

.PHONY: clean
