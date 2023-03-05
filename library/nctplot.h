#ifndef __NCTPLOT_H__
#define __NCTPLOT_H__

#include <nctietue3.h>
#define nctplot(set_or_var) nctplot_(set_or_var, nct_isset(*(set_or_var)))

void nctplot_(void* set_or_var, int isset);

#endif
