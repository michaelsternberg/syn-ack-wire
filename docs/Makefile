# The rulebook and every platform's manual (manual/<platform>/), from the shared rules
# text (rules/), style (style/) and theme (theme/theme.toml, as style/theme.tex).
MANUALS := $(patsubst %/Makefile,%,$(wildcard manual/*/Makefile))

all: rulebook $(MANUALS)

rulebook $(MANUALS):
	$(MAKE) -C $@

clean:
	$(MAKE) -C rulebook clean
	for m in $(MANUALS); do $(MAKE) -C $$m clean; done
	rm -f style/theme.tex

.PHONY: all rulebook $(MANUALS) clean
