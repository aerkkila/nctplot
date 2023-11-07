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
    for (int i=0; i<wlh.yres*wlh.xres; i++)
	wlh.data[i] = wlh_color;
}

static void set_scale(int scalex, int scaley) {
    wlh_scalex = scalex;
    wlh_scaley = scaley;
}

static void quit_graphics() {
    wlh_destroy(&wlh);
}

static void graphics_draw_point(int i, int j) {
    wlh.data[j*wlh.xres + i] = wlh_color;
}

static void key_callback(struct wayland_helper *wlh) {
    const xkb_keysym_t* syms;
    int nsyms = wlh_get_keysyms(wlh, &syms);
    for (int isym=0; isym<nsyms; isym++)
	keydown_func(syms[isym], wlh->last_keymods);
}

static void init_graphics() {
    wlh.key_callback = key_callback;
    wlh_init(&wlh);
}

static void mainloop() {
    while (!wlh.stop && wl_display_roundtrip(wlh.display)) {
	if (wlh.redraw && wlh.can_redraw) {
	    redraw(var);
	    wlh_commit(&wlh);
	}
	if (zid < 0)	play_inv = play_on = 0;
	if (play_inv)	{inc_znum((Arg){.i=-1}); play_on=0;}
	if (play_on)	inc_znum((Arg){.i=1});
	usleep(sleeptime*1000);
	if (wlh_key_should_repeat(&wlh))
	    key_callback(&wlh);
    }
    quit((Arg){0});
}
