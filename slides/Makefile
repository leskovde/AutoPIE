all: slides.pdf

# LaTeX must be run multiple times to get references right
slides.pdf: slides.tex $(wildcard *.tex) slides.xmpdata
	pdflatex $<
	pdflatex $<
	pdflatex $<

clean:
	rm -f *.log *.dvi *.aux *.toc *.lof *.lot *.out *.bbl *.blg *.xmpi
	rm -f slides.pdf