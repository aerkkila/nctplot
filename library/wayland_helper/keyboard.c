#include <sys/time.h>
#include <xkbcommon/xkbcommon.h>

static struct wl_keyboard* keyboard;
static struct xkb_context* xkbcontext;
static struct xkb_state* xkbstate;
static long long repeat_interval_µs, repeat_delay_µs, when_pressed_µs;
char pressed, repeat_started;

static long long timenow_µs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec*1000000 + tv.tv_usec;
}

static void kb_modifiers_kutsu(void* data, struct wl_keyboard* wlkb, uint32_t serial, uint32_t mods_depr,
			       uint32_t mods_latch, uint32_t mods_lock, uint32_t group) {
    xkb_state_update_mask(xkbstate, mods_depr, mods_latch, mods_lock, 0, 0, group);
}

static void kb_keymap_callback(void* data, struct wl_keyboard* wlkb, uint32_t form, int32_t fd, uint32_t size) {
    char* map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if(!map_shm)
	warn("mmap keymap %s: %i",__FILE__,  __LINE__);
    //printf("map_shm:\n%*s\n", size, map_shm);
    struct xkb_keymap* keymap = xkb_keymap_new_from_string(xkbcontext, map_shm,
	XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    xkbstate = xkb_state_new(keymap);
    xkb_keymap_unref(keymap);
    munmap(map_shm, size);
}

static void kb_repeat_info_callback(void* data, struct wl_keyboard* wlkb, int32_t interval, int32_t delay) {
    repeat_interval_µs = interval * 1000;
    repeat_delay_µs = delay * 1000;
}

static void nop() {}

static struct wl_keyboard_listener keyboardlistener = {
    .keymap = kb_keymap_callback,
    .enter = nop, // Should we read the already pressed modifiers?
    .leave = nop,
    .key = NULL, // see init_keyboard: user provides the function
    .modifiers = kb_modifiers_kutsu,
    .repeat_info = kb_repeat_info_callback,
};

void init_keyboard(
    void (*callback_function)(
	void*, struct wl_keyboard*, uint32_t serial,
	uint32_t time, uint32_t key, uint32_t state
    ), struct wayland_helper *imdata) {
    xkbcontext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    keyboard = wl_seat_get_keyboard(seat);
    keyboardlistener.key = callback_function ? callback_function : nop;
    wl_keyboard_add_listener(keyboard, &keyboardlistener, imdata);
}

void destroy_keyboard() {
    wl_keyboard_release(keyboard); keyboard=NULL;
    xkb_context_unref(xkbcontext); xkbcontext=NULL;
    xkb_state_unref(xkbstate); xkbstate=NULL;
}

int repeat_key() {
    static long long last_repeat;
    long long now = timenow_µs();
    int ret =
	(!repeat_started && now - when_pressed_µs > repeat_delay_µs) ||
	(repeat_started  && now - last_repeat > repeat_interval_µs);
    if (ret) {
	last_repeat = now;
	repeat_started = 1;
    }
    return ret;
}
