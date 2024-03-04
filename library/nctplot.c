#include <nctietue3.h>
#include <cmh_colormaps.h>
#include <stdint.h>
#include <curses.h>
#include <sys/time.h>
#include <err.h>
#include <dlfcn.h> // dlopen, dlerror, etc. for mousepaint
#include <unistd.h> // fork, STDIN_FILENO
#include <sys/wait.h> // waitpid
#include <termios.h>
#include "nctplot.h"
#include "autogenerated/pager.h" // pager_path, pager_args
#ifdef HAVE_NCTPROJ
#include <nctproj.h>
#endif

static const struct nctplot_globals default_globals = {
    .color_fg = {255, 255, 255},
    .echo = 1,
    .invert_y = 1,
    .exact = 1,
    .cache_size = 1L<<31,
    .cmapnum = cmh_jet_e,
};

static struct nctplot_globals globs;
static struct nctplot_globals globs_mem;
static struct nctplot_globals* globslist;
int globslistlen;

struct shown_area;

typedef struct {
    nct_var *var, *zvar;
    nct_anyd time0;
    char minmax[8*2];
    size_t stepsize_z;
    float minshift, maxshift;
    double threshold;
    int use_threshold;
    char globs_detached;
    struct shown_area *area;
} plottable;

/* Static variables. Global in the context of this library. */
static plottable* plottables;
static int pltind, prev_pltind, n_plottables;
#define plt (plottables[pltind])
static nct_var* var; // = plt.var
static WINDOW *wnd;
static const unsigned default_sleep_ms=8;
static const double default_fps=50;
static unsigned sleeptime;
static double fps;
static int win_w, win_h, xid, yid, zid, draw_w, draw_h, pending_varnum=-1, pending_cmapnum;
static char stop, fill_on, play_on, update_minmax=1, update_minmax_cur, too_small_to_draw;
static int lines_printed;
static int cmappix=30, cmapspace=10, call_redraw;
static float minshift_abs, maxshift_abs, zoom=1;
static float data_per_pixel; // (n(data) / n(pixels)) in one direction
static const char* echo_highlight = "\033[1;93m";
static void (*draw_funcptr)(const nct_var*);
static enum {no_m, variables_m=-100, colormaps_m, n_cursesmodes, mousepaint_m} prog_mode = no_m;
/* drawing parameters */
static float g_data_per_step;
static int g_pixels_per_datum, g_xlen, g_ylen, g_size1, g_only_nans, g_extended_y;
static char g_minmax[2*8]; // a buffer, which is used with a g_minmax_@nctype pointer
/* When coming backwards to a 0-dimensional variable, we want to jump to previous and not to next. */
static char _variable_changed_direction = 1;

typedef union Arg Arg;
typedef struct Binding Binding;

static int my_isnan_float(float f);
static int my_isnan_double(double f);
static void printinfo(void* minmax);
static void redraw(nct_var* var);
static void multiply_zoom_fixed_point(float multiple, float xfraction, float yfraction);
static void draw_colormap();
static void set_dimids();
static void set_draw_params();
static void end_curses(Arg);
static void curses_write_vars();
static void curses_write_cmaps();
static uint_fast64_t time_now_ms();
static void inc_offset_j(Arg);
static void inc_offset_i(Arg);
static void inc_znum(Arg intarg);
static void quit(Arg _);
static void var_ichange(Arg jump);
static void keydown_func(int keysym, unsigned mod);
static void _maybe_print_mousecoordinate(int vardimid, int at);
static void mousemotion(int xrel, int yrel);
static void mousewheel(int num);
static void mousemove(float xrel, float yrel);
static void mousepaint();
static int prompt_floating(double *out, const char *msg, ...);
static int prompt_integer(long long *out, const char *msg, ...);
static int prompt_string(char *out, int outlen, const char *msg, ...);

union Arg {
    void* v;
    float f;
    int   i;
};

#define size_coastl_params (sizeof(int)*2 + sizeof(data_per_pixel) + sizeof(globs.invert_y) + sizeof(win_w)*2)

struct shown_area {
    int offset_i, offset_j, znum, nusers, j_off_by_one; // nusers = 0, when 1 user
    nct_var *xdim, *ydim, *zdim;
    /* for coastlines */
    double* coasts;	// coordinates of coastlines
    void* points;	// pixelcoordinates of coastlines
    int *breaks, nbreaks; // indices where to lift pen from the paper
    char* crs;
    double x0, y0, xunits_per_datum, yunits_per_datum;
    char coastl_params[size_coastl_params]; // to tell if coastlines need to be redrawn
};

#define ARRSIZE(a) (sizeof(a) / sizeof(*(a)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct Binding {
    int key;
    int mod;
    void (*fun)(Arg);
    Arg arg;
};

struct Mp_params {
    enum {fixed_mp, function_mp} mode;
    nct_any value;
    int size;
    void* dlhandle;
    void* (*fun)(void*,void*);
    char buff[8];
    char filename[256];
}
mp_params = {0};

static int iround(float f) {
    int ifloor = f;
    return ifloor + (f-ifloor >= 0.5) - (f-ifloor <= -0.5);
}

static inline int __attribute__((pure)) additional_height() {
    return cmapspace + cmappix;
}

#ifdef HAVE_WAYLAND
#include "wayland_spesific.c"
#else
#include "sdl_spesific.c"
#endif

#include "functions.c" // draw1d, draw_row, make_minmax; automatically generated from functions.in.c
#include "coastlines.c"
#include "png.c"

/* These isnan functions can be used even with -ffinite-math-only optimization,
   which is part of -Ofast optimization. */
static int my_isnan_float(float f) {
    const unsigned exponent = ((1u<<31)-1) - ((1u<<(31-8))-1);
    uint32_t bits;
    memcpy(&bits, &f, 4);
    return (bits & exponent) == exponent;
}

