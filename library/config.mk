# Select the pager to use to display the keybindings when h (help) is pressed.
# Multiple options can be given. The first existing one is selected at compile time.
# One may want to use a text editor with syntax highlighting since the keybindings are in a regular C source file.
# To pass an argument to the program, use a backslash and a space, e.g. nvim\ -R
pager = nvim\ -R vim\ -R $(PAGER) less more

# shapelib/shplib is used in drawing coastlines. Comment out the following if you don't have shapelib installed.
#have_shapelib = 1

# nctproj is used in coordinate transformations. Comment this out if nctietue3 was compiled without nctproj.
have_nctproj = 1

# proj is used in coordinate transformations on coastlines. Comment this out if you don't have proj or coastlines are disabled.
have_proj = 1

# png library is used to generate figures. Comment this out if necessary.
have_png = 1

# Graphics works directly on wayland. Alternatively, SDL2 library can be used.
# Comment this out, if you don't have wayland or don't want direct wayland support.
# With this, also the keybindings are different because SDL2 is not used for events.
# This is good, if your keyboard layout has e.g. arrow keys on 3rd or 4th level, because SDL2 wouldn't recognize that.
have_wayland = 1

CFLAGS = -Wall -g -fPIC -Ofast
CC = gcc

prefix = /usr/local
includedir = $(prefix)/include
libdir = $(prefix)/lib
shrdir = $(prefix)/share
