CC = gcc
CFLAGS = -O2 -g -Wall
prefix = /usr/local
bindir = $(prefix)/bin

all: nctplot

nctplot: main.c
	$(CC) -o $@ $< $(CFLAGS) -lnctplot -lnctietue3

install: nctplot
	cp nctplot $(bindir)

uninstall:
	rm -f $(bindir)/nctplot

clean:
	rm -f nctplot
