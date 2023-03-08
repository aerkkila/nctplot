=============================
Plotting data to color images
=============================

This is like ncview but subjectively better.
The purpose of this software is to show 3-dimensional data as 2-dimensional color images.
Data with 4-dimensions (each value has 3 coordinates) is shown as video where the slowest changing dimension is time coordinate.
Data with 2-dimensions (each value has 1 coordinate) can be plotted primitively but this is buggy and not recommended.

This is meant to be run from the command line.
The window contains only the color image and a colorbar.
All other information is shown only on the command line.

Library
=======
Contains a library called nctplot which works with nctietue3 library.

Minimal usage:
--------------
.. code:: c
nct_set* set = nct_read_nc("some-file.nc");
nctplot(set);
nct_free1(set);

Executable
===========
Contains a small executable which reads netcdf files, concatenates them if needed and calls the library to plot them.

Improvements to ncview
----------------------
* This works well with tiling window managers / compositors.
* This works also on Wayland.
* One doesn't need a mouse to use this.
* This is easy to use in codes because this is also a library.
* Windows can be enargened steplessly.
* Multiple files can be opened as one file.
* Files can be edited by drawing with mouse.
* This shows just the image and not some control panel.
* This doesn't forcefully draw the coastlines that make seeing the actual data impossible.
* This is a small and therefore hackable library.

How is this worse than ncview?
------------------------------
* 1D-variables are badly supported.
* 4D-variables are not supported.
* Variable groups are not supported.
* One has to know the key bindings.

Bugs:
-----
Yes.
