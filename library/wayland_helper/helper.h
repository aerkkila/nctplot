#ifndef __wayland_helper__
#define __wayland_helper__
#include <xkbcommon/xkbcommon.h>
#include <wayland-client.h>
#include <stdint.h>

/* These enum is copied here from wlroots to avoid making that a dependency. */
enum wlr_keyboard_modifier {
	WLR_MODIFIER_SHIFT = 1 << 0,
	WLR_MODIFIER_CAPS = 1 << 1,
	WLR_MODIFIER_CTRL = 1 << 2,
	WLR_MODIFIER_ALT = 1 << 3,
	WLR_MODIFIER_MOD2 = 1 << 4,
	WLR_MODIFIER_MOD3 = 1 << 5,
	WLR_MODIFIER_LOGO = 1 << 6,
	WLR_MODIFIER_MOD5 = 1 << 7,
};

struct wayland_helper {
    uint32_t* data;
    int xresmin, yresmin, stop, redraw, can_redraw,
	xres, yres; // only mutable in xdg:topconfigure
    struct wl_display* display;
    struct xkb_state* xkbstate;
};

unsigned wlh_get_modstate(const struct wayland_helper *wlh);
void wlh_commit(struct wayland_helper *wlh);
void wlh_nofullscreen();
void wlh_destroy(struct wayland_helper *wlh);
void wlh_fullscreen();
int wlh_init_wayland(struct wayland_helper *wlh);

#endif
