#include "../library/nctplot.h"
#include <getopt.h>
#include <string.h>

struct option longopts[] = {
    { "verbose",	no_argument,		NULL,	'v' },
    { "dimension",	required_argument,	NULL,	'd' },
    {0},
};

const int concatbufflen = 64;

int main(int argc, char** argv) {
    if (argc < 2)
	return 0; // TODO: usage

    nct_set* set;
    int opt;
    char* concat_arg = NULL;
    char concat_buff[concatbufflen];
    concat_buff[concatbufflen-1] = '\0';

    while ((opt = getopt_long(argc, argv, "vd:", longopts, NULL)) >= 0) {
	switch (opt) {
	    case 'v':
		nct_verbose = nct_verbose_newline;
		break;
	    case 'd':
		strncpy(concat_buff, optarg, concatbufflen-1);
		concat_arg = concat_buff;
		break;
	}
    }

    nct_readflags = nct_ratt | nct_rcoord | nct_rkeep*(argc-optind < 11);
    set = nct_read_mfnc_ptrptr(argv+optind, argc-optind, concat_arg);

    nctplot(set);
    nct_free1(set);
}
