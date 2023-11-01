#include <nctietue3.h>
#include <cmh_colormaps.h>
#include <SDL2/SDL.h>
#include <stdint.h>
#include <curses.h>
#include <sys/time.h>
#include <err.h>
#include <dlfcn.h> // dlopen, dlerror, etc. for mousepaint
#include <unistd.h> // fork
#include <sys/wait.h> // waitpid
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
    char globs_detached;
    struct shown_area *area;
} plottable;

/* Static variables. Global in the context of this library. */
static plottable* plottables;
static int pltind, prev_pltind, n_plottables;
#define plt (plottables[pltind])
static nct_var* var; // = plt.var
static SDL_Renderer* rend;
static SDL_Window* window;
static SDL_Texture* base;
static WINDOW *wnd;
static const Uint32 default_sleep=8; // ms
static Uint32 sleeptime;
static int mousex, mousey;
static int win_w, win_h, xid, yid, zid, draw_w, draw_h, pending_varnum=-1, pending_cmapnum;
static char stop, fill_on, play_on, play_inv, update_minmax=1, update_minmax_cur;
static int lines_echoed;
static int cmappix=30, cmapspace=10, call_resized, call_redraw;
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
static void my_echo(void* minmax);
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
static void quit(Arg _);
static void var_ichange(Arg jump);

union Arg {
    void* v;
    float f;
    int   i;
};

#define size_coastl_params (sizeof(int)*2 + sizeof(data_per_pixel) + sizeof(globs.invert_y))

struct shown_area {
    int offset_i, offset_j, znum, nusers, j_off_by_one; // nusers = 0, when 1 user
    nct_var *xdim, *ydim, *zdim;
    /* for coastlines */
    double* coasts;	// coordinates of coastlines
    void* points;	// pixelcoordinates of coastlines
    int *breaks, nbreaks; // indices where to lift pen from the paper
    char* crs;
    double x0, y0, xspace, yspace;
    char coastl_params[size_coastl_params]; // to tell if coastlines need to be redrawn
};

#define ARRSIZE(a) (sizeof(a) / sizeof(*(a)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include "functions.c" // draw1d, draw_row, make_minmax; automatically generated from functions.in.c
#include "coastlines.c"
#include "png.c"

static SDL_Event event;

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

static int recalloc_list(void* varr, int *has, int wants, int size1) {
    void** arr = varr;
    if (*has >= wants)
	return 0;
    void *tmp = realloc(*arr, wants*size1);
    if (!tmp) {
	warn("realloc %i in %s: %i", wants*size1, __FILE__, __LINE__);
	return -1;
    }
    memset(*arr+*has*size1, 0, (wants-*has)*size1);
    *arr = tmp;
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
    my_echo(g_minmax);

    SDL_SetRenderDrawColor(rend, globs.color_bg[0], globs.color_bg[1], globs.color_bg[2], 255);
    SDL_RenderClear(rend);
    SDL_RenderSetScale(rend, g_pixels_per_datum, g_pixels_per_datum);
    if (g_only_nans) return;

    void* dataptr = var->data + (plt.area->znum*plt.stepsize_z*(zid>=0) - var->startpos) * g_size1;

    float fdataj = plt.area->offset_j;
    int idataj = round(fdataj), j;
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
    draw_colormap();
}

static void draw_colormap() {
    float cspace = 255.0f/win_w;
    float di = 0;
    SDL_RenderSetScale(rend, 1, 1);
    int j0 = draw_h + cmapspace - g_extended_y*g_pixels_per_datum;
    if(!globs.invert_c)
	for(int i=0; i<win_w; i++, di+=cspace) {
	    unsigned char* c = cmh_colorvalue(globs.cmapnum, (int)di);
	    SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 255);
	    for(int j=j0; j<draw_h+cmapspace+cmappix; j++)
		SDL_RenderDrawPoint(rend, i, j);
	}
    else
	for(int i=win_w-1; i>=0; i--, di+=cspace) {
	    unsigned char* c = cmh_colorvalue(globs.cmapnum, (int)di);
	    SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 255);
	    for(int j=j0; j<draw_h+cmapspace+cmappix; j++)
		SDL_RenderDrawPoint(rend, i, j);
	}	
}

static void clear_echo() {
    for(int i=0; i<lines_echoed; i++)
	printf("\033[2K\n");		// clear the line
    printf("\r\033[%iA", lines_echoed);	// move cursor to start
    lines_echoed = 0;
}

