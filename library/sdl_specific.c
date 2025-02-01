#include <SDL3/SDL.h>

#define bindings_file "bindings_sdl.h"

static SDL_Renderer* rend;
static SDL_Window* window;
static SDL_Texture* base = NULL;
static SDL_Event event;
static int call_resized;

static void init_graphics(int xlen, int ylen) {
	if (SDL_Init(SDL_INIT_VIDEO)) {
		nct_puterror("sdl_init: %s\n", SDL_GetError());
		return; }
	SDL_Event event;
	while (SDL_PollEvent(&event));

	const SDL_DisplayMode *dm;
	int ndisplays;
	SDL_DisplayID *displays = SDL_GetDisplays(&ndisplays);
	if (ndisplays <= 0 || !(dm = SDL_GetCurrentDisplayMode(displays[0]))) {
		nct_puterror("getting monitor size: %s\n", SDL_GetError());
		win_w = win_h = 500;
	} else {
		win_w = dm->w;
		win_h = dm->h;
		SDL_free(displays);
	}

	if (SDL_CreateWindowAndRenderer(
			MIN(xlen, win_w), MIN(ylen+cmapspace+cmappix, win_h),
			SDL_WINDOW_RESIZABLE, &window, &rend))
		errx(1, "create window and renderer: %s", SDL_GetError());
	SDL_SetWindowTitle(window, "nctplot");
	SDL_GetWindowSize(window, &win_w, &win_h);
	canvas = malloc(win_w * win_h * sizeof(canvas[0]));
}

static int get_modstate() {
	/* makes modstate side-insensitive and removes other modifiers than [alt,ctrl,gui,shift] */
	int mod = 0;
	int mod0 = SDL_GetModState();
	if (mod0 & SDL_KMOD_CTRL)
		mod |= SDL_KMOD_CTRL;
	if (mod0 & SDL_KMOD_SHIFT)
		mod |= SDL_KMOD_SHIFT;
	if (mod0 & SDL_KMOD_ALT)
		mod |= SDL_KMOD_ALT;
	if (mod0 & SDL_KMOD_GUI)
		mod |= SDL_KMOD_GUI;
	return mod;
}

static void quit_graphics() {
	if (base)
		SDL_DestroyTexture(base);
	base = NULL;
	SDL_DestroyRenderer(rend);
	SDL_DestroyWindow(window);
	free(canvas);
	canvas = NULL;
	SDL_Quit();
}

static void resized() {
	static uint_fast64_t lasttime;
	uint_fast64_t thistime = time_now_ms();
	if (thistime-lasttime < 16) {
		call_resized = 1;
		return;
	}
	call_resized = 0;
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	if (w == win_w && h == win_h)
		return;
	win_w = w; win_h = h;
	call_redraw = 1;
	lasttime = thistime;
	SDL_GetWindowSize(window, &win_w, &win_h);
	free(canvas);
	canvas = malloc(win_w * win_h * sizeof(canvas[0]));
	set_draw_params();
#ifdef HAVE_TTRA
	set_ttra();
#endif
}

static int mousex, mousey;

static void key_callback(SDL_Keycode sym, unsigned mod) {
	if (!typingmode || (get_modstate() & (SDL_KMOD_GUI | SDL_KMOD_ALT | SDL_KMOD_CTRL)))
		return keydown_func(event.key.keysym.sym, get_modstate());

	/* SDL_Textmode doesn't automatically turn backspaces into '\b' and so on */
	switch (sym) {
		case SDLK_RETURN:
		case SDLK_KP_ENTER:	typing_input("\r"); break;
		case SDLK_BACKSPACE:	typing_input("\b"); break;
		case SDLK_ESCAPE:	typing_input("\e"); break;
		default: break;
	}
}

static void mainloop() {
	int mouse_pressed=0;
	long play_start_ms = 0;
	int play_start_znum;
start:
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				quit((Arg){0});
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				call_resized = 1;
				break;
			case SDL_EVENT_KEY_DOWN:
				key_callback(event.key.keysym.sym, get_modstate()); break;
			case SDL_EVENT_TEXT_INPUT:
				if (!(get_modstate() & (SDL_KMOD_GUI | SDL_KMOD_ALT | SDL_KMOD_CTRL)))
					typing_input(event.text.text);
				break;
			case SDL_EVENT_MOUSE_MOTION:
				mousex = event.motion.x;
				mousey = event.motion.y;
				if (mouse_pressed) {
					if (prog_mode==mousepaint_m) {
						mousepaint();
						call_redraw = 1;
					}
					else mousemove(event.motion.xrel, event.motion.yrel);
				}
				else
					mousemotion(event.motion.xrel, event.motion.yrel);
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				mouse_pressed=1; break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				mouse_pressed=0; break;
			case SDL_EVENT_MOUSE_WHEEL:
				mousewheel(event.wheel.y);
		}
		if (stop) return;
	}

	if (stop)         return;
	if (call_resized) resized();
	if (zid < 0)      play_on = 0;
	if (play_on) {
		if (!play_start_ms) {
			play_start_ms = time_now_ms();
			play_start_znum = plt.area_z->znum;
		}
		else {
			double change_s = (time_now_ms() - play_start_ms) * 1e-3;
			int new_znum = play_start_znum + fps * change_s;
			if (new_znum != plt.area_z->znum)
				inc_znum((Arg){.i = new_znum - plt.area_z->znum});
		}
	}
	else
		play_start_ms = 0;

	if (call_redraw) {
		redraw(var);
		SDL_Surface *surface = SDL_CreateSurfaceFrom(
			canvas, win_w, win_h, win_w*sizeof(canvas[0]), SDL_PIXELFORMAT_ARGB8888);
		if (base)
			SDL_DestroyTexture(base);
		base = SDL_CreateTextureFromSurface(rend, surface);
		SDL_DestroySurface(surface);
	}
	SDL_RenderTexture(rend, base, NULL, NULL);
	SDL_RenderPresent(rend);
	SDL_Delay(sleeptime);
	goto start;
}
