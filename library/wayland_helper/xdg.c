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
static struct wl_buffer* attach_imagebuffer(struct imagecontent*); // from shm.c

static char xdg_surface_changed = 0;

static void xdgconfigure(void* data, struct xdg_surface* surf, uint32_t serial) {
    xdg_surface_ack_configure(surf, serial);
    if (xdg_surface_changed && buffer) {
	wl_buffer_destroy(buffer);
	buffer = NULL;
	xdg_surface_changed = 0;
    }
    if (!buffer)
	buffer = attach_imagebuffer(data);
    //wl_buffer_add_listener(buffer, &wl_buf_lis, NULL);
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
    //piirrä_uudesti = 1;
}

static struct xdg_surface_listener xdgsurflistener = {
    .configure = xdgconfigure,
};

/*******************************/

static void topconfigure(void* data, struct xdg_toplevel* top, int32_t w, int32_t h, struct wl_array* states) {
    struct imagecontent *im = data;
    int x0 = im->xres, y0 = im->yres;
    w /= im->xscale;
    h /= im->yscale;
    im->xres = w<im->xresmin? im->xresmin: w;
    im->yres = h<im->yresmin? im->yresmin: h;
    xdg_surface_changed = im->xres != x0 || im->yres != y0;
}
static void xdgclose(void* data, struct xdg_toplevel* top) {
    ((struct imagecontent*)data)->stop = 1;
}
static struct xdg_toplevel_listener xdgtoplistener = {
    .configure = topconfigure,
    .close = xdgclose,
};

/*******************************/

static void init_xdg(struct imagecontent *image) {
    assert((xdgsurf = xdg_wm_base_get_xdg_surface(xdg_base, surface)));
    xdg_surface_add_listener(xdgsurf, &xdgsurflistener, image);

    assert((xdgtop  = xdg_surface_get_toplevel(xdgsurf)));
    xdg_toplevel_add_listener(xdgtop, &xdgtoplistener, image);

    wl_surface_commit(surface);
}