#define A echo_highlight
#define B nct_default_color
static void my_echo(void* minmax) {
    if (!(globs.echo && prog_mode > n_cursesmodes))
	return;
    nct_var *zvar = plt.zvar;
    int size1 = nctypelen(var->dtype);
    lines_echoed = echo_h;
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
	    strftime(help, 128, "%F %T", nct_localtime((long)nct_get_integer(zvar, plt.area->znum), plt.time0));
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
    printf("\r\033[%iA", echo_h); // move cursor to start
}
#undef A
#undef B

static int varpos_xy_i, varpos_xy_j;

static long get_varpos_xy(int x, int y) {
    int xlen = nct_get_vardim(var, xid)->len;
    int ylen = yid < 0 ? 0 : nct_get_vardim(var, yid)->len;

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

static void mousemotion() {
    static int count;
    if(prog_mode < n_cursesmodes || !globs.echo)
	return;
    if(!count++)
	return;
    long pos = get_varpos_xy(mousex,mousey);
    if (pos < 0)
	return;
    if (lines_echoed)
	printf("\033[%iB\r", lines_echoed-1); // overwrite the last echoed line
    nct_print_datum(var->dtype, var->data + pos*nctypelen(var->dtype));
    printf(" [%zu pos=(%i,%i) coords=(", pos, varpos_xy_j, varpos_xy_i);
    if (yid >= 0) {
	_maybe_print_mousecoordinate(yid, varpos_xy_j);
	putchar(',');
    }
    _maybe_print_mousecoordinate(xid, varpos_xy_i);
    printf(")]\033[K\n");
    printf("\033[%iA\r", lines_echoed);
}

static void mousewheel() {
    int num = event.wheel.y;
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

static void mousemove() {
    static float move_datax, move_datay;
    move_datax += event.motion.xrel * data_per_pixel;
    move_datay += event.motion.yrel * data_per_pixel;
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

    SDL_SetRenderTarget(rend, base);
    draw_funcptr(var);
    if (globs.coastlines) {
	if (!plt.area->coasts)
	    init_coastlines(plt.area, NULL);
	draw_coastlines(plt.area);
    }
    SDL_SetRenderTarget(rend, NULL);
}

static void resized() {
    static uint_fast64_t lasttime;
    uint_fast64_t thistime = time_now_ms();
    if(thistime-lasttime < 16) {
	call_resized = 1;
	return;
    }
    call_resized = 0;
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w == win_w && h == win_h)
	return;
    win_w = w; win_h = h;
    call_redraw = 1;
    lasttime = thistime;
    SDL_DestroyTexture(base);
    SDL_GetWindowSize(window, &win_w, &win_h);
    base = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, win_w, win_h);
    set_draw_params();
}