static int my_isnan_double(double f) {
    const long unsigned exponent = ((1lu<<63)-1) - ((1lu<<(63-11))-1);
    uint64_t bits;
    memcpy(&bits, &f, 8);
    return (bits & exponent) == exponent;
}

static int recalloc_list(void* varr, int *has, int wants, int size1, const void *fill) {
    void** arr = varr;
    if (*has >= wants)
	return 0;
    void *tmp = realloc(*arr, wants*size1);
    if (!tmp) {
	warn("realloc %i in %s: %i", wants*size1, __FILE__, __LINE__);
	return -1;
    }
    *arr = tmp;
    if (fill)
	for (int i=0; i<wants-*has; i++)
	    memcpy(*arr+(*has+i)*size1, fill, size1);
    else
	memset(*arr+*has*size1, 0, (wants-*has)*size1);
    *has = wants;
    return 1;
}

static void curses_write_vars() {
    int att = COLOR_PAIR(1);
    int xlen, ylen, x;
    getmaxyx(wnd, ylen, xlen);
    int objs=0, row=0, col=0;
    clear();
    int max_x = 0;
    nct_foreach(var->super, svar) {
	objs++;
	move(row, col);
	int att1 = (svar==var)*(A_UNDERLINE|A_BOLD);
	int att2 = (nct_varid(svar)==pending_varnum)*(A_REVERSE);
	attron(att|att1|att2);
	printw("%i. ", objs);
	attroff(att);
	printw("%s", svar->name);
	attroff(att1|att2);
	getyx(wnd, row, x);
	if(x > max_x)
	    max_x = x;
	if(++row >= ylen) {
	    row = 0;
	    if((col=max_x+4) >= xlen) break;
	}
    }
    refresh();
}

static void curses_write_cmaps() {
    int att = COLOR_PAIR(1);
    int xlen, ylen, x;
    getmaxyx(wnd, ylen, xlen);
    int objs=0, row=0, col=0;
    clear();
    int max_x = 0;
    for(int icmap=1; icmap<cmh_n; icmap++) {
	objs++;
	move(row, col);
	int att1 = (icmap==globs.cmapnum)*(A_UNDERLINE|A_BOLD);
	int att2 = (icmap==pending_cmapnum)*(A_REVERSE);
	attron(att|att1|att2);
	printw("%i. ", objs);
	attroff(att);
	printw("%s", cmh_colormaps[icmap].name);
	attroff(att1|att2);
	getyx(wnd, row, x);
	if(x > max_x)
	    max_x = x;
	if(++row >= ylen) {
	    row = 0;
	    if((col=max_x+4) >= xlen) break;
	}
    }
    refresh();
}

static void draw2d(const nct_var* var) {
    printinfo(g_minmax);
    if (too_small_to_draw)
	return;

    /* defined either ind sdl_spesific or wayland_spesific depending on the choice in config.mk */
    set_color(globs.color_bg);
    clear_background();
    set_scale(g_pixels_per_datum, g_pixels_per_datum);

    if (g_only_nans) return;

    void* dataptr = var->data + (plt.area->znum*plt.stepsize_z*(zid>=0) - var->startpos) * g_size1;

    float fdataj = plt.area->offset_j;
    int idataj = round(fdataj), j;
    if (plt.use_threshold) {
	if (globs.invert_y)
	    for(j=draw_h-g_pixels_per_datum; j>=0; j-=g_pixels_per_datum) {
		draw_row_threshold(var->dtype, j,
		    dataptr + g_size1*idataj*g_xlen, plt.threshold);
		idataj = round(fdataj += g_data_per_step);
	    }
	else
	    for(j=0; j<draw_h; j+=g_pixels_per_datum) {
		draw_row_threshold(var->dtype, j,
		    dataptr + g_size1*idataj*g_xlen, plt.threshold);
		idataj = round(fdataj += g_data_per_step);
	    }
    }
    else {
	if (globs.invert_y)
	    for(j=draw_h-g_pixels_per_datum; j>=0; j-=g_pixels_per_datum) {
		draw_row(var->dtype, j,
		    dataptr + g_size1*idataj*g_xlen);
		idataj = round(fdataj += g_data_per_step);
	    }
	else
	    for(j=0; j<draw_h; j+=g_pixels_per_datum) {
		draw_row(var->dtype, j,
		    dataptr + g_size1*idataj*g_xlen);
		idataj = round(fdataj += g_data_per_step);
	    }
    }
    draw_colormap();
}

static void draw_colormap() {
    float cspace = 255.0f/draw_w;
    float di = 0;
    set_scale(1, 1);
    int j0 = draw_h + cmapspace - g_extended_y*g_pixels_per_datum;
    if(!globs.invert_c)
	for(int i=0; i<draw_w; i++, di+=cspace) {
	    unsigned char* c = cmh_colorvalue(globs.cmapnum, (int)di);
	    set_color(c);
	    for(int j=j0; j<draw_h+additional_height(); j++)
		graphics_draw_point(i,j);
	}
    else
	for(int i=draw_w-1; i>=0; i--, di+=cspace) {
	    unsigned char* c = cmh_colorvalue(globs.cmapnum, (int)di);
	    set_color(c);
	    for(int j=j0; j<draw_h+additional_height(); j++)
		graphics_draw_point(i,j);
	}
}

static void clear_infoprint() {
    for(int i=0; i<lines_printed; i++)
	printf("\033[2K\n");		// clear the line
    printf("\r\033[%iA", lines_printed);	// move cursor to start
    lines_printed = 0;
}

