# shplib is used in drawing coastlines. Remove the following if you don't have shplib installed.
HAVE_SHPLIB = -DHAVE_SHPLIB
libraries += -lshp -lm

CFLAGS = -Wall -g -fPIC -O3 $(HAVE_SHPLIB)
CC = gcc
libraries += -lnctietue3 -lncurses -lSDL2 -ldl

prefix = /usr/local
includedir = $(prefix)/include
libdir = $(prefix)/lib
shrdir = $(prefix)/share
