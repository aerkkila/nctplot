#include "wayland_helper.h"

static struct wl_pointer *mouse;

#define u32 uint32_t

static void wlh_motion(void *data, struct wl_pointer *p, u32 time, wl_fixed_t x, wl_fixed_t y) {
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

static void wlh_wheel(void *data, struct wl_pointer *p, u32 time, u32 axis, wl_fixed_t value) {
    struct wayland_helper *wlh = data;
    if (wlh->wheel_callback)
	wlh->wheel_callback(wlh, axis, wl_fixed_to_int(value));
}

static void wlh_button(void *data, struct wl_pointer *p, u32 serial, u32 time, u32 button, u32 state) {
    struct wayland_helper *wlh = data;
    if (state)
	wlh->button |= 1<<button;
    else
	wlh->button &= ~(1<<button);
    if (wlh->button_callback)
	wlh->button_callback(wlh, button, state);
}

static struct wl_pointer_listener pointer_listener = {
    .motion = wlh_motion,
    .axis = wlh_wheel,
    .button = wlh_button,
    .enter = nop, .leave = nop, .frame = nop,
    .axis_source = nop, .axis_stop = nop, .axis_discrete = nop, .axis_value120 = nop,
};

static void wlh_init_mouse(struct wayland_helper *wlh) {
    mouse = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(mouse, &pointer_listener, wlh);
}

#undef u32