#define A echo_highlight
#define B nct_default_color
static void printinfo(void* minmax) {
    if (!(globs.echo && prog_mode > n_cursesmodes))
	return;
    nct_var *zvar = plt.zvar;
    int size1 = nctypelen(var->dtype);
    printf("%s%s%s%s: ", A, var->name, B, plt.globs_detached ? " (detached)" : "");
    printf("min %s", A);   nct_print_datum(var->dtype, minmax);       printf("%s", B);
    printf(", max %s", A); nct_print_datum(var->dtype, minmax+size1); printf("%s", B);
    printf("\033[K\n");
    {
	nct_var* xdim = nct_get_vardim(var, xid);
	printf("x: %s%s(%zu)%s", A, xdim->name, xdim->len, B);
    }
    if (yid >= 0) {
	nct_var* ydim = nct_get_vardim(var, yid);
	printf(", y: %s%s(%zu)%s", A, ydim->name, ydim->len, B);
    }
    if (zvar) {
	printf(", z: %s%s(%i/%zu ", A, zvar->name, plt.area->znum+1, zvar->len);
	if (plt.time0.d >= 0) {
	    char help[128];
	    strftime(help, 128, "%F %T", nct_gmtime((long)nct_get_integer(zvar, plt.area->znum), plt.time0));
	    printf(" %s", help);
	}
	else if (nct_iscoord(zvar))
	    nct_print_datum(zvar->dtype, zvar->data+plt.area->znum*nctypelen(zvar->dtype));
	printf(")%s", B);
    }
    printf("\033[K\n"
	    "minshift %s%.4f%s, maxshift %s%.4f%s\033[K\n"
	    "data/pixel = %s%.4f%s\033[K\n"
	    "colormap = %s%s%s%s\033[K\n",
	    A,plt.minshift,B, A,plt.maxshift,B,
	    A,data_per_pixel,B, A,cmh_colormaps[globs.cmapnum].name,B, globs.invert_c? " reversed": "");
    putchar('\n'); // room for mouseinfo
    lines_printed = 6;
    printf("\r\033[%iA", lines_printed); // move cursor to start
    mousemotion(0, 0); // print mouseinfo
}
#undef A
#undef B

static int varpos_xy_i, varpos_xy_j;

static long get_varpos_xy(int x, int y) {
    long xlen = nct_get_vardim(var, xid)->len;
    long ylen = yid < 0 ? 0 : nct_get_vardim(var, yid)->len;

    if (globs.invert_y)
	y = draw_h / g_pixels_per_datum * g_pixels_per_datum - y;
    int i = x / g_pixels_per_datum;
    int j = y / g_pixels_per_datum;
    float idata_f = plt.area->offset_i + i*g_data_per_step;
    float jdata_f = plt.area->offset_j + j*g_data_per_step;
    int idata = round(idata_f);
    int jdata = round(jdata_f);

    varpos_xy_i = idata;
    varpos_xy_j = jdata;

    if (idata>=xlen || (jdata>=ylen && yid >= 0))
	return -1;
    return (zid>=0)*xlen*ylen*plt.area->znum + (yid>=0)*xlen*jdata + idata;
}

static void _maybe_print_mousecoordinate(int vardimid, int at) {
    nct_var* coord = nct_get_vardim(var, vardimid);
    if (coord->data) {
	void* val = coord->data + at*nctypelen(coord->dtype);
	nct_print_datum(coord->dtype, val);
    }
    else
	printf("Ø");
}

static void mousemotion(int xrel, int yrel) {
    if (prog_mode < n_cursesmodes || !globs.echo)// || !count++)
	return;
    long pos = get_varpos_xy(mousex,mousey);
    if (pos < 0)
	return;
    if (lines_printed)
	printf("\033[%iB\r", lines_printed-1); // This is the last line in info
    nct_print_datum(var->dtype, var->data + (pos - var->startpos) * nctypelen(var->dtype));
    int xlen = nct_get_vardim(var, xid)->len;
    printf(" [%zu pos=(%i,%i: %i) coords=(", pos, varpos_xy_j, varpos_xy_i, varpos_xy_j*xlen + varpos_xy_i);
    if (yid >= 0) {
	_maybe_print_mousecoordinate(yid, varpos_xy_j);
	putchar(',');
    }
    _maybe_print_mousecoordinate(xid, varpos_xy_i);
    printf(")]\033[K\n");
    printf("\033[%iA\r", lines_printed);
}

static void mousewheel(int num) {
    if (!num)
	return;
    float x = mousex;
    float y = mousey;
    float multiple = 0.95;
    if (num < 0) {
	multiple = 1 / multiple;
	num = -num;
    }
    while(num) {
	multiple *= multiple;
	num--;
    }
    multiply_zoom_fixed_point(multiple, x/draw_w, y/draw_h);
}

static void mousemove(float xrel, float yrel) {
    static float move_datax, move_datay;
    move_datax += xrel * data_per_pixel;
    move_datay += yrel * data_per_pixel;
    int xmove, ymove;
    move_datax -= (xmove = iround(move_datax));
    move_datay -= (ymove = iround(move_datay));
    inc_offset_i((Arg){.i=-xmove});
    inc_offset_j((Arg){.i=-ymove});
    set_draw_params();
    call_redraw = 1;
}

struct {
    long sum_of_variables, used_memory;
} memory;

static void manage_memory() {
    long startpos = plt.area->znum * plt.stepsize_z;
    if (var->startpos <= startpos && var->endpos >= startpos+plt.stepsize_z)
	return;
    if (!nct_loadable(var))
	return;

    if (memory.sum_of_variables == 0)
	nct_foreach(var->super, v)
	    memory.sum_of_variables += v->len * nctypelen(v->dtype);

    long thisbytes = var->len * nctypelen(var->dtype);
    double memory_fraction = (double)thisbytes / memory.sum_of_variables;
    long allowed_bytes = globs.cache_size * memory_fraction;

    if (thisbytes <= allowed_bytes) {
	nct_load(var);
	memory.used_memory += thisbytes;
	return;
    }

    long thislen = allowed_bytes / nctypelen(var->dtype) / plt.stepsize_z * plt.stepsize_z;
    if (thislen == 0)
	thislen = plt.stepsize_z;
    long endpos = startpos + thislen;
    int frames_behind = thislen / plt.stepsize_z / 5;
    startpos -= frames_behind * plt.stepsize_z;
    endpos -= frames_behind * plt.stepsize_z;
    if (startpos < 0) {
	endpos += -startpos;
	startpos = 0;
    }
    if (endpos > var->len)
	endpos = var->len;
    nct_load_partially(var, startpos, endpos);
    memory.used_memory += (endpos - startpos) * nctypelen(var->dtype);
}

