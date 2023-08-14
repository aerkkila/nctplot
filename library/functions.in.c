#define ctype @ctype
#define form @form
#define __nctype__ @nctype

void* nct_minmax_@nctype(const nct_var*, void* result); // global but hidden function
void* nct_minmax_nan_@nctype(const nct_var*, long nanval, void* result); // global but hidden function

#define NCTVARDIM(a,b) ((a)->super->dims[(a)->dimids[b]])
#ifndef echo_h
#define echo_h 5
#endif

#define CVAL(val,minmax) ((val) <  (minmax)[0] ? 0   :			\
			  (val) >= (minmax)[1] ? 255 :			\
			  ((val)-(minmax)[0])*255 / ((minmax)[1]-(minmax)[0]) )

/* These isnan functions can be used even with -ffinite-math-only optimization,
   which is part of -Ofast optimization. */
#if __nctype__ == NC_FLOAT
static int my_isnan_float(float f) {
    const unsigned exponent = ((1u<<31)-1) - ((1u<<(31-8))-1);
    uint32_t bits;
    memcpy(&bits, &f, 4);
    return (bits & exponent) == exponent;
}
#elif __nctype__ == NC_DOUBLE
static int my_isnan_double(double f) {
    const long unsigned exponent = ((1lu<<63)-1) - ((1lu<<(63-11))-1);
    uint64_t bits;
    memcpy(&bits, &f, 8);
    return (bits & exponent) == exponent;
}
#endif

static void draw2d_@nctype(const nct_var* var) {
    int usenan = globs.usenan;
    long long nanval = globs.nanval;
    int xlen = nct_get_vardim(var, xid)->len;
    ctype range;
    long dlen = var->len;
    ctype my_minmax[2];
    memcpy(my_minmax, plt.minmax, 2*sizeof(ctype));
    range = my_minmax[1] - my_minmax[0];
    if (minshift_abs != 0) {
	minshift += minshift_abs/range;
	minshift_abs = 0;
    }
    if (maxshift_abs != 0) {
	maxshift += maxshift_abs/range;
	maxshift_abs = 0;
    }
    my_minmax[0] += range*minshift;
    my_minmax[1] += range*maxshift;
    if (prog_mode == variables_m)
	curses_write_vars();

    my_echo(my_minmax);

    size_t offset = plt.znum*plt.stepsize_z*(zid>=0);
    SDL_SetRenderDrawColor(rend, globs.color_bg[0], globs.color_bg[1], globs.color_bg[2], 255);
    SDL_RenderClear(rend);
    if (my_minmax[0] != my_minmax[0]) return; // Return if all values are nan.

    int pixels_per_datum = 1.0 / space;
    float data_per_step = pixels_per_datum * space;
    pixels_per_datum += data_per_step < 1;
    data_per_step = pixels_per_datum*space;

    SDL_RenderSetScale(rend, pixels_per_datum, pixels_per_datum);

    ctype* dataptr = (ctype*)var->data - var->startpos;

    void draw_row(int jpixel, size_t jdata) {
	size_t datastart = jdata*xlen;
	float idata_f = offset_i + 0.5*data_per_step;
	for(int ipixel=0; ipixel<draw_w; ipixel+=pixels_per_datum, idata_f+=data_per_step) {
	    long ind = offset + datastart + (size_t)round(idata_f);
	    if (ind >= dlen)
		return;
	    ctype val = dataptr[ind];
#if __nctype__ == NC_DOUBLE || __nctype__ == NC_FLOAT
#if __nctype__ == NC_DOUBLE
	    if (my_isnan_double(val))
#else
	    if (my_isnan_float(val))
#endif
		continue;
#endif
	    if (usenan && val==nanval)
		continue;
	    int value = CVAL(val,my_minmax);
	    if (invert_c) value = 0xff-value;
	    char* c = COLORVALUE(cmapnum,value);
	    SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 0xff);
	    SDL_RenderDrawPoint(rend, ipixel/pixels_per_datum, jpixel/pixels_per_datum);
	}
    }

    float fdataj = offset_j + 0.5*data_per_step;
    if (globs.invert_y)
	for(int j=draw_h-pixels_per_datum; j>=0; j-=pixels_per_datum) {
	    draw_row(j, round(fdataj));
	    fdataj += data_per_step;
	}
    else
	for(int j=0; j<draw_h; j+=pixels_per_datum) {
	    draw_row(j, round(fdataj));
	    fdataj += data_per_step;
	}
    draw_colormap();
}
#undef CVAL

static void draw1d_@nctype(const nct_var* var) {
    ctype my_minmax[2], range;
    memcpy(my_minmax, plt.minmax, 2*sizeof(ctype));
    range = my_minmax[1]-my_minmax[0];
    my_minmax[0] += range*minshift;
    my_minmax[1] += range*maxshift;
    if (my_minmax[1] == my_minmax[0])
	my_minmax [1] += 1;
    if (prog_mode == variables_m)
	curses_write_vars();
    my_echo(my_minmax);
    SDL_SetRenderDrawColor(rend, globs.color_bg[0], globs.color_bg[1], globs.color_bg[2], 255);
    SDL_RenderClear(rend);
    if (my_minmax[0] != my_minmax[0]) return;
    double di=0;
    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);
    ctype* dataptr = (ctype*)var->data - var->startpos;
    for(int i=0; i<win_w; i++, di+=space) {
	int y = (dataptr[(int)di] - my_minmax[0]) * win_h / (my_minmax[1]-my_minmax[0]);
	SDL_RenderDrawPoint(rend, i, y);
    }
}

#undef ctype
#undef form
#undef __nctype__
