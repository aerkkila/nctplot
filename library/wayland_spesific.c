#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include "wayland_helper/wayland_helper.h"

#ifndef Abs
#define Abs(a) ((a)<0 ? -(a) : (a))
#endif

#ifdef HAVE_TTRA
#include <ttra.h>
static struct ttra ttra;
static int use_ttra = 1;
static int fontheight_ttra = 20;
static const int ttra_space = 8;
#define Printf(...) do { if (use_ttra) ttra_printf(&ttra, __VA_ARGS__); else printf(__VA_ARGS__); } while (0)
#define Nct_print_datum(dtype, datum) do {						\
    if (use_ttra) nct_fprint_datum(dtype, (nct_fprint_t)ttra_printf, &ttra, datum);	\
    else nct_print_datum(dtype, datum);							\
} while (0)
#define update_printarea() do {		\
    if (use_ttra) call_redraw = 1;	\
    else fflush(stdout);		\
} while (0)
#endif

#define bindings_file "bindings_xkb.h"

typedef struct {
    int x, y;
} point_t; // for compatibility with SDL

union point_u { // access the struct as an array
    point_t p;
    int i[2];
};

static struct wayland_helper wlh;
static uint32_t wlh_color = 0;
int wlh_scalex = 1,
    wlh_scaley = 1;

static void set_color(unsigned char* c) {
    wlh_color =
	(c[0] << 16 ) |
	(c[1] << 8 ) |
	(c[2] << 0) |
	(0xff << 24);
}

static void clear_background() {
    for (int i=0; i<win_h*win_w; i++)
	wlh.data[i] = wlh_color;
}

static void set_scale(int scalex, int scaley) {
    wlh_scalex = scalex;
    wlh_scaley = scaley;
}

static void quit_graphics() {
    wlh.stop = 1;
}

static void graphics_draw_point(int i, int j) {
    for (int jj=j*wlh_scaley; jj<(j+1)*wlh_scaley; jj++)
	for (int ii=i*wlh_scalex; ii<(i+1)*wlh_scalex; ii++)
	    wlh.data[jj*win_w + ii] = wlh_color;
}

static void draw_point_in_xscale(int i, int j) {
    for (int ii=i*wlh_scalex; ii<(i+1)*wlh_scalex; ii++)
	wlh.data[j*wlh_scaley*win_w + ii] = wlh_color;
}

static void expand_row_to_yscale(int j) {
    uint32_t* ptr = wlh.data + j*wlh_scaley*win_w;
    for (int jj=1; jj<wlh_scaley; jj++)
	memcpy(ptr+jj*win_w, ptr, win_w*4);
}

#define draw_line_$method draw_line_bresenham

/* https://en.wikipedia.org/wiki/Bresenham's_line_algorithm */
/* This method is nice because it uses only integers. */
static void draw_line_bresenham(const int *xy) {
    int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
    int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0
    int m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep],
	n1=xy[2*!backwards+nosteep],  n0=xy[2*backwards+nosteep];

    const int n_add = n1 > n0 ? 1 : -1;
    const int dm = m1 - m0;
    const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
    const int D_add0 = 2 * dn;
    const int D_add1 = 2 * (dn - dm);
    int D = 2*dn - dm;
    if (nosteep) // (m,n) = (x,y)
	for (; m0<=m1; m0++) {
	    wlh.data[n0*win_w + m0] = wlh_color;
	    n0 += D > 0 ? n_add : 0;
	    D  += D > 0 ? D_add1 : D_add0;
	}
    else // (m,n) = (y,x)
	for (; m0<=m1; m0++) {
	    wlh.data[m0*win_w + n0] = wlh_color;
	    n0 += D > 0 ? n_add : 0;
	    D  += D > 0 ? D_add1 : D_add0;
	}
}

static void draw_lines(const void *v, int n) {
    const union point_u *u = v;
    for (int i=0; i<n-1; i++)
	draw_line_$method(u[i].i);
}

static void key_callback(struct wayland_helper *wlh) {
    if (!wlh->keydown)
	return;

    if (typingmode) {
	char c[32];
	xkb_state_key_get_utf8(wlh->xkbstate, wlh->last_key, c, sizeof(c));
	return typing_input(c);
    }

    const xkb_keysym_t* syms;
    int nsyms = wlh_get_keysyms(wlh, &syms);
    for (int isym=0; isym<nsyms; isym++)
	keydown_func(syms[isym], wlh->last_keymods);
}

static void motion_callback(struct wayland_helper *wlh, int xrel, int yrel) {
    if (!wlh->button) {
	mousemotion(xrel, yrel);
	return;
    }
    if (prog_mode == mousepaint_m) {
	mousepaint(xrel, yrel);
	call_redraw = 1;
    }
    else
	mousemove(xrel, yrel);
}

static void wheel_callback(struct wayland_helper *wlh, int axis, int num) {
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
	mousewheel(num < 0 ? 1 : -1);
}

#define mousex wlh.mousex
#define mousey wlh.mousey

static void init_graphics(int xlen, int ylen) {
    wlh = (struct wayland_helper) {
	.key_callback = key_callback,
	.motion_callback = motion_callback,
	.wheel_callback = wheel_callback,
    };
    wlh_init(&wlh);
#ifdef HAVE_TTRA
    ttra_init(&ttra);
    ttra_set_fontheight(&ttra, fontheight_ttra);
#endif
    win_h = 1;
    win_w = 1;
}

#ifdef HAVE_TTRA
static void set_ttra() {
    ttra.canvas = wlh.data;
    ttra_set_xy0(&ttra, 0, draw_h + cmapspace + cmappix + ttra_space);
    ttra.w = win_w;
    ttra.h = win_h - ttra.y0;
    ttra.realh = win_h;
    ttra.realw = win_w;
}
#endif

static void mainloop() {
    long play_start_ms = 0;
    int play_start_znum;

    while (wl_display_roundtrip(wlh.display) > 0 && !wlh.stop) {
	if (wlh_key_should_repeat(&wlh))
	    key_callback(&wlh);

	if (zid < 0) play_on = 0;

	if (play_on) {
	    if (!play_start_ms) {
		play_start_ms = time_now_ms();
		play_start_znum = plt.area_z->znum;
	    }
	    else {
		double change_s = (time_now_ms() - play_start_ms) * 1e-3;
		int new_znum = play_start_znum + fps * change_s;
		if (new_znum != plt.area_z->znum)
		    inc_znum((Arg){.i = new_znum - plt.area_z->znum});
	    }
	}
	else
	    play_start_ms = 0;

	if (wlh.res_changed) {
	    wlh.res_changed = 0;
	    win_h = wlh.yres;
	    win_w = wlh.xres;
	    set_draw_params();
#ifdef HAVE_TTRA
	    set_ttra();
#endif
	}

	if ((wlh.redraw || call_redraw) && wlh.can_redraw) {
	    redraw(var);
	    wlh_commit(&wlh);
	}
	usleep(sleeptime*1000);
    }

    quit((Arg){0});
#ifdef HAVE_TTRA
    wlh_destroy(&wlh);
    ttra_destroy(&ttra);
    ttra_fini();
#endif
}
