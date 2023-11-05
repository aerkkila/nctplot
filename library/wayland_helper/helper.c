#include <wayland-client.h>
#include "xdg-shell.h"
#include "xdg-shell.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

struct imagecontent {
    unsigned char* data;
    int xresmin, yresmin, stop, redraw, can_redraw,
	win_w, win_h, // only mutable in xdg:topconfigure
	xres, yres; // buffer size: win_w / xscale
    float xscale, yscale; // only mutable in set_scale
};

/* things to create */
struct wl_surface*    surface;
struct wl_buffer*     buffer;
#include "framecallback.c" // framecaller

/* things to recieve */
struct wl_display*    display;
struct wl_registry*   registry;
struct wl_compositor* compositor;
#include "xdg.c" // xdg_base
#include "shm.c" // shared_memory
struct wl_seat*       seat;

#include "keyboard.c"

#define do_binding if(0)
#define option(a,b,c) } else if(!strcmp(interface, a##_interface.name)) { b = wl_registry_bind(reg, id, &a##_interface, c)

static void registry_handler(void* data, struct wl_registry* reg, uint32_t id,
			     const char* interface, uint32_t version) {
    do_binding { 
	option(wl_compositor, compositor, 4);
	option(xdg_wm_base, xdg_base, 1),
	    xdg_wm_base_add_listener(xdg_base, &xdg_base_listener, NULL);
	option(wl_shm, shared_memory, 1);
	option(wl_seat, seat, 8);
    }
}

#undef do_binding
#undef option

static void registry_destroyer(void* data, struct wl_registry* reg, uint32_t id) {
    wl_registry_destroy(registry);
    registry = NULL;
}

static struct wl_registry_listener registry_listener = {
    .global = registry_handler,
    .global_remove = registry_destroyer,
};

static int require(void* a, ...) {
    va_list vl;
    char* b;
    int has_error = 0;
    va_start(vl, a);
    do {
	b = va_arg(vl, char*);
	if (!a) {
	    fprintf(stderr, "%s is missing\n", b);
	    has_error = 1;
	}
    } while ((a=va_arg(vl, void*)));
    va_end(vl);
    return has_error;
}

int init_wayland(struct imagecontent *image) {
    assert((display = wl_display_connect(NULL)));
    assert((registry = wl_display_get_registry(display)));
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    if (require(display, "display", compositor, "compositor", shared_memory,
	    "shared_memory", xdg_base, "xdg_base", seat, "wl_seat", NULL))
	return 1;

    assert((surface = wl_compositor_create_surface(compositor)));
    framecaller = wl_surface_frame(surface);
    wl_callback_add_listener(framecaller, &frame_listener, image);

    if (image->xresmin <= 0)
	image->xresmin = 1;
    if (image->yresmin <= 0)
	image->yresmin = 1;
    init_xdg(image);
    return 0;
}

void destroy_wayland(struct imagecontent *image) {
    destroy_imagebuffer(image);
    if (buffer) {
	wl_buffer_destroy(buffer);
	buffer = NULL;
    }

    xdg_toplevel_destroy(xdgtop); xdgtop=NULL;
    xdg_surface_destroy(xdgsurf); xdgsurf=NULL;

    wl_surface_destroy(surface); surface=NULL;
    wl_callback_destroy(framecaller); framecaller=NULL;

    if (keyboard)
	destroy_keyboard();

    wl_seat_destroy(seat); seat=NULL;
    wl_shm_destroy(shared_memory); shared_memory=NULL;
    xdg_wm_base_destroy(xdg_base); xdg_base=NULL;
    wl_compositor_destroy(compositor); compositor=NULL;
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
}

void wayland_fullscreen() {
    xdg_toplevel_set_fullscreen(xdgtop, NULL);
}

void wayland_nofullscreen() {
    xdg_toplevel_unset_fullscreen(xdgtop);
}

void wayland_render(struct imagecontent *image) {
    wl_surface_damage_buffer(surface, 0, 0, image->xres, image->yres);
    wl_surface_attach(surface, buffer, 0, 0); // This is always released automatically.
    wl_surface_commit(surface);
    image->redraw = image->can_redraw = 0;
}

#ifdef wayland_test
static void kb_key_callback(void* data, struct wl_keyboard* wlkb, uint32_t serial,
			    uint32_t time, uint32_t key, uint32_t state) {
    key += 8;
    if (!state) {
	repeat_started = 0;
	return;
    }
    static const xkb_keysym_t* syms;
    int ival = xkb_state_key_get_syms(xkbstate, key, &syms);
    for (int i=0; i<ival; i++) {
	if (syms[i] == XKB_KEY_q)
	    ((struct imagecontent*)data)->stop = 1;
    }
}

int main() {
    struct imagecontent imag = {
	.yscale = 2,
	.xscale = 2,
    };
    if (init_wayland(&imag))
	errx(1, "init_wayland");
    init_keyboard(kb_key_callback, &imag);
    xdg_toplevel_set_title(xdgtop, "wltest");
    int number = 0;
    putchar('\n');
    while (!imag.stop && wl_display_roundtrip(display) > 0) {
	if (!imag.can_redraw) {
	    usleep(10000);
	    continue;
	}
	for (int j=0; j<imag.yres; j++) {
	    for (int i=0; i<imag.xres; i++) {
		unsigned char* ptr = imag.data + (j*imag.xres + i) * 4;
		ptr[0] = number*2;
		ptr[1] = j/2*(i/2) + number;
		ptr[2] = j/5*(i/5) + number*5;
		ptr[3] = 0xff;
	    }
	}
	number++;
	printf("\033[A\r%i\n", number);
	wayland_render(&imag);
	usleep(10000);
    }
    destroy_wayland(&imag);
}
#endif
