#include <xkbcommon/xkbcommon.h>
#include "waylandhelper/waylandhelper.h"

static struct waylandhelper wlh;
#define bindings_file "bindings_xkb.h"

static void key_callback(struct waylandhelper *wlh) {
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

static void motion_callback(struct waylandhelper *wlh, int xrel, int yrel) {
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

static void wheel_callback(struct waylandhelper *wlh, int axis, int num) {
	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
		mousewheel(num < 0 ? 1 : -1);
}

#define mousex wlh.mousex
#define mousey wlh.mousey

static void init_graphics(int xlen, int ylen) {
	wlh = (struct waylandhelper) {
		.key_callback = key_callback,
			.motion_callback = motion_callback,
			.wheel_callback = wheel_callback,
	};
	wlh_init(&wlh);
	win_h = 1;
	win_w = 1;
	canvas = wlh.data;
}

static void quit_graphics() {
	wlh.stop = 1;
}

static void mainloop() {
	long play_start_ms = 0;
	int play_start_znum;

	while (!wlh.stop && wlh_roundtrip(&wlh) >= 0) {
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
			canvas = wlh.data;
			set_draw_params();
			call_redraw = 1;
#ifdef HAVE_TTRA
			set_ttra();
#endif
		}

		if ((wlh.redraw || call_redraw) && wlh.can_redraw) {
			switch (call_redraw) {
				case 1: redraw(var); break;
				case commit_only_e: break;
			}
			wlh_commit(&wlh);
			call_redraw = 0;
		}
		usleep(sleeptime*1000);
	}

	quit((Arg){0});
	wlh_destroy(&wlh);
}
