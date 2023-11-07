#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include "wayland_helper/helper.h"

#define bindings_file "bindings_xkb.h"

static struct wayland_helper wlh;
uint32_t wlh_color = 0;
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

/* tämä olisi hyvä toteuttaa eri tavalla */
const static unsigned (*get_modstate)(void*) = (unsigned(*)(void*))wlh_get_modstate;

static void mainloop() {
    while (!wlh.stop && wl_display_roundtrip(wlh.display)) {
	if (wlh.redraw && wlh.can_redraw) {
	    redraw(var);
	    wlh_commit(&wlh);
	}
	if (zid < 0)	play_inv = play_on = 0;
	if (play_inv)	{inc_znum((Arg){.i=-1}); play_on=0;}
	if (play_on)	inc_znum((Arg){.i=1});
	sleep_frame_rate();
    }
    quit((Arg){0});
}
