#include <nctietue3.h>
#include <SDL2/SDL.h>
#include <stdint.h>
#include <curses.h>
#include <sys/time.h>
#include <err.h>
#include <dlfcn.h> // dlopen, dlerror, etc. for mousepaint
#include "nctplot.h"

static struct nctplot_globals globs = {
    .color_fg = {255, 255, 255},
    .echo = 1,
    .invert_y = 1,
};

/* Static variables. Global in the context of this library. */
static SDL_Renderer* rend;
static SDL_Window* window;
static SDL_Texture* base;
static nct_var *var, *zvar, *lastvar;
static nct_anyd time0 = {.d=-1};
static WINDOW *wnd;
static const Uint32 default_sleep=8; // ms
static Uint32 sleeptime;
static int win_w, win_h, xid, yid, zid, draw_w, draw_h, znum, pending_varnum=-1;
static size_t stepsize_z;
static char invert_c, stop, has_echoed, fill_on, play_on, play_inv, update_minmax=1;
static int cmapnum=18, cmappix=30, cmapspace=10, call_resized, call_redraw, offset_i, offset_j;
static float minshift, maxshift, minshift_abs, maxshift_abs, zoom=1;
static float space; // (n(data) / n(pixels)) in one direction
static const char* echo_highlight = "\033[1;93m";
static void (*draw_funcptr)(const nct_var*);
static enum {no_m, variables_m=-100, n_cursesmodes, mousepaint_m} prog_mode = no_m;

typedef union Arg Arg;
typedef struct Binding Binding;

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

#include "functions.c"
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

static long get_varpos_xy(int x, int y) {
    int xlen = NCTVARDIM(var, xid)->len;
    int ylen = NCTVARDIM(var, yid)->len;
    y = (int)(y*space) + offset_j;
    x = (int)(x*space) + offset_i;
    if(x>=xlen || y>=ylen) return -1;
    if(globs.invert_y && yid>=0)
	y = NCTVARDIM(var, yid)->len - y - 1;
    return (zid>=0)*xlen*ylen*znum + (yid>=0)*xlen*y + x;
}

static void mousemotion() {
    static int count;
    if(prog_mode == variables_m || !globs.echo) return;
    if(!count++) return;
    int x = event.motion.x;
    int y = event.motion.y;
    long pos = get_varpos_xy(x,y);
    if(pos < 0) return;
    printf("\033[A\r");
    print_value(var, pos);
    printf("[%zu (%i,%i)]\033[K\n", pos,(int)(y*space),(int)(x*space));
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

    if (!var->data) {
	if (!nct_loadable(var)) {
	    nct_puterror("Cannot load the variable.\n");
	    quit((Arg){});
	}
	nct_load(var);
    }

    SDL_SetRenderTarget(rend, base);
    draw_funcptr(var);
    if (globs.coastlines)
	coastlines(NULL);
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
	zvar = var->super->dims[var->dimids[zid]];
	time0 = nct_mktime0_nofail(zvar, NULL);
	if(!zvar->data && nct_iscoord(zvar))
	    nct_load(zvar);
    }
    else {
	zvar = NULL;
	time0.d = -1;
    }
}

#define GET_SPACE_FILL(xlen,win_w,ylen,win_h)    MIN((float)(ylen)/(win_h), (float)(xlen)/(win_w))
#define GET_SPACE_NONFILL(xlen,win_w,ylen,win_h) MAX((float)(ylen)/(win_h), (float)(xlen)/(win_w))
#define GET_SPACE(a,b,c,d) (fill_on? GET_SPACE_FILL(a,b,c,d): GET_SPACE_NONFILL(a,b,c,d))

static void set_draw_params() {
    int xlen = NCTVARDIM(var, xid)->len, ylen;
    if(yid>=0) {
	ylen  = NCTVARDIM(var, yid)->len;
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
    if(zid>=0)
	stepsize_z = nct_get_len_from(var, zid+1);
}

static uint_fast64_t time_now_ms() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000 + t.tv_usec/1000;
}