static void update_minmax_fun() {
    long start = 0;
    long end = var->endpos - var->startpos;

    if (update_minmax_cur) {
	start = (zid>=0) * plt.stepsize_z * plt.area->znum;
	end = start + plt.stepsize_z;
	update_minmax_cur = 0;
    }

    update_minmax = 0;
    if (globs.usenan)
	nct_minmax_nan_at(var, globs.nanval, start, end, plt.minmax);
    else
	nct_minmax_at(var, start, end, plt.minmax);
}

static void redraw(nct_var* var) {
    static uint_fast64_t lasttime;
    uint_fast64_t thistime = time_now_ms();
    if(thistime-lasttime < 10) {
	call_redraw = 1;
	return;
    }
    call_redraw = 0;
    lasttime = thistime;

    manage_memory();

    if (update_minmax | update_minmax_cur) {
	update_minmax_fun();
	g_only_nans = make_minmax(var->dtype);
    }

#ifndef HAVE_WAYLAND
    SDL_SetRenderTarget(rend, base);
#endif
    draw_funcptr(var);
    if (globs.coastlines) {
	if (!plt.area->coasts)
	    init_coastlines(plt.area, NULL);
	draw_coastlines(plt.area);
    }
#ifndef HAVE_WAYLAND
    SDL_SetRenderTarget(rend, NULL);
#endif
}

static void set_dimids() {
    xid = var->ndims-1;
    yid = var->ndims-2;
    zid = var->ndims-3;
    draw_funcptr = yid<0? draw1d: draw2d;
    if (zid < 0)
	zid = -1;
    if (zid >= 0) {
	plt.zvar = var->super->dims[var->dimids[zid]];
	plt.time0 = nct_timegm0_nofail(plt.zvar, NULL);
	if(!plt.zvar->data && nct_iscoord(plt.zvar))
	    nct_load(plt.zvar);
    }
    else {
	plt.zvar = NULL;
	plt.time0.d = -1;
    }
}

#define GET_SPACE_FILL(xlen,win_w,ylen,win_h)    MIN((float)(ylen)/(win_h), (float)(xlen)/(win_w))
#define GET_SPACE_NONFILL(xlen,win_w,ylen,win_h) MAX((float)(ylen)/(win_h), (float)(xlen)/(win_w))
#define GET_SPACE(a,b,c,d) (fill_on? GET_SPACE_FILL(a,b,c,d): GET_SPACE_NONFILL(a,b,c,d))

static void set_draw_params() {
    int offset_j = plt.area->offset_j + plt.area->j_off_by_one;
    plt.area->j_off_by_one = 0;
    int offset_i = plt.area->offset_i;

    g_only_nans = make_minmax(var->dtype);

    g_size1 = nctypelen(var->dtype);
    g_xlen = nct_get_vardim(var, xid)->len;
    if(yid>=0) {
	g_ylen  = nct_get_vardim(var, yid)->len;
	data_per_pixel = GET_SPACE(g_xlen, win_w, g_ylen, win_h - additional_height());
    } else {
	data_per_pixel = (float)(g_xlen)/(win_w);
	g_ylen  = win_h * data_per_pixel;
    }
    data_per_pixel *= zoom;
    if (globs.exact)
	data_per_pixel = data_per_pixel >= 1 ? ceil(data_per_pixel) :
	    1.0 / floor(1.0/data_per_pixel);
    if (offset_i < 0) offset_i = plt.area->offset_i = 0;
    if (offset_j < 0) offset_j = plt.area->offset_j = 0;

    draw_w = round((g_xlen-offset_i) / data_per_pixel); // how many pixels data can reach
    draw_h = round((g_ylen-offset_j) / data_per_pixel);
    draw_w = MIN(win_w, draw_w);
    draw_h = MIN(win_h-additional_height(), draw_h);
    too_small_to_draw = draw_h < 0;
    if (zid < 0) zid = -1;
    plt.stepsize_z = nct_get_len_from(var, zid+1); // works even if zid == -1
    plt.stepsize_z += plt.stepsize_z == 0; // length must be at least 1

    g_pixels_per_datum = globs.exact ? round(1.0 / data_per_pixel) : 1.0 / data_per_pixel;
    g_pixels_per_datum += !g_pixels_per_datum;
    g_data_per_step = g_pixels_per_datum * data_per_pixel; // step is a virtual pixel >= physical pixel

    draw_w = draw_w / g_pixels_per_datum * g_pixels_per_datum;
    draw_h = draw_h / g_pixels_per_datum * g_pixels_per_datum;

#ifndef HAVE_WAYLAND
    int if_add_1;
    if_add_1 = draw_w / g_pixels_per_datum < g_xlen - offset_i && draw_w < win_w;
    draw_w += if_add_1 * g_pixels_per_datum; // may be larger than win_w which is not a problem in SDL

    if (globs.invert_y) {
	if_add_1 = draw_h / g_pixels_per_datum < g_ylen - offset_j && draw_h < win_h && offset_j;
	offset_j -= if_add_1;
	plt.area->offset_j = offset_j;
	plt.area->j_off_by_one = if_add_1;
    }
    else
	if_add_1 = draw_h / g_pixels_per_datum < g_ylen - offset_j && draw_h < win_h;
    draw_h += if_add_1 * g_pixels_per_datum; // may be larger than win_h which is not a problem in SDL
    g_extended_y = if_add_1;
#endif
}

static uint_fast64_t time_now_ms() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000 + t.tv_usec/1000;
}

