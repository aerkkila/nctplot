#ifndef __NCTPLOT_H__
#define __NCTPLOT_H__

#include <nctietue3.h> // Voiko tämän poistaa?
#define nctplot(set_or_var) nctplot_(set_or_var, nct_isset(*(set_or_var)))

struct nctplot_shared {
	long long nanval; // custom nan-value if usenan = 1
	char usenan, invert_y, exact, invert_c;
	uint64_t used_features;
	int cmapnum;
	unsigned char color_fg[3], color_bg[3];
};

void* nctplot_(void* set_or_var, int isset); // returns the input pointer
struct nctplot_shared* nctplot_get_shared();

typedef struct {
	nct_var *var, *zvar;
	unsigned noreset			: 1,
			 use_threshold		: 1,
			 shared_detached	: 1,
			 use_cmapfun		: 1,
			 show_cmap			: 1;
	nct_anyd time0;
	char minmax[8*2];
	size_t stepsize_z;
	float minshift, maxshift;
	double threshold;
	int n_threshold;
	struct shown_area_xy *area_xy;
	struct shown_area_z *area_z;
	nct_var varbuff;
} nct_plottable;

extern nct_plottable *nct_plottables;
extern int nct_pltind, nct_prev_pltind, nct_nplottables;

#endif
