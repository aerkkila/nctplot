====================
Viewing netcdf files
====================

This program shows netcdf files as color images.
The window contains only the plot and a colorbar
and other information is shown on the command line.

Features
--------
* Works well with tiling window managers / compositors.
* Mouse is not needed for basic use.
* Zooming is possible.
* Multiple files can be opened as one file.
* Coordinate transformations.
* Files can be edited by drawing with mouse.
* Shows just the image instead of some annoying control panel (compare to ncview).
* Coastlines if user wants them.

Things yet to do
----------------
* Coastlines in differents coordinates.
* Better performance.
* Support for 4D-variables.
* Support for variable groups.
* Better suport for 1D-variables.

Installation
------------
    >>> ~$ make
    >>> ~# make install

Dependencies:
    * nctietue3 (github.com/aerkkila/nctietue3)
    * SDL2
    * ncurses

Optional dependencies:
    * shapelib (for coastlines)
    * nctietue3::nctproj (requires proj) (for coordinate transformations)
        - nctproj is integrated into nctietue3 unless it was compiled without proj-library

See library/config.mk to disable optional dependencies.

Usage
-----
The executable is called nctplot.
This can also be used from C-codes with:
    >>> #include <nctplot.h>
    >>> nctplot(args);
Compile the code with -lnctplot

Keybindings are easily found in the source code:
Search keydown_bindings in library/nctplot.c.
No other documentation exists for keybindings.

Bugs:
-----
Yes.