static void variable_changed() {
    if(!var->data)
	nct_load(var);
    set_dimids();
    set_draw_params();
    call_redraw = update_minmax = 1;
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
    size_t zlen = NCTVARDIM(var,zid)->len;
    znum = (znum + zlen + intarg.i) % zlen;
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
    if(scanf("%i", &znum) != 1)
	znum = 0;
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
    globs.usenan = 1;
}

static void use_pending(Arg _);

static void use_and_exit(Arg _) {
    use_pending(_);
    end_curses(_);
}

static void use_pending(Arg _) {
    lastvar = var;
    if(pending_varnum >= 0) {
	var = var->super->vars[pending_varnum];
	pending_varnum = -1;
    }
    variable_changed();
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
    if(!lastvar) return;
    nct_var* tmp = var;
    var = lastvar;
    lastvar = tmp;
    variable_changed();
}

static void quit(Arg _) {
    stop = 1;
    if(prog_mode != no_m)
	end_curses((Arg){0});
    if(mp_params.dlhandle)
	dlclose(mp_params.dlhandle);
    mp_params = (struct Mp_params){0};
    SDL_DestroyTexture(base);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static int mp_set_fvalue(char str[256]);
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

static void mp_set_action(Arg arg) {
    char str[256];
    if(var->dtype == NC_DOUBLE || var->dtype == NC_FLOAT) {
	if(!mp_set_fvalue(str)) return; }
    else {
	if(!mp_set_ivalue(str)) return; }

    if(mp_params.dlhandle) {
	dlclose(mp_params.dlhandle);
	mp_params.fun = mp_params.dlhandle = NULL;
    }
    mp_params.dlhandle = dlopen(str, RTLD_LAZY);
    if(!mp_params.dlhandle) {
	printf("dlopen %s: %s\n", str, dlerror());
	return; }
    if(!(mp_params.fun = dlsym(mp_params.dlhandle, "function"))) {
	printf("dlsym(\"function\") failed %s\n", dlerror());
	return; }
    mp_params.mode = function_mp;
}

static void mp_set_filename(Arg arg) {
    printf("set filename: ");
    if(scanf("%255s", mp_params.filename) < 0)
	warn("mp_set_filename, scanf");
}

static int mp_set_fvalue(char str[256]) {
    printf("set floating point value or *.so with void function(void* in, void* out): ");
    if(scanf("%255s", str) != 1)
	warn("scanf");
    if(sscanf(str, "%lf", &mp_params.value.lf) == 1) {
	mp_params.mode = fixed_mp;
	return 0; }
    return 1;
}

static int mp_set_ivalue(char str[256]) {
    printf("set integer value or *.so with void function(void* in, void* out): ");
    if(scanf("%255s", str) != 1)
	warn("scanf");
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

/* In mousepaint_m mode these override the default bindings. */
static Binding keydown_bindings_mousepaint_m[] = {
    { SDLK_SPACE,     0,          mp_set_action,           },
    { SDLK_RETURN,    0,          mp_save,                 },
    { SDLK_KP_ENTER,  0,          mp_save,                 },
    { SDLK_s,         0,          mp_set_filename,         },
    { SDLK_PLUS,      0,          mp_size,         {.i=1}  },
    { SDLK_MINUS,     0,          mp_size,         {.i=-1} },
};

/* In variables_m mode these override the default bindings. */
static Binding keydown_bindings_variables_m[] = {
    { SDLK_UP,       0,          pending_var_dec, },
    { SDLK_DOWN,     0,          pending_var_inc, },
    { SDLK_RETURN,   0,          use_and_exit,    },
    { SDLK_KP_ENTER, 0,          use_and_exit,    },
};

static Binding keydown_bindings[] = {
    { SDLK_q,        0,                   quit,          {0}               },
    { SDLK_ESCAPE,   0,                   end_curses,    {0}               },
    { SDLK_1,        0,                   shift_min,     {.f=-0.02}        },
    { SDLK_1,        KMOD_SHIFT,          shift_max,     {.f=-0.02}        },
    { SDLK_2,        0,                   shift_min,     {.f=0.02}         },
    { SDLK_2,        KMOD_SHIFT,          shift_max,     {.f=0.02}         },
    { SDLK_1,        KMOD_ALT,            shift_min_abs, {.f=-0.02}        },
    { SDLK_1,        KMOD_SHIFT|KMOD_ALT, shift_max_abs, {.f=-0.02}        },
    { SDLK_2,        KMOD_ALT,            shift_min_abs, {.f=0.02}         },
    { SDLK_2,        KMOD_SHIFT|KMOD_ALT, shift_max_abs, {.f=0.02}         },
    { SDLK_e,        0,                   toggle_var,    {.v=&globs.echo}  },
    { SDLK_f,        0,                   toggle_var,    {.v=&fill_on}     },
    { SDLK_i,        0,                   toggle_var,    {.v=&globs.invert_y}},
    { SDLK_j,        0,                   jump_to,       {0}               },
    { SDLK_SPACE,    0,                   toggle_var,    {.v=&play_on}     },
    { SDLK_SPACE,    KMOD_SHIFT,          toggle_var,    {.v=&play_inv}    },
    { SDLK_c,        0,                   cmap_ichange,  {.i=1}            },
    { SDLK_c,        KMOD_SHIFT,          cmap_ichange,  {.i=-1}           },
    { SDLK_c,        KMOD_ALT,            toggle_var,    {.v=&invert_c}    },
    { SDLK_l,	     0,			  toggle_var,    {.v=&globs.coastlines}},
    { SDLK_v,        0,                   var_ichange,   {.i=1}            },
    { SDLK_v,        KMOD_SHIFT,          var_ichange,   {.i=-1}           },
    { SDLK_v,        KMOD_ALT,            set_prog_mode, {.i=variables_m}  },
    { SDLK_w,        0,                   use_lastvar,                     },
    { SDLK_m,        0,                   set_prog_mode, {.i=mousepaint_m} },
    { SDLK_n,        0,                   set_nan,			   },
    { SDLK_n,        KMOD_SHIFT,          toggle_var,	 {.v=&globs.usenan}},
    { SDLK_p,	     0,			  print_var,			   },
    { SDLK_PLUS,     0,			  inc_zoom,	 {.f=-0.04}	   }, // smaller number is more zoom
    { SDLK_MINUS,    0,			  inc_zoom,	 {.f=+0.04}	   }, // larger number is less zoom
    { SDLK_RIGHT,    0,                   inc_znum,      {.i=1}            },
    { SDLK_LEFT,     0,                   inc_znum,      {.i=-1}           },
    { SDLK_RIGHT,    KMOD_SHIFT,          inc_offset_i,  {.i=7}            },
    { SDLK_LEFT,     KMOD_SHIFT,          inc_offset_i,  {.i=-7}           },
    { SDLK_RIGHT,    KMOD_SHIFT|KMOD_ALT, inc_offset_i,  {.i=1}            },
    { SDLK_LEFT,     KMOD_SHIFT|KMOD_ALT, inc_offset_i,  {.i=-1}           },
    { SDLK_UP,	     KMOD_SHIFT,          inc_offset_j,  {.i=-7}           },
    { SDLK_DOWN,     KMOD_SHIFT,          inc_offset_j,  {.i=7}            },
    { SDLK_UP,	     KMOD_SHIFT|KMOD_ALT, inc_offset_j,  {.i=-1}           },
    { SDLK_DOWN,     KMOD_SHIFT|KMOD_ALT, inc_offset_j,  {.i=1}            },
    { SDLK_s,        0,                   set_sleep,     {0}               },
    { SDLK_RETURN,   0,                   use_pending,   {0}               },
    { SDLK_KP_ENTER, 0,                   use_pending,   {0}               },
};

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
    set_draw_params();

    sleeptime = default_sleep;
    stop = has_echoed = play_on = play_inv = 0;
    call_redraw = update_minmax = 1;
    lastvar = NULL;
    mp_params = (struct Mp_params){0};

    mainloop();
}

struct nctplot_globals* nctplot_get_globals() {
    return &globs;
}
