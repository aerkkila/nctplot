/* This file defines those functions which must be repeated for each data type.
   They are further processed with Perl program make_functions.pl: in=functions.in.c, out=functions.c.

   No wrapper function is made if function name begins with underscore,
   or function returns ctype.
   */

@startperl // entry for the perl program

#define ctype @ctype
#define form @form
#define __nctype__ @nctype

void* nct_minmax_@nctype(const nct_var*, void* result); // a global but hidden function
void* nct_minmax_nan_@nctype(const nct_var*, long nanval, void* result); // a global but hidden function

static ctype* g_minmax_@nctype = (ctype*)g_minmax;

/* max-min can be larger than a signed number can handle.
   Therefore we cast to the corresponding unsigned type. */
#define CVAL(val,minmax) ((val) <  (minmax)[0] ? 0   :			\
			  (val) >= (minmax)[1] ? 255 :			\
			  (@uctype)((val)-(minmax)[0])*255 / (@uctype)((minmax)[1]-(minmax)[0]) )

static int draw_row_threshold_@nctype(int jpixel, const void* vrowptr, double dthr) {
    float idata_f = plt.area_xy->offset_i;
    const ctype thr = dthr;
    const int cvals[] = {255*1/10, 255*9/10, 255*1/10};
    int count = 0;
    for (int ipixel=0; ipixel<draw_w; ipixel+=g_pixels_per_datum[0], idata_f+=g_data_per_step[0]) {
	long ind = (size_t)round(idata_f);
	if (ind >= g_xlen)
	    return 0;
	ctype val = ((const ctype*)vrowptr)[ind];
#if __nctype__ == NC_DOUBLE
	if (my_isnan_double(val)) continue;
#elif __nctype__ == NC_FLOAT
	if (my_isnan_float(val)) continue;
#endif
	if (globs.usenan && val==globs.nanval)
	    continue;
	count += val >= thr;
	int value = cvals[(val >= thr) + globs.invert_c];
	unsigned char* c = cmh_colorvalue(globs.cmapnum,value);
	set_color(c);
#ifdef HAVE_WAYLAND // the #else would also work but this is more optimal
	draw_point_in_xscale(ipixel/g_pixels_per_datum[0], jpixel/g_pixels_per_datum[1]);
#else
	graphics_draw_point(ipixel/g_pixels_per_datum[0], jpixel/g_pixels_per_datum[1]);
#endif
    }
#ifdef HAVE_WAYLAND // same comment as above
    expand_row_to_yscale(jpixel/g_pixels_per_datum[1]);
#endif
    return count;
}

static void draw_row_@nctype(int jpixel, const void* vrowptr) {
    float idata_f = plt.area_xy->offset_i;
    for (int ipixel=0; ipixel<draw_w; ipixel+=g_pixels_per_datum[0], idata_f+=g_data_per_step[0]) {
	long ind = (size_t)round(idata_f);
	if (ind >= g_xlen)
	    return;
	ctype val = ((const ctype*)vrowptr)[ind];
#if __nctype__ == NC_DOUBLE
	if (my_isnan_double(val)) continue;
#elif __nctype__ == NC_FLOAT
	if (my_isnan_float(val)) continue;
#endif
	if (globs.usenan && val==globs.nanval)
	    continue;
	int value = CVAL(val, g_minmax_@nctype);
	if (globs.invert_c) value = 0xff-value;
	unsigned char* c = cmh_colorvalue(globs.cmapnum,value);
	set_color(c);
#ifdef HAVE_WAYLAND // the #else would also work but this is more optimal
	draw_point_in_xscale(ipixel/g_pixels_per_datum[0], jpixel/g_pixels_per_datum[1]);
#else
	graphics_draw_point(ipixel/g_pixels_per_datum[0], jpixel/g_pixels_per_datum[1]);
#endif
    }
#ifdef HAVE_WAYLAND // same comment as above
    expand_row_to_yscale(jpixel/g_pixels_per_datum[1]);
#endif
}

static void __attribute__((unused)) draw_row_buffer_@nctype(const void* vrowptr, void* buff) {
    float idata_f = plt.area_xy->offset_i;
    void* ptr = buff;
    for (int ipixel=0; ipixel<draw_w;
	    ipixel	+= g_pixels_per_datum[0],
	    idata_f	+= g_data_per_step[0],
	    ptr		+= 3*g_pixels_per_datum[0])
    {
	long ind = (size_t)round(idata_f);
	if (ind >= g_xlen)
	    return;
	ctype val = ((const ctype*)vrowptr)[ind];
#if __nctype__ == NC_DOUBLE
	if (my_isnan_double(val)) continue;
#elif __nctype__ == NC_FLOAT
	if (my_isnan_float(val)) continue;
#endif
	if (globs.usenan && val==globs.nanval)
	    continue;
	int value = CVAL(val, g_minmax_@nctype);
	if (globs.invert_c) value = 0xff-value;
	unsigned char* c = cmh_colorvalue(globs.cmapnum, value);
	/* put value */
	for(int i=0; i<g_pixels_per_datum[0]; i++)
	    memcpy(ptr+i*3, c, 3);
    }
    for (int j=1; j<g_pixels_per_datum[1]; j++, ptr+=draw_w*3)
	memcpy(ptr, buff, draw_w*3);
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

static double get_min_@nctype() {
    return g_minmax_@nctype[0];
}

static double get_max_@nctype() {
    return g_minmax_@nctype[1];
}

static void draw1d_@nctype(const nct_var* var) {
    if (too_small_to_draw)
	return;
    make_minmax_@nctype();
    if (g_minmax_@nctype[1] == g_minmax_@nctype[0])
	g_minmax_@nctype [1] += 1;
    if (prog_mode == variables_m)
	curses_write_vars();
    printinfo(g_minmax_@nctype);
    set_color(globs.color_bg);
    clear_background();
#if __nctype__ == NC_DOUBLE
	if (my_isnan_double(g_minmax_@nctype[0])) return;
#elif __nctype__ == NC_FLOAT
	if (my_isnan_float(g_minmax_@nctype[0])) return;
#endif
    double di=0;
    set_color(globs.color_fg);
    ctype* dataptr = (ctype*)var->data - var->startpos;
    for(int i=0; i<win_w; i++, di+=data_per_pixel[0]) {
	int y = (dataptr[(int)di] - g_minmax_@nctype[0]) * win_h / (g_minmax_@nctype[1]-g_minmax_@nctype[0]);
	graphics_draw_point(i, y);
    }
}

#undef ctype
#undef form
#undef __nctype__
