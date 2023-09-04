#include "../library/nctplot.h"
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct option longopts[] = {
    { "dimension",	required_argument,	NULL,	'd' },
    { "help",		no_argument,		NULL,	'h' },
    { "verbose",	no_argument,		NULL,	'v' },
    {0},
};
const char* opt_str = "d:hv";

static const char* usage_ =
"Usage: %s [OPTIONS] file0.nc [file1.nc ...]\n"
"Draw images from netcdf files.\n\nOptions:\n";

static const char* usage[] = {
    " %-22sSpecify the name of the dimension along which to concatenate\n",	// opt 0
    " %-22s  multiple files. It must be either the slowest changing dimension\n",
    " %-22s  or a new dimension to be added.\n",
    " %-22s  \"-v\" has a special meaning: Keep the variables separate.\n",
    " %-22sShow this message and exit.\n",					// opt 1
    " %-22sPrint information about what is read from which files.\n",		// opt 2
};

static const char* usage_args[] = {
    " <name> ", "", "",
};

static int usage_lines_per_opt[] = {3, 0, 0};

static void print_usage(const char* thisprog, int exitcode) {
    printf(usage_, thisprog);
    char help[23];
    int iusage = 0;
    for (int iopt=0; iopt<sizeof(longopts)/sizeof(*longopts)-1; iopt++) {
	sprintf(help, "-%c, --%s%s", longopts[iopt].val, longopts[iopt].name, usage_args[iopt]);
	printf(usage[iusage++], help);
	for (int i=0; i<usage_lines_per_opt[iopt]; i++)
	    printf(usage[iusage++], "");
    }
    exit(exitcode);
}

const int concatbufflen = 64;

int main(int argc, char** argv) {
    if (argc < 2)
	print_usage(argv[0], 1);

    nct_set* set;
    int opt;
    char* concat_arg = NULL;
    char concat_buff[concatbufflen];
    concat_buff[concatbufflen-1] = '\0';

    while ((opt = getopt_long(argc, argv, opt_str, longopts, NULL)) >= 0) {
	switch (opt) {
	    case 'd':
		strncpy(concat_buff, optarg, concatbufflen-1);
		concat_arg = concat_buff;
		break;
	    case 'h':
		print_usage(argv[0], 0); // will exit
	    case 'v':
		nct_verbose = nct_verbose_newline;
		break;
	}
    }

    nct_readflags = nct_ratt | nct_rcoord | nct_rkeep*(argc-optind < 11);
    set = nct_read_mfnc_ptrptr(argv+optind, argc-optind, concat_arg);

    nctplot(set);
    nct_free1(set);
}
