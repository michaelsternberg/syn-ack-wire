ROOT  := ../..
DOC   := syn-ack-wire-rules

$(DOC).pdf: $(DOC).tex theme.tex
	pdflatex -interaction=nonstopmode -halt-on-error $(DOC).tex >/dev/null
	pdflatex -interaction=nonstopmode -halt-on-error $(DOC).tex | grep -E '^!|Overfull|Output written' || true
	rm -f $(DOC).aux $(DOC).log $(DOC).out

theme.tex: $(ROOT)/theme/theme.toml $(ROOT)/tools/gen_theme.py
	python3 $(ROOT)/tools/gen_theme.py tex > $@

clean:
	rm -f $(DOC).pdf theme.tex $(DOC).aux $(DOC).log $(DOC).out

.PHONY: clean
