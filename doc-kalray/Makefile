SHELL := /bin/bash

RUBBER_OPTS := $(RUBBER_OPTS)$(shell echo '\newcommand{\prefix}{'$$(echo $(PREFIX) | sed -e 's/_/\\_/g')'}')

convert := 	$(shell which convert)
epstopdf := 	$(shell which epstopdf)
inkscape := 	$(shell which inkscape)

texfiles:=	$(texfiles) $(shell ls *.tex | grep -v rubtmp)
texfiles:=  $(sort $(texfiles))
svgfiles:=	$(wildcard *.svg)
pdffiles:=	$(texfiles:.tex=.pdf)

rulesfile:=	rules.ini

all: pdf

install: $(pdffiles)
	mkdir -p $(DOCDIR)
	install -- $(pdffiles) $(DOCDIR)

dvi: $(dvifiles)

pdf: $(pdffiles)

$(pdffiles): %.pdf : %.tex $(svgfiles) $(rulesfile) $(deps) ${PNGS} $(build_deps)
	echo '$(RUBBER_OPTS)\input{$<}' >rubtmp.tex
	rubber $(RUBBER_INCLUDES) $(shell cat includespath.list) -m pdftex -c "rules $(rulesfile)" rubtmp.tex
	mv rubtmp.pdf $@
	mv rubtmp.aux $*.aux
	rm rubtmp*


touch:
	touch $(texfiles)

echo:
	@echo "texfiles:	$(texfiles)"
	@echo "dvifiles:	$(dvifiles)"
	@echo "clsfiles:	$(clsfiles)"

clean:
	rm -Rf $(texfiles:.tex=.ps) *.pdf  *.toc *.aux *.bbl *.blg *.dvi *.log *.out $(indexfile) *.ilg *.ind $(deps) $(extra_clean) rubtmp*.tex *.idx

