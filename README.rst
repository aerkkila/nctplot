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
* Multiple files can be opened as if they were one file.
* Files can be larger than RAM.
* Coordinate transformations.
* Files can be edited by drawing with mouse.
* Shows just the image instead of some annoying control panel (compare to ncview).
* Coastlines if user wants them.

Things yet to do
----------------
* Loading data asynchronously.
* To decide better which data to keep in memory, if everything doesn't fit.
* Averaging data in the drawn pixel. Now we just pick one datum in that area.
* Support for 4D-variables.
* Support for variable groups (needs changes in nctietue3 library).
* To try, if Vulkan would increase performance while drawing high-resolution data.
* Better suport for 1D-variables.

Installation
------------
    * If needed, edit configuration in library/config.mk and excecutable/Makefile, then:
    >>> make
    >>> make install

Compile time dependencies in addition to a C compiler:
    * make
    * which

Dependencies:
    * nctietue3 (github.com/aerkkila/nctietue3)
    * colormap-headers (codeberg.org/aerkkila/colormap-headers)
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
