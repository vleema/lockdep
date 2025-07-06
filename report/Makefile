all: clean
	lualatex main.tex
	make clean

clean:
	rm ./**/*.aux ./**/*.log ./**/*.out ./**/*.out || true
	rm main.aux main.log main.out ./main.lof ./main.lot ./main.toc || true

distclean: clean
	rm main.pdf
