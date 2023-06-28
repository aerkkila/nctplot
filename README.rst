========================
Viewing netcdf form data
========================

This program shows netcdf files as color images.
The window contains only the plot and a colorbar
and other information is shown on the command line.

Some features
-------------
* This works well with tiling window managers / compositors.
* Mouse is not needed for basic use.
* Zooming is possible.
* Multiple files can be opened as one file.
* Files can be edited by drawing with mouse.
* Shows just the image instead of some control panel (compare to ncview).
* Coastlines if user wants them.

Things yet to do
----------------
* Better suport for 1D-variables.
* Support for 4D-variables.
* Support for variable groups.
* Coordinate tranformations.
* Better performance.

Installation
------------
~$ make
~# make install
Library nctietue3 must be installed (github.com/aerkkila/nctietue3).
Other dependencies are SDL2 and ncurses.
An optional dependeny is shapelib for drawing coastlines.
See library/config.mk to install without shapelib.

Usage
-----
The executable is called nctplot.
This can also be used from C-codes with:
>>> #include <nctplot.h>
>>> nctplot(args);
Compile the code with -lnctplot

Keybindings are easily found in the source code:
Search keydown_bindings in library/nctplot.c.
No other documentation exists.

Bugs:
-----
Yes.