static void unlink_area(struct shown_area *area) {
    if (!area || area->nusers--)
	return;
    free(area->coasts);
    free(area->points);
    free(area->breaks);
    free(area->crs);
    free(area);
}

static struct shown_area* get_ref_shown_area() {
    nct_var* xdim = nct_get_vardim(plt.var, xid);
    nct_var* ydim = yid >= 0 ? nct_get_vardim(plt.var, yid) : NULL;
    nct_var* zdim = zid >= 0 ? nct_get_vardim(plt.var, zid) : NULL;
    if (plt.var->ndims < 2)
	goto make_new;
    for (int ip=0; ip<n_plottables; ip++) {
	nct_var* var1 = plottables[ip].var;
	struct shown_area *a = plottables[ip].area;
	if (!var1 || !a || var1->ndims < 2)
	    continue;
	if (a->xdim == xdim && a->ydim == ydim && a->zdim == zdim) {
	    a->nusers++;
	    return a;
	}
    }
make_new:
    struct shown_area* area = calloc(1, sizeof(struct shown_area));
    area->xdim = xdim;
    area->ydim = ydim;
    area->zdim = zdim;
    return area;
}

static void variable_changed() {
    if (var->ndims < 1)
	return var_ichange((Arg){.i=_variable_changed_direction}); // 0-dimensional variables are not supported.

    long wants = MAX(n_plottables, pltind);
    recalloc_list(&globslist, &globslistlen, wants, sizeof(struct nctplot_globals), &default_globals);

    /* Is previous was detached, take back the global state and put its state to memory. */
    if (plt.globs_detached) {
	globslist[pltind] = globs;
	globs = globs_mem;
    }

    pltind = nct_varid(var); // this is the core change
    recalloc_list(&plottables, &n_plottables, pltind+1, sizeof(plottable), NULL);

    if (plt.globs_detached) {
	globs_mem = globs;
	globs = globslist[pltind];
    }

    /* Order matters here. */
    if (!plt.var) // using this variable for the first time
	update_minmax = 1;
    plt.var = var;
    set_dimids();
    if (!plt.area)
	plt.area = get_ref_shown_area(pltind);
    set_draw_params(); // sets stepsize_z needed in manage_memory
    manage_memory();

    nct_att* att;
    if (var->dtype != NC_FLOAT && var->dtype != NC_DOUBLE &&
	    ((att = nct_get_varatt(var, "_FillValue")) || (att = nct_get_varatt(var, "FillValue")))) {
	globs.usenan = 1;
	globs.nanval = nct_getatt_integer(att, 0);
    }
    call_redraw = 1;
}

/* We could export to those struct shown_area :s which share the same dimensions. */
static void export_projection() {
    return;
#if 0
    int* ids0 = plt.var->dimids;
    int ndims0 = plt.var->ndims;
    if (ndims0 < 2)
	return;
    /* Variables don't have to be yet added to plottables. */
    nct_foreach(plt.var->super, var1) {
	int ndims1 = var1->ndims;
	int iplt = nct_varid(var1);
	if (ndims1 < 2 || iplt == pltind)
	    continue;
	int* ids1 = var1->dimids;
	if (ids0[ndims0-1] == ids1[ndims1-1] && ids0[ndims0-2] == ids1[ndims1-2]) {
	    free(plottables[iplt].crs);
	    plottables[iplt].crs = strdup(plt.crs);
	}
    }
#endif
}

static void ask_crs(Arg _) {
    char crs[256];
    call_redraw = 1;
    if (prompt_string(crs, 256, "coordinate system: "))
	return;
    free(plt.area->crs);
    plt.area->crs = strdup(crs);
    export_projection();
    free(plt.area->coasts);
    plt.area->coasts = NULL;
}

static void cmap_ichange(Arg jump) {
    int len = cmh_n - 1;
    globs.cmapnum = (globs.cmapnum-1+len+jump.i) % len + 1;
    call_redraw = 1;
}

static void toggle_detached(Arg _) {
    plt.globs_detached = !plt.globs_detached;
    globs_mem = globs;
    call_redraw = 1;
}

static void toggle_play(Arg _) {
    play_on = !play_on;
    if (fps < 0) fps = -fps;
}

static void toggle_play_rev(Arg _) {
    toggle_play(_);
    fps = -fps;
}

static void invert_colors(Arg _) {
    typeof(globs.color_bg) mem;
    memcpy(mem,			globs.color_fg,	sizeof(globs.color_fg));
    memcpy(globs.color_fg,	globs.color_bg,	sizeof(globs.color_fg));
    memcpy(globs.color_bg,	mem,		sizeof(globs.color_fg));
    call_redraw = 1;
}

static void end_curses(Arg _) {
    endwin();
    prog_mode = no_m;
}

static void show_bindings(Arg _) {
    int a = fork();
    if (!a) {
	int b = fork();
	if (!b) {
	    execv(pager_path, pager_args);
	    nct_puterror("Pager \"%s\" could not be executed.\n", pager_path);
	    exit(1);
	}
	_exit(0);
    }
    else
	waitpid(a, NULL, 0);
}

static void inc_offset_i(Arg arg) {
    if (draw_w <= win_w - g_pixels_per_datum && arg.i > 0)
	return;
    plt.area->offset_i += arg.i;
    set_draw_params();
    int too_much = floor((win_w - draw_w) * data_per_pixel + 1e-10);
    if (too_much > 0) {
	plt.area->offset_i -= too_much;
	set_draw_params();
    }
    call_redraw = 1;
}

static void inc_offset_j(Arg arg) {
    if (globs.invert_y)
	arg.i = -arg.i;
    int winh = win_h - additional_height();
    if (draw_h <= winh - g_pixels_per_datum && arg.i > 0)
	return;
    plt.area->offset_j += arg.i;
    set_draw_params();
    int too_much = floor((winh - draw_h) * data_per_pixel + 1e-10);
    if (too_much > 0) {
	plt.area->offset_j -= too_much;
	set_draw_params();
    }
    call_redraw = 1;
}

