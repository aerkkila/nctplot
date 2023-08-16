#include <nctietue3.h>
#include <SDL2/SDL.h>
#include <stdint.h>
#include <curses.h>
#include <sys/time.h>
#include <err.h>
#include <dlfcn.h> // dlopen, dlerror, etc. for mousepaint
#include <unistd.h> // fork
#include <sys/wait.h> // waitpid
#include "nctplot.h"
#include "autogenerated/pager.h"
#ifdef HAVE_NCTPROJ
#include <nctproj.h>
#endif

static struct nctplot_globals globs = {
    .color_fg = {255, 255, 255},
    .echo = 1,
    .invert_y = 1,
    .cache_size = 1L<<31,
};

typedef struct {
    nct_var *var, *zvar;
    nct_anyd time0;
    char minmax[8*2];
    int znum;
    size_t stepsize_z;
    /* for coastlines */
    double* coasts;
    char* crs;
    double x0, y0, xspace, yspace;
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
static int win_w, win_h, xid, yid, zid, draw_w, draw_h, pending_varnum=-1;
static char invert_c, stop, has_echoed, fill_on, play_on, play_inv, update_minmax=1;
static int cmapnum=18, cmappix=30, cmapspace=10, call_resized, call_redraw, offset_i, offset_j;
static float minshift, maxshift, minshift_abs, maxshift_abs, zoom=1;
static float space; // (n(data) / n(pixels)) in one direction
static const char* echo_highlight = "\033[1;93m";
static void (*draw_funcptr)(const nct_var*);
static enum {no_m, variables_m=-100, n_cursesmodes, mousepaint_m} prog_mode = no_m;

typedef union Arg Arg;
typedef struct Binding Binding;

static void my_echo(void* minmax);
static void redraw(nct_var* var);
static void draw_colormap();
static void set_dimids();
static void set_draw_params();
static void end_curses(Arg);
static void curses_write_vars();
static uint_fast64_t time_now_ms();
static void quit(Arg _);

#define ARRSIZE(a) (sizeof(a) / sizeof(*(a)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include "functions.c" // draw1d, draw2d
#ifdef HAVE_SHPLIB
#include "coastlines.c"
#endif

static SDL_Event event;

union Arg {
    void* v;
    float f;
    int   i;
};

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

static void curses_write_vars() {
    int att = COLOR_PAIR(1);
    int xlen, ylen, x;
    getmaxyx(wnd, ylen, xlen);
    int vars=0, row=0, col=0;
    clear();
    int max_x = 0;
    nct_foreach(var->super, svar) {
	vars++;
	move(row, col);
	int att1 = (svar==var)*(A_UNDERLINE|A_BOLD);
	int att2 = (nct_varid(svar)==pending_varnum)*(A_REVERSE);
	attron(att|att1|att2);
	printw("%i. ", vars);
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

static void draw_colormap() {
    float cspace = 255.0f/win_w;
    float di = 0;
    SDL_RenderSetScale(rend, 1, 1);
    if(!invert_c)
	for(int i=0; i<win_w; i++, di+=cspace) {
	    char* c = COLORVALUE(cmapnum, (int)di);
	    SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 255);
	    for(int j=draw_h+cmapspace; j<draw_h+cmapspace+cmappix; j++)
		SDL_RenderDrawPoint(rend, i, j);
	}
    else
	for(int i=win_w-1; i>=0; i--, di+=cspace) {
	    char* c = COLORVALUE(cmapnum, (int)di);
	    SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 255);
	    for(int j=draw_h+cmapspace; j<draw_h+cmapspace+cmappix; j++)
		SDL_RenderDrawPoint(rend, i, j);
	}	
}

#define A echo_highlight
#define B nct_default_color
static void my_echo(void* minmax) {
    if (!(globs.echo && prog_mode > n_cursesmodes))
	return;
    nct_var *zvar = plt.zvar;
    int size1 = nctypelen(var->dtype);
    for(int i=0; i<echo_h*!has_echoed; i++)
	putchar('\n');
    has_echoed = 1;
    printf("\r\033[%iA", echo_h); // move cursor to start
    printf("%s%s%s: ", A, var->name, B);
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
	printf(", z: %s%s(%i/%zu ", A, zvar->name, plt.znum+1, zvar->len);
	if (plt.time0.d >= 0) {
	    char help[128];
	    strftime(help, 128, "%F %T", nct_localtime((long)nct_get_integer(zvar, plt.znum), plt.time0));
	    printf(" %s", help);
	}
	else if (nct_iscoord(zvar))
	    nct_print_datum(zvar->dtype, zvar->data+plt.znum*nctypelen(zvar->dtype));
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

static long get_varpos_xy(int x, int y) {
    int xlen = nct_get_vardim(var, xid)->len;
    int ylen = yid < 0 ? 0 : nct_get_vardim(var, yid)->len;
    y = (int)(y*space) + offset_j;
    x = (int)(x*space) + offset_i;
    if (x>=xlen || (y>=ylen && yid >= 0))
	return -1;
    if (globs.invert_y && yid>=0)
	y = nct_get_vardim(var, yid)->len - y - 1;
    return (zid>=0)*xlen*ylen*plt.znum + (yid>=0)*xlen*y + x;
}

static void mousemotion() {
    static int count;
    if(prog_mode < n_cursesmodes || !globs.echo)
	return;
    if(!count++)
	return;
    int x = event.motion.x;
    int y = event.motion.y;
    long pos = get_varpos_xy(x,y);
    if (pos < 0)
	return;
    printf("\033[A\r");
    nct_print_datum(var->dtype, var->data + pos*nctypelen(var->dtype));
    printf("[%zu (%i,%i)]\033[K\n", pos,(int)(y*space),(int)(x*space));
}

struct {
    long sum_of_variables, used_memory;
} memory;

static void manage_memory() {
    long startpos = plt.znum * plt.stepsize_z;
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

    if (update_minmax) {
	update_minmax = 0;
	if (globs.usenan)
	    nct_minmax_nan(var, globs.nanval, plt.minmax);
	else
	    nct_minmax(var, plt.minmax);
    }

    SDL_SetRenderTarget(rend, base);
    draw_funcptr(var);
    if (globs.coastlines) {
	if (!plt.coasts)
	    init_coastlines(&plt, NULL);
	draw_coastlines(&plt);
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
    int xlen = nct_get_vardim(var, xid)->len, ylen;
    if(yid>=0) {
	ylen  = nct_get_vardim(var, yid)->len;
	space = GET_SPACE(xlen, win_w, ylen, win_h-cmapspace-cmappix);
    } else {
	space = (float)(xlen)/(win_w);
	ylen  = win_h * space;
    }
    space *= zoom;
    if (offset_i < 0) offset_i = 0;
    if (offset_j < 0) offset_j = 0;
    draw_w = (xlen-offset_i) / space; // how many pixels data can reach
    draw_h = (ylen-offset_j) / space;
    draw_w = MIN(win_w, draw_w);
    draw_h = MIN(win_h-cmapspace-cmappix, draw_h);
    plt.stepsize_z = nct_get_len_from(var, zid+1); // works even if zid == -1
}

static uint_fast64_t time_now_ms() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000 + t.tv_usec/1000;
}

static void variable_changed() {
    pltind = nct_varid(var);
    if (pltind >= n_plottables) { // possible if a new variable was created
	int add = pltind + 1 - n_plottables;
	n_plottables = pltind + 1;
	plottables = realloc(plottables, n_plottables*sizeof(plottable));
	memset(plottables+n_plottables-add, 0, add*sizeof(plottable));
    }
    if (!plt.var) // using this variable for the first time
	update_minmax = 1;
    plt.var = var;
    set_dimids();
    manage_memory();
    set_draw_params();
    nct_att* att;
    if (var->dtype != NC_FLOAT && var->dtype != NC_DOUBLE &&
	    ((att = nct_get_varatt(var, "_FillValue")) || (att = nct_get_varatt(var, "FillValue")))) {
	globs.usenan = 1;
	globs.nanval = nct_getatt_integer(att, 0);
    }
    call_redraw = 1;
}

static void export_projection() {
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
}

static void ask_crs(Arg _) {
    char crs[256];
    int i = 0;
    printf("coordinate system: ");
    while (!i)
	for (i=0; i<255; i++)
	    if ((crs[i] = getchar()) == '\n')
		break;
    free(plt.crs);
    plt.crs = strdup(crs);
    export_projection();
    free(plt.coasts);
    plt.coasts = NULL;
    call_redraw = 1;
}

static void cmap_ichange(Arg jump) {
    int len = ARRSIZE(colormaps) / 2;
    cmapnum = (cmapnum+len+jump.i) % len;
    call_redraw = 1;
}

static void end_curses(Arg _) {
    endwin();
    prog_mode = no_m;
}

static void show_bindings(Arg _) {
    int a = fork();
    if (!a) {
	int a = fork();
	if (!a) {
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
    offset_i += arg.i;
    set_draw_params();
    call_redraw = 1;
}

static void inc_offset_j(Arg arg) {
    offset_j += globs.invert_y ? -arg.i : arg.i;
    set_draw_params();
    call_redraw = 1;
}

static void inc_znum(Arg intarg) {
    if (!plt.zvar)
	return;
    size_t zlen = plt.zvar->len;
    /* below: znum + intarg.i, but goes around when zlen or a negative number is reached. */
    plt.znum = (plt.znum + zlen + intarg.i) % zlen;
    call_redraw = 1;
}

static void inc_zoom(Arg arg) {
    zoom += arg.f;
    set_draw_params();
    call_redraw = 1;
}

static void jump_to(Arg _) {
    printf("Enter a framemumber to jump to ");
    fflush(stdout);
    if(scanf("%i", &plt.znum) != 1)
	plt.znum = 0;
    printf("\033[A\r\033[K");
    fflush(stdout);
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

static void print_var(Arg _) {
    if(has_echoed)
	for(int i=0; i<echo_h; i++)
	    printf("\033[A\033[2K");
    nct_print(var->super);
    has_echoed = 0;
}

static void shift_max(Arg shift) {
    maxshift += shift.f;
    call_redraw = 1;
}

static void shift_max_abs(Arg shift) {
    maxshift_abs += shift.f;
    call_redraw = 1;
}

static void shift_min(Arg shift) {
    minshift += shift.f;
    call_redraw = 1;
}

static void shift_min_abs(Arg shift) {
    minshift_abs += shift.f;
    call_redraw = 1;
}

static void toggle_var(Arg intptr) {
    *(char*)intptr.v = !*(char*)intptr.v;
    set_draw_params();
    call_redraw = 1;
}

static void set_nan(Arg _) {
    printf("enter NAN: "), fflush(stdout);
    if(scanf("%lli", &globs.nanval) != 1)
	warn("scanf");
    globs.usenan = update_minmax = call_redraw = 1;
}

static void use_pending(Arg _) {
    prev_pltind = pltind;
    if(pending_varnum >= 0) {
	var = var->super->vars[pending_varnum];
	pending_varnum = -1;
    }
    variable_changed();
}

static void use_and_exit(Arg _) {
    end_curses(_);
    use_pending(_);
}

static void var_ichange(Arg jump) {
    nct_var* v;
    if(jump.i > 0) {
	if(!(v = nct_nextvar(var)))
	    v = nct_firstvar(var->super);
    } else {
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
    curses_write_vars();
}

static void set_sleep(Arg _) {
    printf("Enter sleeptime in ms (default = %i): ", default_sleep);
    fflush(stdout);
    if(scanf("%i", &sleeptime) != 1)
	sleeptime = default_sleep;
    printf("\033[A\r\033[K");
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
    free(plott->coasts);
    free(plott->crs);
}

static void quit(Arg _) {
    stop = 1;
    if (prog_mode < n_cursesmodes)
	end_curses((Arg){0});
    if (mp_params.dlhandle)
	dlclose(mp_params.dlhandle);
    free_coastlines();
    for(int i=0; i<n_plottables; i++)
	free_plottable(plottables+i);
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
    printf("from: ");
    if (plt.crs)
	printf("%s\n", plt.crs);
    else {
	while (!i)
	    for (i=0; i<255; i++)
		if ((from[i] = getchar()) == '\n')
		    break;
	from[i] = 0;
	plt.crs = strdup(from);
	export_projection();
    }
    printf("to: ");
    i = 0;
    while (!i)
	for (i=0; i<255; i++)
	    if ((to[i] = getchar()) == '\n')
		break;
    printf("\033[A\r\033[K");
    printf("\033[A\r\033[K");
    to[i] = 0;
    var = nctproj_open_converted_var(var, plt.crs, to, NULL);
    nct_load_stream(var, var->len);
    variable_changed();
    plt.crs = strdup(to); // not before variable_changed()
}
#endif

static int mp_set_fvalue(char str[256]);
static int mp_set_lfvalue(char str[256]);
static int mp_set_ivalue(char str[256]);

#define TMP mp_params.size
static void mp_replace_val(void* new_val) {
    for(int j=-TMP; j<=TMP; j++)
	for(int i=-TMP; i<=TMP; i++) {
	    int x = event.motion.x + i;
	    int y = event.motion.y + j;
	    if(x < 0 || y < 0) continue;
	    long pos = get_varpos_xy(x,y)*nctypelen(var->dtype);
	    if(pos < 0) continue;
	    memcpy(var->data+pos, new_val, nctypelen(var->dtype));
	}
}

static void mp_replace_fun() {
    for(int j=-TMP; j<=TMP; j++)
	for(int i=-TMP; i<=TMP; i++) {
	    int x = event.motion.x + i;
	    int y = event.motion.y + j;
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
    data += ((zid >= 0) * plt.znum * plt.stepsize_z - var->startpos) * nctypelen(var->dtype);
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
	printf("dlopen %s: %s\n", str, dlerror());
	has_echoed = 0;
	return; }
    if(!(mp_params.fun = dlsym(mp_params.dlhandle, "function"))) {
	printf("dlsym(\"function\") failed %s\n", dlerror());
	has_echoed = 0;
	return; }
    mp_params.mode = function_mp;
}

static void mp_set_filename(Arg arg) {
    printf("set filename: ");
    if(scanf("%255s", mp_params.filename) < 0) {
	warn("mp_set_filename, scanf");
	has_echoed = 0;
    }
    else
	printf("\033[A\r\033[K");
}

static int mp_set_fvalue(char str[256]) {
    printf("set floating point value or *.so with void* function(void* in, void* out): ");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    printf("\033[A\r\033[K");
    if(sscanf(str, "%f", &mp_params.value.f) == 1) {
	mp_params.mode = fixed_mp;
	return 0; }
    return 1;
}

static int mp_set_lfvalue(char str[256]) {
    printf("set floating point value or *.so with void* function(void* in, void* out): ");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    printf("\033[A\r\033[K");
    if(sscanf(str, "%lf", &mp_params.value.lf) == 1) {
	mp_params.mode = fixed_mp;
	return 0; }
    return 1;
}

static int mp_set_ivalue(char str[256]) {
    printf("set integer value or *.so with void* function(void* in, void* out): ");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    printf("\033[A\r\033[K");
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
	    if(mouse_pressed && prog_mode==mousepaint_m) {
		mousepaint();
		call_redraw = 1;
	    }
	    else
		mousemotion();
	    break;
	case SDL_MOUSEBUTTONDOWN:
	    mouse_pressed=1; break;
	case SDL_MOUSEBUTTONUP:
	    mouse_pressed=0; break;
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
    var = isset ? nct_firstvar((nct_set*)(vobject)) : vobject;
    if (!var) {
	nct_puterror("No variable to plot\n");
	return; }
    
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
  
    window = SDL_CreateWindow("Figure", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			      MIN(xlen, win_w), MIN(ylen+cmapspace+cmappix, win_h), SDL_WINDOW_RESIZABLE);
    rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);
    SDL_GetWindowSize(window, &win_w, &win_h);
    base = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, win_w, win_h);
    variable_changed();

    sleeptime = default_sleep;
    stop = has_echoed = play_on = play_inv = 0;
    update_minmax = 1;
    prev_pltind = pending_varnum = -1;
    mp_params = (struct Mp_params){0};

    mainloop();
}

struct nctplot_globals* nctplot_get_globals() {
    return &globs;
}
