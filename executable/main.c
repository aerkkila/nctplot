#include "../library/nctplot.h"

int main(int argc, char** argv) {
    nct_set* set;
    if (argc < 2)
	return 0; // TODO: usage
    nct_readflags = nct_ratt;
    if (argc == 2) {
	set = nct_read_ncf(argv[1], nct_rcoord|nct_ratt);
	goto plot;
    }
    /* The first argument may tell how to concatenate.
       The rest tell which files to concatenate. */
    int argind = 1;
    char* concat_arg = NULL;
    if (argv[argind][0] == '-')
	concat_arg = argv[argind++];
    set = nct_read_nc(argv[argind++]);
    for(; argind < argc; argind++) {
	nct_readm_nc(set1, argv[argind]);
	nct_concat(set, &set1, concat_arg, argc-argind-1);
	nct_free1(&set1);
    }

plot:
    nctplot(set);
    nct_free1(set);
}