static void inc_znum(Arg intarg) {
    if (!plt.zvar)
	return;
    size_t zlen = plt.zvar->len;
    /* below: znum + intarg.i, but goes around when zlen or a negative number is reached. */
    plt.area->znum = (plt.area->znum + zlen + intarg.i) % zlen;
    call_redraw = 1;
}

static void multiply_zoom_fixed_point(float multiple, float xfraction, float yfraction) {
    yfraction = (float[]){yfraction, 1-yfraction}[!!globs.invert_y];
    float fixed_datax = draw_w*data_per_pixel * xfraction + plt.area->offset_i;
    float fixed_datay = draw_h*data_per_pixel * yfraction + plt.area->offset_j;
    zoom *= multiple;
    set_draw_params();
    plt.area->offset_i = iround(fixed_datax - draw_w*data_per_pixel * xfraction);
    plt.area->offset_j = iround(fixed_datay - draw_h*data_per_pixel * yfraction);
    set_draw_params();
    call_redraw = 1;
}

static void multiply_zoom(Arg arg) {
    multiply_zoom_fixed_point(arg.f, 0.5, 0.5);
}

static void jump_to(Arg _) {
    printf("Enter a framenumber to jump to \033[K");
    fflush(stdout);
    int arg0, month=0, day=1, hour=0, minute=0;
    float second=0;
    switch (scanf("%d-%d-%d[ *]%d:%d:%f", &arg0, &month, &day, &hour, &minute, &second)) {
	case 1: plt.area->znum = arg0-1; break; // user entered a frame number
	case 0: break;
	case -1: warn("scanf in %s", __func__); break;
	default:
	    if (plt.time0.d < 0)
		return; // date entered but z-coordinate is not date
	    struct tm tm = {
		.tm_year = arg0-1900,
		.tm_mon = month-1,
		.tm_mday = day,
		.tm_hour = hour,
		.tm_min = minute,
		.tm_sec = second,
	    };
	    time_t target_time = timegm(&tm);
	    long long coordval = nct_convert_time_anyd(target_time, plt.time0);
	    plt.area->znum = nct_find_sorted(plt.zvar, coordval);
	    break;
    }
    if (plt.area->znum < 0)
	plt.area->znum = 0;
    else if (plt.area->znum >= plt.zvar->len)
	plt.area->znum = plt.zvar->len-1;
    lines_printed--;
    fflush(stdout);
    call_redraw = 1;
}

static void pending_var_dec(Arg _) {
    if (pending_varnum<0) pending_varnum = nct_varid(var);
    nct_var* var1 = nct_prevvar(var->super->vars[pending_varnum]);
    pending_varnum = var1 ? nct_varid(var1) : nct_varid(nct_lastvar(var->super));
    if (prog_mode == variables_m)
	curses_write_vars();
}

static void pending_var_inc(Arg _) {
    if (pending_varnum<0) pending_varnum = nct_varid(var);
    nct_var* var1 = nct_nextvar(var->super->vars[pending_varnum]);
    pending_varnum = var1 ? nct_varid(var1) : nct_varid(nct_firstvar(var->super));
    if (prog_mode == variables_m)
	curses_write_vars();
}

static void pending_map_dec(Arg _) {
    if (!pending_cmapnum) pending_cmapnum = globs.cmapnum;
    if (--pending_cmapnum <= 0)
	pending_cmapnum += cmh_n-1;
    if (prog_mode == colormaps_m)
	curses_write_cmaps();
}

static void pending_map_inc(Arg _) {
    if (!pending_cmapnum) pending_cmapnum = globs.cmapnum;
    if (++pending_cmapnum >= cmh_n)
	pending_cmapnum -= cmh_n-1;
    if (prog_mode == colormaps_m)
	curses_write_cmaps();
}

static void print_var(Arg _) {
    clear_infoprint();
    nct_print(var->super);
}

