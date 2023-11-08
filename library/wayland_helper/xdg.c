static struct xdg_wm_base* xdg_base; // used in the main file
static struct xdg_surface*   xdgsurf;
static struct xdg_toplevel*  xdgtop;

/*******************************/

static void xdg_wm_base_ping(void* data, struct xdg_wm_base *xwb, uint32_t ser) {
    xdg_wm_base_pong(xwb, ser);
}

static struct xdg_wm_base_listener xdg_base_listener = {
    .ping = xdg_wm_base_ping,
};

/*******************************/
static struct wl_buffer* attach_imagebuffer(struct wayland_helper*); // from shm.c

static char xdg_surface_changed = 0;
static int pending_x, pending_y;

static void xdgconfigure(void* data, struct xdg_surface* surf, uint32_t serial) {
    struct wayland_helper *wlh = data;
    xdg_surface_ack_configure(surf, serial);
    if (xdg_surface_changed && buffer) {
	wlh->xres = pending_x;
	wlh->yres = pending_y;
	wl_buffer_destroy(buffer);
	buffer = NULL;
	xdg_surface_changed = 0;
	wlh->res_changed = 1;
    }
    if (!buffer)
	buffer = attach_imagebuffer(data);
    //wl_buffer_add_listener(buffer, &wl_buf_lis, NULL);
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
}

static struct xdg_surface_listener xdgsurflistener = {
    .configure = xdgconfigure,
};

/*******************************/

static void topconfigure(void* data, struct xdg_toplevel* top, int32_t w, int32_t h, struct wl_array* states) {
    struct wayland_helper *wlh = data;
    pending_x = w<wlh->xresmin ? wlh->xresmin : w;
    pending_y = h<wlh->yresmin ? wlh->yresmin : h;
    xdg_surface_changed = wlh->xres != pending_x || wlh->yres != pending_y;
}

static void xdgclose(void* data, struct xdg_toplevel* top) {
    ((struct wayland_helper*)data)->stop = 1;
}

static struct xdg_toplevel_listener xdgtoplistener = {
    .configure = topconfigure,
    .close = xdgclose,
};

/*******************************/

static void init_xdg(struct wayland_helper *image) {
    assert((xdgsurf = xdg_wm_base_get_xdg_surface(xdg_base, surface)));
    xdg_surface_add_listener(xdgsurf, &xdgsurflistener, image);

    assert((xdgtop  = xdg_surface_get_toplevel(xdgsurf)));
    xdg_toplevel_add_listener(xdgtop, &xdgtoplistener, image);

    wl_surface_commit(surface);
}
