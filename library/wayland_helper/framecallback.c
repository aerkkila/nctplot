static struct wl_callback* framecaller;

static void framecallback(void*, struct wl_callback*, uint32_t);

static const struct wl_callback_listener frame_listener = {
    framecallback,
};

static void framecallback(void* data, struct wl_callback* callback, uint32_t time) {
    wl_callback_destroy(framecaller);
    framecaller = wl_surface_frame(surface);
    wl_callback_add_listener(framecaller, &frame_listener, data);
    ((struct wayland_helper*)data)->can_redraw = 1;
}
