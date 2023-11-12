#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include "wayland_helper/wayland_helper.h"

#define bindings_file "bindings_xkb.h"

static struct wayland_helper wlh;
static uint32_t wlh_color = 0;
int wlh_scalex = 1,
    wlh_scaley = 1;

static void set_color(unsigned char* c) {
    wlh_color = 
	(c[0] << 0 ) |
	(c[1] << 8 ) |
	(c[2] << 16) |
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

static void key_callback(struct wayland_helper *wlh) {
    if (!wlh->keydown)
	return;
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
    win_h = 1;
    win_w = 1;
}

static void mainloop() {
    while (wl_display_roundtrip(wlh.display) > 0 && !wlh.stop) {
	if (wlh_key_should_repeat(&wlh))
	    key_callback(&wlh);
	if (zid < 0)	play_inv = play_on = 0;
	if (play_inv)	{inc_znum((Arg){.i=-1}); play_on=0;}
	if (play_on)	inc_znum((Arg){.i=1});
	if (wlh.res_changed) {
	    wlh.res_changed = 0;
	    win_h = wlh.yres;
	    win_w = wlh.xres;
	    set_draw_params();
	}
	if ((wlh.redraw || call_redraw) && wlh.can_redraw) {
	    redraw(var);
	    wlh_commit(&wlh);
	}
	usleep(sleeptime*1000);
    }
    quit((Arg){0});
    wlh_destroy(&wlh);
}
