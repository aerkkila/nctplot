# Select the pager to use to display the keybindings when h (help) is pressed.
# Multiple options can be given. The first existing one is selected at compile time.
# One may want to use a text editor with syntax highlighting since the keybindings are in a regular C source file.
# To pass an argument to the program, use a backslash and a space, e.g. nvim\ -R
pager = nvim\ -R vim\ -R $(PAGER) less more

# shapelib/shplib is used in drawing coastlines. Comment out the following if you don't have shapelib installed.
have_shapelib = 1

# This is used in coordinate transformations. Comment this out if nctietue3 was compiled without proj library
have_nctproj = 1

# proj is used in coordinate transformations on coastlines. Comment this out if you don't have proj or coastlines are disabled.
have_proj = 1

# png library is used to save figures. Comment this out if necessary.
have_png = 1

# Graphics work directly on wayland. Alternatively, SDL3 library can be used.
# Comment this out, if you don't have wayland or don't want direct wayland support.
have_wayland = 1

# To print text to the window rather than to terminal when using wayland.
# Without wayland, text can only be printed to terminal.
have_ttra = 1

CFLAGS = -Wall -fPIC -Ofast -O0 -gdwarf-2 -g3
CC = gcc

prefix = /usr/local
includedir = $(prefix)/include
libdir = $(prefix)/lib
shrdir = $(prefix)/share
