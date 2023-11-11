#include <wayland-client.h>
#include <xkbcommon/xkbcommon-names.h>
#include "xdg-shell.h"
#include "xdg-shell.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "wayland_helper.h"

/* things to create */
static struct wl_surface*    surface;
static struct wl_buffer*     buffer;
#include "framecallback.c" // framecaller

/* things to recieve */
static struct wl_registry*   registry;
static struct wl_compositor* compositor;
static struct wl_seat*       seat;
#include "xdg.c" // xdg_base
#include "shm.c" // shared_memory

#include "keyboard.c"
#include "mouse.c"

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

int wlh_init(struct wayland_helper *wlh) {
    assert((wlh->display = wl_display_connect(NULL)));
    assert((registry = wl_display_get_registry(wlh->display)));
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_dispatch(wlh->display);
    wl_display_roundtrip(wlh->display);
    if (require(wlh->display, "display", compositor, "compositor", shared_memory,
	    "shared_memory", xdg_base, "xdg_base", seat, "wl_seat", NULL))
	return 1;

    assert((surface = wl_compositor_create_surface(compositor)));
    framecaller = wl_surface_frame(surface);
    wl_callback_add_listener(framecaller, &frame_listener, wlh);

    wlh->redraw = 1;

    if (wlh->xresmin <= 0)
	wlh->xresmin = 1;
    if (wlh->yresmin <= 0)
	wlh->yresmin = 1;
    if (wlh->xres <= 0)
	wlh->xres = wlh->xresmin;
    if (wlh->yres <= 0)
	wlh->yres = wlh->yresmin;
    init_xdg(wlh);

    init_keyboard(wlh);
    wlh_init_mouse(wlh);
    return 0;
}

void wlh_destroy(struct wayland_helper *wlh) {
    destroy_imagebuffer(wlh);
    if (buffer) {
	wl_buffer_destroy(buffer);
	buffer = NULL;
    }

    xdg_toplevel_destroy(xdgtop); xdgtop=NULL;
    xdg_surface_destroy(xdgsurf); xdgsurf=NULL;

    wl_surface_destroy(surface); surface=NULL;
    wl_callback_destroy(framecaller); framecaller=NULL;

    if (keyboard)
	destroy_keyboard(wlh);

    if (mouse)
	mouse = (wl_pointer_release(mouse), NULL);

    wl_seat_destroy(seat); seat=NULL;
    wl_shm_destroy(shared_memory); shared_memory=NULL;
    xdg_wm_base_destroy(xdg_base); xdg_base=NULL;
    wl_compositor_destroy(compositor); compositor=NULL;
    wl_registry_destroy(registry);
    wl_display_disconnect(wlh->display);
    wlh->stop = 1;
}

void wlh_fullscreen() {
    xdg_toplevel_set_fullscreen(xdgtop, NULL);
}

void wlh_nofullscreen() {
    xdg_toplevel_unset_fullscreen(xdgtop);
}

void wlh_commit(struct wayland_helper *wlh) {
    wl_surface_damage_buffer(surface, 0, 0, wlh->xres, wlh->yres);
    wl_surface_attach(surface, buffer, 0, 0); // This is always released automatically.
    wl_surface_commit(surface);
    wlh->redraw = wlh->can_redraw = 0;
}

#define modbits(a) (WLR_MODIFIER_##a * !!xkb_state_mod_name_is_active(wlh->xkbstate, XKB_MOD_NAME_##a, XKB_STATE_MODS_EFFECTIVE))
unsigned wlh_get_modstate(const struct wayland_helper *wlh) {
    return modbits(CTRL) | modbits(ALT) | modbits(SHIFT) | modbits(LOGO);
}
#undef modbits

long long wlh_timenow_µs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec*1000000 + tv.tv_usec;
}

int wlh_get_keysyms(const struct wayland_helper *wlh, const xkb_keysym_t** syms) {
    return xkb_state_key_get_syms(wlh->xkbstate, wlh->last_key, syms);
}

int wlh_key_should_repeat(struct wayland_helper *wlh) {
    if (!wlh->keydown)
	return 0;
    long long now = wlh_timenow_µs();
    int ret;
    if (wlh->last_repeat_µs)
	ret = now - wlh->last_repeat_µs >= wlh->repeat_interval_µs;
    else
	ret = now - wlh->last_keytime_µs >= wlh->repeat_delay_µs;
    if (ret) {
	wlh->last_repeat_µs = now;
	return 1;
    }
    return 0;
}

#ifdef wayland_test
static int pause_drawing = 0;

static void key_callback(struct wayland_helper *wlh) {
    if (!wlh->keydown)
	return;
    const xkb_keysym_t *syms;
    int nsyms = wlh_get_keysyms(wlh, &syms);
    for (int isym=0; isym<nsyms; isym++) {
	switch (syms[isym]) {
	    case XKB_KEY_q:
		wlh->stop = 1; break;
	    case XKB_KEY_space:
		pause_drawing = !pause_drawing; break;
	    default:
		break;
	}
    }
}

int main() {
    struct wayland_helper wlh = {
	.key_callback = key_callback,
    };
    if (wlh_init(&wlh))
	errx(1, "wlh_init_wayland");
    xdg_toplevel_set_title(xdgtop, "wltest");
    int number = 0;
    putchar('\n');
    int scale = 1;
    while (!wlh.stop && wl_display_roundtrip(wlh.display) > 0) {
#ifndef benchmark
	if (!wlh.can_redraw) {
	    usleep(10000);
	    continue;
	}
#endif
	if (pause_drawing) {
	    usleep(10000);
	    continue;
	}
	int maxx = wlh.xres/scale;
	int maxy = wlh.yres/scale;
	for (int j=0; j<maxy-scale; j++) {
	    uint32_t *ptr = wlh.data + j*scale*wlh.xres;
	    for (int i=0; i<=maxx-scale; i++) {
		uint32_t color =
		    (number*2 & 0xff) |				// red
		    ((j/2*(i/2) + number) & 0xff) << 8 |	// green
		    ((j/5*(i/4) + number*5) & 0xff) << 16 |	// blue
		    0xff << 24;					// alpha
		ptr[i*scale] = color;
		for (int sx=1; sx<scale; sx++)
		    ptr[i*scale+sx] = color;
	    }
	    for (int sy=1; sy<scale; sy++)
		memcpy(ptr+wlh.xres*sy, ptr, wlh.xres*4);
	}
	number++;
	printf("\033[A\r%i\n", number);
	wlh_commit(&wlh);
#ifndef benchmark
	usleep(10000);
#else
	if (number >= 1000)
	    break;
#endif
    }
    wlh_destroy(&wlh);
}
#endif
