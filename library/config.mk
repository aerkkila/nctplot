# shapelib/shplib is used in drawing coastlines. Comment out the following if you don't have shapelib installed.
have_shapelib = 1

# nctproj is used in coordinate transformations. Comment this out if nctietue3 was compiled without nctproj.
have_nctproj = 1

CFLAGS = -Wall -g -fPIC -O3
CC = gcc
libraries += -lnctietue3 -lncurses -lSDL2 -ldl

prefix = /usr/local
includedir = $(prefix)/include
libdir = $(prefix)/lib
shrdir = $(prefix)/share
