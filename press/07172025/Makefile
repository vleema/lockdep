all:
	pdflatex -interaction=nonstopmode ./report.tex || true
	pdflatex -interaction=nonstopmode ./report.tex || true
	rm *.aux *.nav *.vrb *.log *.out *.snm *.toc
	firefox --new-window ./report.pdf

clean:
	rm *.aux *.nav *.vrb *.log *.out *.snm *.toc