static int prompt_floating(double *out, const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\033[K");
    fflush(stdout);
    if (scanf("%lf", out) != 1)
	return 1;
    lines_printed--;
    return 0;
}

static int prompt_integer(long long *out, const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\033[K");
    fflush(stdout);
    if (scanf("%lli", out) != 1)
	return 1;
    lines_printed--;
    return 0;
}

static int prompt_string(char *out, int outlen, const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\033[K\033[s");
    fflush(stdout);
    lines_printed--;

    /* Get characters when pressed instead of waiting for newline. */
    struct termios oldterminal, terminal;
    tcgetattr(STDIN_FILENO, &oldterminal);
    terminal = oldterminal;
    terminal.c_lflag &= ~(ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal);

    int i = 0, ret = 0;
    while (!i)
	for (i=0; i<outlen-1; i++)
	    if ((out[i] = getchar()) == '\n')
		break;
	    else if (out[i] == 127) { // backspace
		while (i && (out[--i] >> 6 & 3) == 2);
		out[i--] = 0;
		printf("\033[u%s\033[K", out), fflush(stdout);
	    }
	    else if (out[i] == EOF || out[i] == '\033') {
		printf("\033[2;3maborted\033[0m\n");
		ret = 1;
		goto done; }

done:
    out[i] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &oldterminal); // restore termial behaviour
    return ret;
}

static void shift_max(Arg shift) {
    plt.maxshift += shift.f;
    g_only_nans = make_minmax(var->dtype);
    call_redraw = 1;
}

static void shift_max_abs(Arg shift) {
    maxshift_abs += shift.f;
    g_only_nans = make_minmax(var->dtype);
    call_redraw = 1;
}

static void shift_min(Arg shift) {
    plt.minshift += shift.f;
    g_only_nans = make_minmax(var->dtype);
    call_redraw = 1;
}

static void shift_min_abs(Arg shift) {
    minshift_abs += shift.f;
    g_only_nans = make_minmax(var->dtype);
    call_redraw = 1;
}

static void toggle_var(Arg intptr) {
    *(char*)intptr.v = !*(char*)intptr.v;
    set_draw_params();
    call_redraw = 1;
}

static void set_nan(Arg _) {
    long long result;
    if (prompt_integer(&result, "enter NAN: "))
	return;
    globs.nanval = result;
    globs.usenan = update_minmax = call_redraw = 1;
}

static void use_pending(Arg _) {
    prev_pltind = pltind;
    if (pending_varnum >= 0) {
	var = var->super->vars[pending_varnum];
	pending_varnum = -1;
    }
    _variable_changed_direction = 1;
    variable_changed();
}

static void use_pending_cmap(Arg _) {
    if (pending_cmapnum <= 0)
	return;
    int help = globs.cmapnum;
    globs.cmapnum = pending_cmapnum;
    pending_cmapnum = help;
    call_redraw = 1;
}


static void use_and_exit(Arg _) {
    end_curses(_);
    use_pending(_);
}

static void use_map_and_exit(Arg _) {
    end_curses(_);
    use_pending_cmap(_);
}

static void var_ichange(Arg jump) {
    nct_var* v;
    if(jump.i > 0) {
	_variable_changed_direction = 1;
	if(!(v = nct_nextvar(var)))
	    v = nct_firstvar(var->super);
    } else {
	_variable_changed_direction = -1;
	if(!(v = nct_prevvar(var)))
	    v = nct_lastvar(var->super);
    }
    if(!v) {
	nct_puterror("This is impossible. A variable was not found.\n");
	return;
    }
    var = v;
    variable_changed();
}

static void set_prog_mode(Arg mode) {
    prog_mode = mode.i;
    if(prog_mode > n_cursesmodes) return;
    wnd = initscr();
    start_color();
    cbreak();
    noecho();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    if (prog_mode == variables_m)
	curses_write_vars();
    else if (prog_mode == colormaps_m)
	curses_write_cmaps();
}

static void ask_fps(Arg _) {
    double result;
    if(prompt_floating(&result, "Enter fps (default = %f): ", default_fps))
	return;
    fps = result;
}

static void ask_sleep(Arg _) {
    long long result;
    if (prompt_integer(&result, "Enter sleeptime in ms (default = %i): ", default_sleep_ms))
	return;
    sleeptime = result;
}

static void toggle_threshold(Arg _) {
    if ((plt.use_threshold = !plt.use_threshold)) {
	double result;
	if (prompt_floating(&result, "Enter threshold value: ")) {
	    plt.use_threshold = !plt.use_threshold;
	    return;
	}
	else
	    plt.threshold = result;
    }
    call_redraw = 1;
}

static void use_lastvar(Arg _) {
    if (prev_pltind < 0) return;
    int tmp = pltind;
    var = var->super->vars[prev_pltind];
    prev_pltind = tmp;
    variable_changed();
}

static void free_plottable(plottable* plott) {
    unlink_area(plott->area);
}

static void quit(Arg _) {
    stop = 1;
    if (prog_mode < n_cursesmodes)
	end_curses((Arg){0});
    if (lines_printed > 0)
	printf("\r\033[%iB", lines_printed), fflush(stdout);	// move cursor past the echo region
    lines_printed = 0;
    if (mp_params.dlhandle)
	dlclose(mp_params.dlhandle);
    free_coastlines();
    for(int i=0; i<n_plottables; i++)
	free_plottable(plottables+i);
    plottables = (free(plottables), NULL);
    n_plottables = 0;
    free(globslist); globslist = NULL; globslistlen = 0;
    mp_params = (struct Mp_params){0};
    memset(&memory, 0, sizeof(memory));
    quit_graphics();
}

#ifdef HAVE_NCTPROJ
static void convert_coord(Arg _) {
    char from[256], to[256];
    printf("from: ");
    if (plt.area->crs)
	printf("%s\033[K\n", plt.area->crs);
    else if (prompt_string(from, sizeof(from), ""))
	return;
    else
	plt.area->crs = strdup(from);
    export_projection();
    if (prompt_string(to, sizeof(to), "to: "))
	return;
    var = nctproj_open_converted_var(var, plt.area->crs, to, NULL);
    variable_changed();
    plt.area->crs = strdup(to); // not before variable_changed()
}
#endif

static int mp_set_fvalue(char str[256]);
static int mp_set_lfvalue(char str[256]);
static int mp_set_ivalue(char str[256]);

#define TMP mp_params.size
static void mp_replace_val(void* new_val) {
    for(int j=-TMP; j<=TMP; j++)
	for(int i=-TMP; i<=TMP; i++) {
	    int x = mousex + i;
	    int y = mousey + j;
	    if(x < 0 || y < 0) continue;
	    long pos = get_varpos_xy(x,y)*nctypelen(var->dtype);
	    if(pos < 0) continue;
	    memcpy(var->data+pos, new_val, nctypelen(var->dtype));
	}
}

static void mp_replace_fun() {
    for(int j=-TMP; j<=TMP; j++)
	for(int i=-TMP; i<=TMP; i++) {
	    int x = mousex + i;
	    int y = mousey + j;
	    if(x < 0 || y < 0) continue;
	    long pos = get_varpos_xy(x,y)*nctypelen(var->dtype);
	    if(pos < 0) return;
	    memcpy(var->data+pos, mp_params.fun(var->data+pos, mp_params.buff), nctypelen(var->dtype));
	}
}
#undef TMP

static void mp_save(Arg) {
    int zero = 0;
    if(!mp_params.filename[0]) {
	zero = 1;
	long unsigned timevar = time(NULL);
	sprintf(mp_params.filename, "%lu.nc", timevar);
    }
    nct_write_nc(var->super, mp_params.filename);
    if(zero)
	mp_params.filename[0] = '\0';
}

static void mp_save_frame(Arg) {
    int zero = 0;
    if (!mp_params.filename[0]) {
	zero = 1;
	long unsigned timevar = time(NULL);
	sprintf(mp_params.filename, "%lu.nc", timevar);
    }
    nct_set out = {0};
    int len = 1;
    int id = 0;
    if (yid >= 0) {
	nct_copy_var(&out, nct_get_vardim(var, yid), 1);
	len *= out.dims[id++]->len;
    }
    nct_copy_var(&out, nct_get_vardim(var, xid), 1);
    len *= out.dims[id++]->len;

    void* data = var->data;
    data += ((zid >= 0) * plt.area->znum * plt.stepsize_z - var->startpos) * nctypelen(var->dtype);
    nct_ensure_unique_name(nct_add_var_alldims(&out, data, var->dtype, "data"))
	-> not_freeable = 1;

    nct_write_nc(&out, mp_params.filename);
    nct_free1(&out);
    if (zero)
	mp_params.filename[0] = '\0';
}

static void mp_set_action(Arg arg) {
    char str[256];
    if(var->dtype == NC_FLOAT) {
	if(!mp_set_fvalue(str)) return; }
    else if (var->dtype == NC_DOUBLE) {
	if(!mp_set_lfvalue(str)) return; }
    else {
	if(!mp_set_ivalue(str)) return; }

    if(mp_params.dlhandle) {
	dlclose(mp_params.dlhandle);
	mp_params.fun = mp_params.dlhandle = NULL;
    }
    mp_params.dlhandle = dlopen(str, RTLD_LAZY);
    if(!mp_params.dlhandle) {
	printf("dlopen %s: %s\033[K\n", str, dlerror());
	lines_printed--;
	return; }
    if(!(mp_params.fun = dlsym(mp_params.dlhandle, "function"))) {
	printf("dlsym(\"function\") failed %s\033[K\n", dlerror());
	lines_printed--;
	return; }
    mp_params.mode = function_mp;
}

static void mp_set_filename(Arg arg) {
    prompt_string(mp_params.filename, 256, "set filename: ");
}

static int mp_set_fvalue(char str[256]) {
    printf("set floating point value or *.so with void* function(void* in, void* out): \033[K");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    lines_printed--;
    if(sscanf(str, "%f", &mp_params.value.f) == 1) {
	mp_params.mode = fixed_mp;
	return 0; }
    return 1;
}

static int mp_set_lfvalue(char str[256]) {
    printf("set floating point value or *.so with void* function(void* in, void* out): \033[K");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    lines_printed--;
    if(sscanf(str, "%lf", &mp_params.value.lf) == 1) {
	mp_params.mode = fixed_mp;
	return 0; }
    return 1;
}

static int mp_set_ivalue(char str[256]) {
    printf("set integer value or *.so with void* function(void* in, void* out): \033[K");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    lines_printed--;
    if(sscanf(str, "%lli", &mp_params.value.lli) == 1) {
	mp_params.mode = fixed_mp;
	return 0; }
    return 1;
}

static void mp_size(Arg arg) {
    mp_params.size += arg.i;
    if(mp_params.size < 0)
	mp_params.size = 0;
}

static void mousepaint() {
    switch(mp_params.mode) {
	case fixed_mp:
	    mp_replace_val((void*)&mp_params.value); return;
	case function_mp:
	    mp_replace_fun(); return;
    }
}

#define handle_keybindings(sym, mod, a) _handle_keybindings(sym, mod, a, ARRSIZE(a))
static int _handle_keybindings(int keysym, unsigned modstate, Binding b[], int len) {
    int ret = 0, found = 0;
    for(int i=0; i<len; i++)
	if (keysym == b[i].key && modstate == b[i].mod) {
	    found = 1;
	    b[i].fun(b[i].arg);
	    ret++; // There can be multiple bindings for the same key.
	}
	else if (found)
	    return ret; // Multiple bindings must be consecutive.
    return ret;
}

#include bindings_file

static void keydown_func(int keysym, unsigned mod) {
    if(0);
    else if (prog_mode == variables_m  && handle_keybindings(keysym, mod, keydown_bindings_variables_m)) return;
    else if (prog_mode == mousepaint_m && handle_keybindings(keysym, mod, keydown_bindings_mousepaint_m)) return;
    else if (prog_mode == colormaps_m  && handle_keybindings(keysym, mod, keydown_bindings_colormaps_m)) return;
    handle_keybindings(keysym, mod, keydown_bindings);
}

/* Only following functions should be called from programs. */

void nctplot_(void* vobject, int isset) {
    if (isset) {
	nct_foreach(vobject, varnow)
	    if (varnow->ndims > 1) {
		var = varnow;
		goto variable_found;
	    }
	nct_foreach(vobject, varnow)
	    if (varnow->ndims >= 1) {
		var = varnow;
		goto variable_found;
	    }
	nct_puterror("Only 0-dimensional variables\n");
	return;
    }
    else if (!(var = vobject)) {
	nct_puterror("No variable to plot\n");
	return;
    }

variable_found:
    globs = default_globals; // must be early because globals may be modified or needed by functions
    n_plottables = var->super->nvars;
    plottables = calloc(n_plottables, sizeof(plottable));
    pltind = nct_varid(var);
    plottables[pltind].var = var;
    set_dimids();
    int xlen = var->super->dims[var->dimids[xid]]->len, ylen;
    if (yid>=0)
	ylen = var->super->dims[var->dimids[yid]]->len;
    else
	ylen = 400;

    init_graphics(xlen, ylen); // defined either in sdl_spesific.c or in wayland_spesific.c depending on the choice in config.mk
    variable_changed();

    sleeptime = default_sleep_ms;
    fps = default_fps;
    stop = lines_printed = play_on = 0;
    update_minmax = 1;
    prev_pltind = pending_varnum = -1;
    mp_params = (struct Mp_params){0};

    mainloop();
}

struct nctplot_globals* nctplot_get_globals() {
    return &globs;
}
