CC = gcc
CFLAGS = -O0 -g -Wall
prefix = /usr/local
bindir = $(prefix)/bin

all: nctplot

nctplot: main.c
	$(CC) -o $@ $< $(CFLAGS) -L../library -lnctplot -lnctietue3

install: nctplot
	cp nctplot $(bindir)

uninstall:
	rm -f $(bindir)/nctplot

clean:
	rm -f nctplot
