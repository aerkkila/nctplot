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
* Support for 4D-variables.
* Support for variable groups.
* Better performance.
* Better suport for 1D-variables.

Installation
------------
    >>> ~$ make
    >>> ~# make install

Compile time dependencies in addition to a C compiler:
    * make
    * which

Dependencies:
    * nctietue3 (github.com/aerkkila/nctietue3)
    * SDL2
    * ncurses

Optional dependencies:
    * shapelib (for coastlines)
    * nctietue3::nctproj (requires proj) (for coordinate transformations)
        - nctproj is integrated into nctietue3 unless it was compiled without proj-library
    * proj (for coastlines in different coordinate systems than longitude-latitude)

See library/config.mk to disable optional dependencies.

Usage
-----
The executable is called nctplot.
This can also be used from C-codes with:
    >>> #include <nctplot.h>
    >>> nctplot(args);
Compile the code with -lnctplot

The keybindings are in a separate source file called bindings.h.
This file is used as documentation for the keybindings.
The only key you need to know is h (help), which will display the keybinding file.
See pager in library/config.mk to select the program to display the file.
