CC = gcc
CFLAGS = -O0 -g -Wall
prefix = /usr/local
bindir = $(prefix)/bin
bashcompdir = $(prefix)/share/bash-completion/completions

all: nctplot

nctplot: main.c usage.c binary.c
	$(CC) -o $@ $< $(CFLAGS) -L../library -lnctplot -lnctietue3 -lm -lnetcdf

$(bashcompdir):
	mkdir -p $(bashcompdir)
	@echo -e "\033[1;93mWarning:\033[0m Created directory \033[1m$(bashcompdir)\033[0m which will not be removed by 'make uninstall'".

install: nctplot | $(bashcompdir)
	cp nctplot $(bindir)
	cp bash_completion.sh $(bashcompdir)/nctplot

uninstall:
	rm -f $(bindir)/nctplot $(bashcompdir)/nctplot

clean:
	rm -f nctplot
