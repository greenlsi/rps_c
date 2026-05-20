prefix=rps-

pdf: $(prefix)c.pdf

PANDOC_FLAGS = \
	--from markdown \
	--template eisvogel \
	-M date="$(shell date +%Y-%m-%d)" \
	-M titlepage=true \
	-M titlepage-rule-height=2 \
	-M listings=true \
	-M book=true \
	--syntax-highlighting idiomatic \
	--toc \
	--toc-depth=3 \
	--lof \
	-V caption-justification=centering \
	-V documentclass=report \
	--top-level-division=chapter \
	--filter pandoc-crossref \
	--filter mermaid-filter \
	--pdf-engine=xelatex

$(prefix)c.pdf: transformar-fsm-a-raft-rx.md
	@echo "Generating $@..."
	@pandoc $^ -o $@ $(PANDOC_FLAGS) \
		-M title="Transformar una aplicacion FSM a raft_rx" \
		-M subtitle="Versión para C"

clean:
	rm -f $(prefix)c.pdf
