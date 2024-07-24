/* This is included into the main file. */

static const char* usage_ =
"Usage: %s [OPTIONS] file0 [file1 ...]\n"
"Display netcdf and other files as images.\n\nOptions:\n";

static const char* usage[] = {
    " %-22sSet filetype as (raw) binary. Default is netcdf.\n",			// opt 0 b
    " %-22sSpecify the name of the dimension along which to concatenate\n",	// opt 1 d
    " %-22s  multiple files. It must be either the slowest changing dimension\n",
    " %-22s  or a new dimension to be added.\n",
    " %-22s  \"-v\" has a special meaning: Keep the variables separate.\n",
    " %-22sShow this message and exit.\n",					// opt 2 h
    " %-22sPrint information about what is read from which files.\n",		// opt 3 v
    " %-22sSpecify the length of y (vertical) dimension.\n",			// opt 4 y
    " %-22s  Sets filetype as binary as otherwise this has no effect.\n",
    " %-22sSpecify the length of x (horizonal) dimension.\n",			// opt 5 x
    " %-22s  Sets filetype as binary as otherwise this has no effect.\n",
    " %-22sSpecify binary file datatype: NC_BYTE, NC_INT, NC_FLOAT, etc.\n",	// opt 6 t
    " %-22s  See [/usr/include/?]netcdf.h for all types. Default is NC_UBYTE.\n",
    " %-22s  Sets filetype as binary as otherwise this has no effect.\n",
};

static const char* usage_args[] = {
    /*b*/"", /*d*/" <name> ", /*h*/"", /*v*/"", /*y*/" <length> ", /*x*/" <length> ", /*t*/ " <nc_type> ",
};

static int extra_lines_per_opt[] = {/*b*/0, /*d*/3, /*h*/0, /*v*/0, /*y*/1, /*x*/1, /*t*/2,};

static void print_usage(const char* thisprog, int exitcode) {
    printf(usage_, thisprog);
    char help[23];
    int iusage = 0;
    for (int iopt=0; iopt<sizeof(longopts)/sizeof(*longopts)-1; iopt++) {
	sprintf(help, "-%c, --%s%s", longopts[iopt].val, longopts[iopt].name, usage_args[iopt]);
	printf(usage[iusage++], help);
	for (int i=0; i<extra_lines_per_opt[iopt]; i++)
	    printf(usage[iusage++], "");
    }
    exit(exitcode);
}
