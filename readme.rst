===============================
Viewing netcdf and binary files
===============================

This program shows netcdf files as color images.

Features
--------
* Works well with tiling window compositors.
* Mouse is not needed for basic use.
* Multiple files can be opened as if they were one file.
* Files can be larger than RAM.
* Coordinate transformations.
* Files can be edited by drawing with mouse.
* Zooming
* Coastlines

Installation
------------
If needed, edit configuration in library/config.mk and excecutable/Makefile, then:

    | make
    | make install

Compile time dependencies in addition to a C compiler:
    * make
    * which

    Internet connection and git are needed on the first compilation to clone the submodules.

Dependencies:
    * nctietue3 (https://codeberg.org/aerkkila/nctietue3)
    * wayland (preferred) **OR** SDL3
    * ncurses

Optional dependencies:
    * shapelib (for coastlines)
    * nctietue3 compiled with proj-library (for coordinate transformations)
    * proj (for coordinate transformations and coastlines)
    * png (for saving the plotted figure)
    * ttra (submodule, requires: freetype2, utf8proc, fontconfig) (for printing text to the window instead of terminal)

See library/config.mk to disable optional dependencies.

Usage
-----
The executable is called nctplot.
There is no man page but running 'nctplot --help' will help.

This can also be used from C-codes with:

    | #include <nctplot.h>
    | nctplot(args); // args is either nct_var* or nct_set*

Compile the code with -lnctplot. Nct_var and nct_set are structs from
the nctietue3 library (see dependencies).

The keybindings are in a separate source file called bindings.h.
This file is used as documentation for the keybindings.
The only key you need to know during the runtime is h (help), which will display the keybinding file.
See pager in library/config.mk to select the program to display the file.
