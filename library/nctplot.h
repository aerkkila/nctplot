#ifndef __NCTPLOT_H__
#define __NCTPLOT_H__

#include <nctietue3.h> // Voiko tämän poistaa?
#define nctplot(set_or_var) nctplot_(set_or_var, nct_isset(*(set_or_var)))

struct nctplot_globals {
    long long nanval; // custom nan-value if usenan = 1
    char usenan, coastlines, echo, invert_y;
    unsigned char color_fg[3], color_bg[3];
    size_t cache_size;
};

void nctplot_(void* set_or_var, int isset);
struct nctplot_globals* nctplot_get_globals();

#endif