static void set_dimids() {
    xid = var->ndims-1;
    yid = var->ndims-2;
    zid = var->ndims-3;
    draw_funcptr = yid<0? draw1d: draw2d;
    if (zid < 0)
	zid = -1;
    if(zid>=0) {
	plt.zvar = var->super->dims[var->dimids[zid]];
	plt.time0 = nct_mktime0_nofail(plt.zvar, NULL);
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

    g_size1 = nctypelen(var->dtype);
    g_xlen = nct_get_vardim(var, xid)->len;
    if(yid>=0) {
	g_ylen  = nct_get_vardim(var, yid)->len;
	data_per_pixel = GET_SPACE(g_xlen, win_w, g_ylen, win_h-cmapspace-cmappix);
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
    draw_h = MIN(win_h-cmapspace-cmappix, draw_h);
    if (zid < 0) zid = -1;
    plt.stepsize_z = nct_get_len_from(var, zid+1); // works even if zid == -1
    plt.stepsize_z += plt.stepsize_z == 0; // length must be at least 1

    g_pixels_per_datum = globs.exact ? round(1.0 / data_per_pixel) : 1.0 / data_per_pixel;
    g_pixels_per_datum += !g_pixels_per_datum;
    g_data_per_step = g_pixels_per_datum * data_per_pixel; // step is a virtual pixel >= physical pixel

    int if_add_1;
    draw_w = draw_w / g_pixels_per_datum * g_pixels_per_datum;
    if_add_1 = draw_w / g_pixels_per_datum < g_xlen - offset_i && draw_w < win_w;
    draw_w += if_add_1 * g_pixels_per_datum; // may be larger than win_w which is not a problem

    draw_h = draw_h / g_pixels_per_datum * g_pixels_per_datum;
    if (globs.invert_y) {
	if_add_1 = draw_h / g_pixels_per_datum < g_ylen - offset_j && draw_h < win_h && offset_j;
	offset_j -= if_add_1;
	plt.area->offset_j = offset_j;
	plt.area->j_off_by_one = if_add_1;
    }
    else
	if_add_1 = draw_h / g_pixels_per_datum < g_ylen - offset_j && draw_h < win_h;
    draw_h += if_add_1 * g_pixels_per_datum; // may be larger than win_h which is not a problem
    g_extended_y = if_add_1;

    g_only_nans = make_minmax(var->dtype);
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
    if (plt.globs_detached) {
	long wants = MAX(n_plottables, pltind);
	recalloc_list(&globslist, &globslistlen, wants, sizeof(struct nctplot_globals));
	globslist[pltind] = globs;
	globs = globs_mem;
    }

    pltind = nct_varid(var);
    if (!recalloc_list(&plottables, &n_plottables, pltind+1, sizeof(plottable)) && plt.globs_detached) {
	if (pltind >= globslistlen)
	    return variable_changed(); // Shouldn't happen. To allocate globslist.
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
    int i = 0;
    printf("coordinate system: \033[K");
    while (!i)
	for (i=0; i<255; i++)
	    if ((crs[i] = getchar()) == '\n')
		break;
    crs[i] = 0;
    lines_echoed--;
    free(plt.area->crs);
    plt.area->crs = strdup(crs);
    export_projection();
    free(plt.area->coasts);
    plt.area->coasts = NULL;
    call_redraw = 1;
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
    int winh = win_h - cmappix - cmapspace;
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
    printf("Enter a framemumber to jump to \033[K");
    fflush(stdout);
    int arg0, month=0, day=1, hour=0, minute=0;
    float second=0;
    switch (scanf("%d-%d-%d[ *]%d:%d:%f", &arg0, &month, &day, &hour, &minute, &second)) {
	case 1: plt.area->znum = arg0; break; // user entered a frame number
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
	    time_t target_time = mktime(&tm);
	    time_t current_time = nct_mktime(plt.zvar, NULL, &plt.time0, plt.area->znum).a.t;
	    int move = (target_time - current_time)*1000 / nct_get_interval_ms(plt.time0.d);
	    // TODO milliseconds
	    plt.area->znum += move;
	    break;
    }
    if (plt.area->znum < 0)
	plt.area->znum = 0;
    else if (plt.area->znum >= plt.zvar->len)
	plt.area->znum = plt.zvar->len-1;
    lines_echoed--;
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
    clear_echo();
    nct_print(var->super);
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
    printf("enter NAN: \033[K"), fflush(stdout);
    if(scanf("%lli", &globs.nanval) != 1)
	warn("scanf");
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

static void set_sleep(Arg _) {
    printf("Enter sleeptime in ms (default = %i): \033[K", default_sleep);
    fflush(stdout);
    if(scanf("%i", &sleeptime) != 1)
	sleeptime = default_sleep;
    lines_echoed--;
    fflush(stdout);
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
    if (lines_echoed > 0)
	printf("\r\033[%iB", lines_echoed), fflush(stdout);	// move cursor past the echo region
    if (mp_params.dlhandle)
	dlclose(mp_params.dlhandle);
    free_coastlines();
    for(int i=0; i<n_plottables; i++)
	free_plottable(plottables+i);
    free(globslist); globslist = NULL; globslistlen = 0;
    plottables = (free(plottables), NULL);
    mp_params = (struct Mp_params){0};
    memset(&memory, 0, sizeof(memory));
    SDL_DestroyTexture(base);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

#ifdef HAVE_NCTPROJ
static void convert_coord(Arg _) {
    char from[256], to[256];
    int i = 0;
    printf("from: \033[K");
    if (plt.area->crs)
	printf("%s\n", plt.area->crs);
    else {
	while (!i)
	    for (i=0; i<255; i++)
		if ((from[i] = getchar()) == '\n')
		    break;
	from[i] = 0;
	plt.area->crs = strdup(from);
	export_projection();
    }
    printf("to: \033[K");
    i = 0;
    while (!i)
	for (i=0; i<255; i++)
	    if ((to[i] = getchar()) == '\n')
		break;
    lines_echoed -= 2;
    to[i] = 0;
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
	lines_echoed--;
	return; }
    if(!(mp_params.fun = dlsym(mp_params.dlhandle, "function"))) {
	printf("dlsym(\"function\") failed %s\033[K\n", dlerror());
	lines_echoed--;
	return; }
    mp_params.mode = function_mp;
}

static void mp_set_filename(Arg arg) {
    printf("set filename: \033[K");
    if(scanf("%255s", mp_params.filename) < 0)
	warn("mp_set_filename, scanf");
    lines_echoed--;
}

static int mp_set_fvalue(char str[256]) {
    printf("set floating point value or *.so with void* function(void* in, void* out): \033[K");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    lines_echoed--;
    if(sscanf(str, "%f", &mp_params.value.f) == 1) {
	mp_params.mode = fixed_mp;
	return 0; }
    return 1;
}

static int mp_set_lfvalue(char str[256]) {
    printf("set floating point value or *.so with void* function(void* in, void* out): \033[K");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    lines_echoed--;
    if(sscanf(str, "%lf", &mp_params.value.lf) == 1) {
	mp_params.mode = fixed_mp;
	return 0; }
    return 1;
}

static int mp_set_ivalue(char str[256]) {
    printf("set integer value or *.so with void* function(void* in, void* out): \033[K");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    lines_echoed--;
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

#include "bindings.h"

static int get_modstate() {
    /* makes modstate side-insensitive and removes other modifiers than [alt,ctrl,gui,shift] */
    int mod = 0;
    int mod0 = SDL_GetModState();
    if(mod0 & KMOD_CTRL)
	mod |= KMOD_CTRL;
    if(mod0 & KMOD_SHIFT)
	mod |= KMOD_SHIFT;
    if(mod0 & KMOD_ALT)
	mod |= KMOD_ALT;
    if(mod0 & KMOD_GUI)
	mod |= KMOD_GUI;
    return mod;
}

#define handle_keybindings(a) _handle_keybindings(a, ARRSIZE(a))
static int _handle_keybindings(Binding b[], int len) {
    int ret = 0;
    for(int i=0; i<len; i++)
	if(event.key.keysym.sym == b[i].key)
	    if(get_modstate() == b[i].mod) {
		b[i].fun(b[i].arg);
		ret++; // There can be multiple bindings for the same key.
	    }
    return ret;
}

static void keydown_func() {
    if(0);
    else if (prog_mode == variables_m  && handle_keybindings(keydown_bindings_variables_m)) return;
    else if (prog_mode == mousepaint_m && handle_keybindings(keydown_bindings_mousepaint_m)) return;
    else if (prog_mode == colormaps_m  && handle_keybindings(keydown_bindings_colormaps_m)) return;
    handle_keybindings(keydown_bindings);
}

static void mainloop() {
    int mouse_pressed=0;
start:
    while(SDL_PollEvent(&event)) {
	switch(event.type) {
	case SDL_QUIT:
	    quit((Arg){0}); break;
	case SDL_WINDOWEVENT:
	    if(event.window.event==SDL_WINDOWEVENT_RESIZED)
		call_resized = 1;
	    break;
	case SDL_KEYDOWN:
	    keydown_func(); break;
	case SDL_MOUSEMOTION:
	    mousex = event.motion.x;
	    mousey = event.motion.y;
	    if(mouse_pressed) {
		if (prog_mode==mousepaint_m) {
		    mousepaint();
		    call_redraw = 1;
		}
		else mousemove();
	    }
	    else
		mousemotion();
	    break;
	case SDL_MOUSEBUTTONDOWN:
	    mouse_pressed=1; break;
	case SDL_MOUSEBUTTONUP:
	    mouse_pressed=0; break;
	case SDL_MOUSEWHEEL:
	    mousewheel();
	}
	if(stop) return;
    }

    if (stop)		return;
    if (call_resized)	resized();
    if (call_redraw)	redraw(var);
    if (zid < 0)	play_inv = play_on = 0;
    if (play_inv)	{inc_znum((Arg){.i=-1}); play_on=0;}
    if (play_on)	inc_znum((Arg){.i=1});

    SDL_RenderCopy(rend, base, NULL, NULL);
    SDL_RenderPresent(rend);
    SDL_Delay(sleeptime);
    goto start;
}

/* Only following functions should be called from programs. */

void nctplot_(void* vobject, int isset) {
    if (isset) {
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
    if (SDL_Init(SDL_INIT_VIDEO)) {
	nct_puterror("sdl_init: %s\n", SDL_GetError());
	return; }
    SDL_Event event;
    while(SDL_PollEvent(&event));
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm)) {
	nct_puterror("getting monitor size: %s\n", SDL_GetError());
	win_w = win_h = 500;
    } else {
	win_w = dm.w;
	win_h = dm.h;
    }

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
  
    window = SDL_CreateWindow("nctplot", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			      MIN(xlen, win_w), MIN(ylen+cmapspace+cmappix, win_h), SDL_WINDOW_RESIZABLE);
    rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);
    SDL_GetWindowSize(window, &win_w, &win_h);
    base = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, win_w, win_h);
    variable_changed();

    sleeptime = default_sleep;
    stop = lines_echoed = play_on = play_inv = 0;
    update_minmax = 1;
    prev_pltind = pending_varnum = -1;
    mp_params = (struct Mp_params){0};

    mainloop();
}

struct nctplot_globals* nctplot_get_globals() {
    return &globs;
}
