#include "wayland_helper.h"

static struct wl_pointer *mouse;

static void wlh_motion(void *data, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    struct wayland_helper *wlh = data;
    int x1 = wl_fixed_to_int(x);
    int y1 = wl_fixed_to_int(y);
    int xrel = x1 - wlh->mousex;
    int yrel = x1 - wlh->mousey;
    wlh->mousex = x1;
    wlh->mousey = y1;
    if (wlh->motion_callback && (xrel | yrel))
	wlh->motion_callback(wlh, xrel, yrel);
}

static void wlh_wheel(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis, wl_fixed_t value) {
    struct wayland_helper *wlh = data;
    if (wlh->wheel_callback)
	wlh->wheel_callback(wlh, axis, wl_fixed_to_int(value));
}

static struct wl_pointer_listener pointer_listener = {
    .motion = wlh_motion,
    .axis = wlh_wheel,
    .enter = nop, .leave = nop, .button = nop, .frame = nop,
    .axis_source = nop, .axis_stop = nop, .axis_discrete = nop, .axis_value120 = nop,
};

static void wlh_init_mouse(struct wayland_helper *wlh) {
    mouse = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(mouse, &pointer_listener, wlh);
}
