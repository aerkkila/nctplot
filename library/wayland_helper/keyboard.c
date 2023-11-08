#include <sys/time.h>
#include <xkbcommon/xkbcommon.h>

static struct wl_keyboard* keyboard;
static struct xkb_context* xkbcontext;

static void kb_modifiers_callback(void* data, struct wl_keyboard* wlkb, uint32_t serial, uint32_t mods_depr,
			       uint32_t mods_latch, uint32_t mods_lock, uint32_t group) {
    xkb_state_update_mask(((struct wayland_helper*)data)->xkbstate, mods_depr, mods_latch, mods_lock, 0, 0, group);
}

static void kb_keymap_callback(void* data, struct wl_keyboard* wlkb, uint32_t form, int32_t fd, uint32_t size) {
    char* map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if(!map_shm)
	warn("mmap keymap %s: %i",__FILE__,  __LINE__);
    //printf("map_shm:\n%*s\n", size, map_shm);
    struct xkb_keymap* keymap = xkb_keymap_new_from_string(xkbcontext, map_shm,
	XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    ((struct wayland_helper*)data)->xkbstate = xkb_state_new(keymap);
    xkb_keymap_unref(keymap);
    munmap(map_shm, size);
}

static void kb_repeat_info_callback(void* data, struct wl_keyboard* wlkb, int32_t interval, int32_t delay) {
    struct wayland_helper *wlh = data;
    wlh->repeat_interval_µs = interval * 1000;
    wlh->repeat_delay_µs = delay * 1000;
}

static void nop() {}

static void kb_key_callback(
    void* vdata, struct wl_keyboard* kb, uint32_t serial,
    uint32_t time, uint32_t key, uint32_t state) {
    struct wayland_helper *wlh = vdata;
    wlh->keydown = state;
    wlh->last_key = key + 8;
    wlh->last_keymods = wlh_get_modstate(wlh);
    wlh->last_keytime_µs = wlh_timenow_µs();
    wlh->last_repeat_µs = 0;
    wlh->key_callback(wlh);
}

static void kb_leave_callback(
	void *data,
	struct wl_keyboard *wl_keyboard,
	uint32_t serial,
	struct wl_surface *surface) {
    struct wayland_helper *wlh = data;
    wlh->keydown = 0;
}

static struct wl_keyboard_listener keyboardlistener = {
    .keymap = kb_keymap_callback,
    .enter = nop, // Should we read the already pressed modifiers?
    .leave = kb_leave_callback,
    .key = kb_key_callback, // see init_keyboard: user provides the function
    .modifiers = kb_modifiers_callback,
    .repeat_info = kb_repeat_info_callback,
};

static void init_keyboard(struct wayland_helper *wlh) {
    xkbcontext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(keyboard, &keyboardlistener, wlh);
    if (!wlh->key_callback)
	wlh->key_callback = nop;
}

static void destroy_keyboard(struct wayland_helper *wlh) {
    wl_keyboard_release(keyboard); keyboard=NULL;
    xkb_context_unref(xkbcontext); xkbcontext=NULL;
    xkb_state_unref(wlh->xkbstate); wlh->xkbstate=NULL;
}
