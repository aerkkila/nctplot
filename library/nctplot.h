#ifndef __NCTPLOT_H__
#define __NCTPLOT_H__

#include <nctietue3.h> // Voiko tämän poistaa?
#define nctplot(set_or_var) nctplot_(set_or_var, nct_isset(*(set_or_var)))

struct nctplot_globals {
    long long nanval; // custom nan-value if usenan = 1
    char usenan, coastlines, echo, invert_y, exact, invert_c;
    int cmapnum;
    unsigned char color_fg[3], color_bg[3];
    size_t cache_size;
};

void* nctplot_(void* set_or_var, int isset); // returns the input pointer
struct nctplot_globals* nctplot_get_globals();

typedef struct {
    nct_var *var, *zvar;
    unsigned truncated : 1,
	     use_threshold : 1,
	     globs_detached : 1,
	     use_cmapfun : 1;
    nct_anyd time0;
    char minmax[8*2];
    size_t stepsize_z;
    float minshift, maxshift;
    double threshold;
    int n_threshold;
    struct shown_area_xy *area_xy;
    struct shown_area_z *area_z;
} nct_plottable;

extern nct_plottable *nct_plottables;
extern int nct_pltind, nct_prev_pltind, nct_nplottables;

#endif
