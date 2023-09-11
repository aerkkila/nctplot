/* This file defines the drawing functions: draw1d and draw2d.
   However, we need a function for each data type,
   for example, draw2d_NC_INT, draw2d_NC_FLOAT, ...
   Those are created by Perl program make_functions.pl,
   which uses this file as input and replaces @something with the right things.
   The generated files are called
   autogenerated/{functions_NC_INT.c,functions_NC_FLOAT.c,...}
   */

#define ctype @ctype
#define form @form
#define __nctype__ @nctype

void* nct_minmax_@nctype(const nct_var*, void* result); // global but hidden function
void* nct_minmax_nan_@nctype(const nct_var*, long nanval, void* result); // global but hidden function

#define NCTVARDIM(a,b) ((a)->super->dims[(a)->dimids[b]])
#ifndef echo_h
#define echo_h 5
#endif

static ctype* g_minmax_@nctype = (ctype*)g_minmax;

/* max-min can be larger than a signed number can handle.
   Therefore we cast to the corresponding unsigned type. */
#define CVAL(val,minmax) ((val) <  (minmax)[0] ? 0   :			\
			  (val) >= (minmax)[1] ? 255 :			\
			  (@uctype)((val)-(minmax)[0])*255 / (@uctype)((minmax)[1]-(minmax)[0]) )

static void draw_row_@nctype(int jpixel, size_t jdata, const void* vdataptr) {
    size_t datastart = jdata*g_xlen;
    float idata_f = offset_i + 0.5*g_data_per_step;
    for(int ipixel=0; ipixel<draw_w; ipixel+=g_pixels_per_datum, idata_f+=g_data_per_step) {
	long ind = datastart + (size_t)round(idata_f);
	if (ind >= g_dlen)
	    return;
	ctype val = ((const ctype*)vdataptr)[ind];
#if __nctype__ == NC_DOUBLE
	if (my_isnan_double(val)) continue;
#else
	if (my_isnan_float(val)) continue;
#endif
	if (globs.usenan && val==globs.nanval)
	    continue;
	int value = CVAL(val, g_minmax_@nctype);
	if (globs.invert_c) value = 0xff-value;
	unsigned char* c = cmh_colorvalue(globs.cmapnum,value);
	SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 0xff);
	SDL_RenderDrawPoint(rend, ipixel/g_pixels_per_datum, jpixel/g_pixels_per_datum);
    }
}
#undef CVAL

static int make_minmax_@nctype() {
    @uctype range;
    memcpy(g_minmax_@nctype, plt.minmax, 2*sizeof(ctype));
    range = g_minmax_@nctype[1] - g_minmax_@nctype[0];
    if (minshift_abs != 0) {
	plt.minshift += minshift_abs/range;
	minshift_abs = 0;
    }
    if (maxshift_abs != 0) {
	plt.maxshift += maxshift_abs/range;
	maxshift_abs = 0;
    }
    g_minmax_@nctype[0] += (@uctype)(range*plt.minshift);
    g_minmax_@nctype[1] += (@uctype)(range*plt.maxshift);

    return g_minmax_@nctype[0] == g_minmax_@nctype[1];
}

static void draw1d_@nctype(const nct_var* var) {
    make_minmax_@nctype();
    if (g_minmax_@nctype[1] == g_minmax_@nctype[0])
	g_minmax_@nctype [1] += 1;
    if (prog_mode == variables_m)
	curses_write_vars();
    my_echo(g_minmax_@nctype);
    SDL_SetRenderDrawColor(rend, globs.color_bg[0], globs.color_bg[1], globs.color_bg[2], 255);
    SDL_RenderClear(rend);
#if __nctype__ == NC_DOUBLE
	if (my_isnan_double(g_minmax_@nctype[0])) return;
#else
	if (my_isnan_float(g_minmax_@nctype[0])) return;
#endif
    double di=0;
    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);
    ctype* dataptr = (ctype*)var->data - var->startpos;
    for(int i=0; i<win_w; i++, di+=data_per_pixel) {
	int y = (dataptr[(int)di] - g_minmax_@nctype[0]) * win_h / (g_minmax_@nctype[1]-g_minmax_@nctype[0]);
	SDL_RenderDrawPoint(rend, i, y);
    }
}

#undef ctype
#undef form
#undef __nctype__
