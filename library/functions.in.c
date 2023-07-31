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

    /* Draws a data row to the screen.
     * Data is scaled so that each datum becomes j1-j0 pixels high and wide.
     * j0, j1 and i are window coordinates, jj and (size_t)di are data coordinates */
    void draw_thick_i_line(int j0, int j1, size_t jj) {
	size_t start = jj*xlen;
	int i=0;
	float di = offset_i + 0.5*data_per_step;
	while (i<draw_w) {
	    long ind = offset + start + (size_t)round(di);
	    if (ind >= dlen)
		return;
	    ctype val = ((ctype*)var->data)[ind];
#if __nctype__==NC_DOUBLE || __nctype__==NC_FLOAT
	    if (val != val) {
		di += data_per_step;
		i += pixels_per_datum;
		continue; }
#endif
	    if (usenan && val==nanval) {
		di += data_per_step;
		i += pixels_per_datum;
		continue; }
	    int value = CVAL(val,my_minmax);
	    if (invert_c) value = 0xff-value;
	    char* c = COLORVALUE(cmapnum,value);
	    SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 0xff);
	    int i_ = i;
	    for(int j=j0; j<j1; j++) {
		i_ = i;
		for(int _i=0; _i<pixels_per_datum; _i++)
		    SDL_RenderDrawPoint(rend, i_++, j);
	    }
	    di += data_per_step;
	    i = i_;
	}
    }

    int j0,j1;
    float fdataj = offset_j + 0.5*data_per_step;
    if (globs.invert_y)
	for(j0=j1=draw_h-1; j0>=0; j0=j1) {
	    //float f = 0;
	    //while((--j1>=0) & ((f+=space)<1));
	    //fdataj += f;
	    j1 -= pixels_per_datum;
	    draw_thick_i_line(j1, j0, round(fdataj));
	    fdataj += data_per_step;
	}
    else
	for(j0=j1=0; j0<draw_h; j0=j1) {
	    //float f = 0;
	    //while((++j1<draw_h) & ((f+=space)<1));
	    //fdataj += f;
	    j1 += pixels_per_datum;
	    draw_thick_i_line(j0, j1, round(fdataj));
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
    for(int i=0; i<win_w; i++, di+=space) {
	int y = (((ctype*)var->data)[(int)di] - my_minmax[0]) * win_h / (my_minmax[1]-my_minmax[0]);
	SDL_RenderDrawPoint(rend, i, y);
    }
}

#undef ctype
#undef form
#undef __nctype__
