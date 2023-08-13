#include "../library/nctplot.h"

int main(int argc, char** argv) {
    nct_set* set;
    if (argc < 2)
	return 0; // TODO: usage
    nct_readflags = nct_ratt | nct_rcoord;
    int argind = 1;
    char* concat_arg = NULL;
    if (argv[argind][0] == '-')
	concat_arg = argv[argind++];
    set = nct_read_mfnc_ptrptr(argv+argind, argc-argind, concat_arg);

    nctplot(set);
    nct_free1(set);
}
