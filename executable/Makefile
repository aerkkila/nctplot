CC = gcc
CFLAGS = -O0 -g -Wall
prefix = /usr/local
bindir = $(prefix)/bin
bashcompdir = $(prefix)/share/bash-completion/completions

all: nctplot

nctplot: main.c
	$(CC) -o $@ $< $(CFLAGS) -L../library -lnctplot -lnctietue3

install: nctplot
	cp nctplot $(bindir)
	if [ -d $(bashcompdir) ]; then cp bash_completion.sh $(bashcompdir)/nctplot; fi

uninstall:
	rm -f $(bindir)/nctplot $(bashcompdir)/nctplot

clean:
	rm -f nctplot
