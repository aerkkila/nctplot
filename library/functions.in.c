#define ctype @ctype
#define form @form
#define __nctype__ @nctype

void* nct_minmax_@nctype(const nct_var*, void* result); // global but hidden function

static void print_value_@nctype(const nct_var* var, size_t pos) {
    printf("%@form", ((@ctype*)var->data)[pos]);
}

#define NCTVARDIM(a,b) ((a)->super->dims[(a)->dimids[b]])

#define A echo_highlight
#define B nct_default_color
static void draw_echo_@nctype(ctype minmax[]) {
    if (!has_echoed++)
	for(int i=0; i<5; i++)
	    putchar('\n');
    printf("\033[5F%s%s%s: min %s%@form%s, max %s%@form%s\033[K\n"
	   "x: %s%s(%zu)%s",
	   A,var->name,B, A,minmax[0],B, A,minmax[1],B,
	   A,NCTVARDIM(var, xid)->name,NCTVARDIM(var, xid)->len,B);
    if (yid>=0)
	printf(", y: %s%s(%zu)%s",
	       A,NCTVARDIM(var, yid)->name,NCTVARDIM(var, yid)->len,B);
    if (zvar) {
	printf(", z: %s%s(%i/%zu",
	       A,NCTVARDIM(var, zid)->name,znum+1,NCTVARDIM(var, zid)->len);
	if (time0.d >= 0) {
	    char help[128];
	    strftime(help, 128, "%F %T", nct_localtime((long)nct_get_integer(zvar,znum), time0));
	    printf(" %s", help);
	}
	printf(")%s", B);
    }
    printf("\033[K\n"
	   "minshift %s%.4f%s, maxshift %s%.4f%s\033[K\n"
	   "space = %s%.4f%s\033[K\n"
	   "colormap = %s%s%s\033[K\n",
	   A,minshift,B, A,maxshift,B,
	   A,space,B, A,colormaps[cmapnum*2+1],B);
}
#undef A
#undef B

#define CVAL(val,minmax) ((val) <  (minmax)[0] ? 0   :			\
			  (val) >= (minmax)[1] ? 255 :			\
			  ((val)-(minmax)[0])*255 / ((minmax)[1]-(minmax)[0]) )

static void draw2d_@nctype(const nct_var* var) {
    int usenan = globs.usenan;
    long long nanval = globs.nanval;
    int xlen = NCTVARDIM(var, xid)->len;
    float dj=0;
    ctype minmax[2], range;
    static ctype minmax_static[2];
    if (update_minmax) {
	update_minmax = 0;
	nct_minmax_@nctype(var, minmax_static);
    }
    memcpy(minmax, minmax_static, 2*sizeof(ctype));
    range = minmax[1]-minmax[0];
    if (minshift_abs != 0) {
	minshift += minshift_abs/range;
	minshift_abs = 0;
    }
    if (maxshift_abs != 0) {
	maxshift += maxshift_abs/range;
	maxshift_abs = 0;
    }
    minmax[0] += range*minshift;
    minmax[1] += range*maxshift;
    if (prog_mode == variables_m)
	curses_write_vars();
    else if (globs.echo)
	draw_echo_@nctype(minmax);

    size_t offset = znum*stepsize_z*(zid>=0);
    SDL_SetRenderDrawColor(rend, globs.color_bg[0], globs.color_bg[1], globs.color_bg[2], 255);
    SDL_RenderClear(rend);
    if (minmax[0] != minmax[0]) return; // Return if all values are nan.

    /* Draws a data row to the screen.
     * Data is scaled so that each datum becomes j1-j0 pixels high and wide.
     * j0, j1 and i are window coordinates, jj and (size_t)di are data coordinates */
    void draw_thick_i_line(int j0, int j1, size_t jj) {
	size_t start = jj*xlen;
	int diff = j1-j0;
	float di = offset_i;
	for(int i=0; i<draw_w; i+=diff, di+=space*diff) {
	    ctype val = ((ctype*)var->data)[offset + start + (size_t)di];
#if __nctype__==NC_DOUBLE || __nctype__==NC_FLOAT
	    if (val != val)
		continue;
#endif
	    if (usenan && val==nanval)
		continue;
	    int value = CVAL(val,minmax);
	    if (invert_c) value = 0xff-value;
	    char* c = COLORVALUE(cmapnum,value);
	    SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 0xff);
	    for(int j=j0; j<j1; j++)
		for(int ii=0; ii<diff; ii++)
		    SDL_RenderDrawPoint(rend, i+ii, j);
	}
	di = 0;
    }

    int j0,j1;
    if (globs.invert_y)
	for(j0=j1=draw_h-1; j0>=0; j0=j1) {
	    size_t cmp = (size_t)dj;
	    // TODO: Tehdäänkö tässä kaikki nan-arvot yksittäin?
	    while((--j1>0) & ((size_t)(dj+=space)==cmp));
	    draw_thick_i_line(j1, j0, dj-space);
	}
    else
	for(j0=j1=0; j0<draw_h; j0=j1) {
	    size_t cmp = (size_t)dj;
	    while((++j1<draw_h) & ((size_t)(dj+=space)==cmp));
	    draw_thick_i_line(j0, j1, dj-space);
	}
    draw_colormap();
}
#undef CVAL

static void draw1d_@nctype(const nct_var* var) {
    ctype minmax[2], range;
    nct_minmax_@nctype(var, minmax);
    range = minmax[1]-minmax[0];
    minmax[0] += range*minshift;
    minmax[1] += range*maxshift;
    if (minmax[1] == minmax[0])
	minmax [1] += 1;
    if (prog_mode == variables_m)
	curses_write_vars();
    else if (globs.echo)
	draw_echo_@nctype(minmax);
    SDL_SetRenderDrawColor(rend, globs.color_bg[0], globs.color_bg[1], globs.color_bg[2], 255);
    SDL_RenderClear(rend);
    if (minmax[0] != minmax[0]) return;
    double di=0;
    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);
    for(int i=0; i<win_w; i++, di+=space) {
	int y = (((ctype*)var->data)[(int)di] - minmax[0]) * win_h / (minmax[1]-minmax[0]);
	SDL_RenderDrawPoint(rend, i, y);
    }
}

#undef ctype
#undef form
#undef __nctype__
