#include "../library/nctplot.h"
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct option longopts[] = {
    { "binary",		no_argument,		NULL,	'b' },
    { "dimension",	required_argument,	NULL,	'd' },
    { "help",		no_argument,		NULL,	'h' },
    { "verbose",	no_argument,		NULL,	'v' },
    { "y",		required_argument,	NULL,	'y' },
    { "x",		required_argument,	NULL,	'x' },
    { "datatype",	required_argument,	NULL,	't' },
    { "area",		required_argument,	NULL,	'a' },
    {0},
};
const char* opt_str = "bd:hvx:y:t:a:";

#include "usage.c" // print_usage
#include "binary.c"

const int concatbufflen = 64;

enum {netcdf, binary} filetype = netcdf;

#define match(str, type) if (!strcmp(str, #type)) return type
nc_type str_to_nctype(const char *str) {
    match(str, NC_BYTE);
    match(str, NC_UBYTE);
    match(str, NC_SHORT);
    match(str, NC_USHORT);
    match(str, NC_INT);
    match(str, NC_UINT);
    match(str, NC_INT64);
    match(str, NC_UINT64);
    match(str, NC_FLOAT);
    match(str, NC_DOUBLE);
    match(str, NC_CHAR);
    fprintf(stderr, "Data type %s not recognized\n", str);
    return NC_UBYTE;
}
#undef match

int main(int argc, char** argv) {
    if (argc < 2)
	print_usage(argv[0], 1);

    nct_set* set;
    int opt;
    int x=0, y=0;
    char* concat_arg = NULL;
    char concat_buff[concatbufflen];
    concat_buff[concatbufflen-1] = '\0';
    nc_type bin_dtype = NC_UBYTE;
    char *areas[32];
    int nareas = 0;

    while ((opt = getopt_long(argc, argv, opt_str, longopts, NULL)) >= 0) {
	switch (opt) {
	    case 'b':
		filetype = binary;
		break;
	    case 'd':
		strncpy(concat_buff, optarg, concatbufflen-1);
		concat_arg = concat_buff;
		break;
	    case 'h':
		print_usage(argv[0], 0); // will exit
	    case 'v':
		nct_verbose = nct_verbose_newline;
		break;
	    case 'x':
		x = atoi(optarg);
		filetype = binary;
		break;
	    case 'y':
		y = atoi(optarg);
		filetype = binary;
		break;
	    case 't':
		bin_dtype = str_to_nctype(optarg);
		filetype = binary;
		break;
	    case 'a':
		if (nareas < sizeof(areas)/sizeof(areas[0]))
		    areas[nareas++] = optarg;
		break;
	}
    }

    if ((unsigned)optind >= argc)
	print_usage(argv[0], 2);

    switch (filetype) {
	case netcdf:
	    nct_readflags = nct_ratt | nct_rcoord | nct_rcoordall | nct_rkeep*(argc-optind < 11);
	    set = nct_read_mfnc_ptrptr(argv+optind, argc-optind, concat_arg);
	    break;
	default:
	    warnx("Unknown filetype: %i. Using binary.", filetype);
	    filetype = binary;
	case binary:
	    set = read_binary(argv[optind++], x, y, bin_dtype);
	    for (int i=optind; i<argc; i++) {
		nct_set* set1 = read_binary(argv[i], x, y, bin_dtype);
		nct_concat(set, set1, concat_arg, argc-i-1);
		nct_free1(set1);
	    }
	    break;
    }

    for (int i=0; i<nareas; i++) {
	char *str = strtok(areas[i], ":");
	nct_var *dim;
	if (!str || !(dim = nct_get_var(set, str)))
	    continue;
	if (!(str = strtok(NULL, ":")))
	    continue;
	int reverse = nct_get_floating_last(dim, 1) < nct_get_floating(dim, 0);
	if (str[0]) {
	    double val = atof(str);
	    if (reverse) {
		long ind = nct_bsearch_reversed(dim, val, 1);
		nct_set_length(dim, ind+1);
	    }
	    else {
		long ind = nct_bsearch(dim, val, 0);
		nct_set_start(dim, ind);
	    }
	}
	if (!(str = strtok(NULL, ":")) || str[0] == 0)
	    continue;
	double val = atof(str);
	if (reverse) {
	    long ind = nct_bsearch_reversed(dim, val, 0);
	    nct_set_start(dim, ind);
	}
	else {
	    long ind = nct_bsearch(dim, val, 1);
	    nct_set_length(dim, ind+1);
	}
    }

    nctplot(set);
    if (filetype == binary)
	free_binary(set);
    nct_free1(set);
}
