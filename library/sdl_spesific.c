#include <SDL2/SDL.h>
#include "bindings.h"

#define bindings_file "bindings.h"

static SDL_Renderer* rend;
static SDL_Window* window;
static SDL_Texture* base;
static SDL_Event event;

#define graphics_draw_point(i,j) SDL_RenderDrawPoint(rend, i, j)

static inline void set_color(unsigned char* c) {
    SDL_SetRenderDrawColor(rend, c[0], c[1], c[2], 255);
}

static inline void clear_background() {
    SDL_RenderClear(rend);
}

static inline void set_scale(int scalex, int scaley) {
    SDL_RenderSetScale(rend, scalex, scaley);
}

static void init_graphics(int xlen_in, int ylen_in) {
    if (SDL_Init(SDL_INIT_VIDEO)) {
	nct_puterror("sdl_init: %s\n", SDL_GetError());
	return; }
    SDL_Event event;
    while(SDL_PollEvent(&event));
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm)) {
	nct_puterror("getting monitor size: %s\n", SDL_GetError());
	win_w = win_h = 500;
    } else {
	win_w = dm.w;
	win_h = dm.h;
    }

    window = SDL_CreateWindow("nctplot", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	MIN(xlen, win_w), MIN(ylen+cmapspace+cmappix, win_h), SDL_WINDOW_RESIZABLE);
    rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);
    SDL_GetWindowSize(window, &win_w, &win_h);
    base = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, win_w, win_h);
}

static int get_modstate() {
    /* makes modstate side-insensitive and removes other modifiers than [alt,ctrl,gui,shift] */
    int mod = 0;
    int mod0 = SDL_GetModState();
    if(mod0 & KMOD_CTRL)
	mod |= KMOD_CTRL;
    if(mod0 & KMOD_SHIFT)
	mod |= KMOD_SHIFT;
    if(mod0 & KMOD_ALT)
	mod |= KMOD_ALT;
    if(mod0 & KMOD_GUI)
	mod |= KMOD_GUI;
    return mod;
}

static void quit_graphics() {
    SDL_DestroyTexture(base);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static void resized() {
    static uint_fast64_t lasttime;
    uint_fast64_t thistime = time_now_ms();
    if(thistime-lasttime < 16) {
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
    SDL_DestroyTexture(base);
    SDL_GetWindowSize(window, &win_w, &win_h);
    base = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, win_w, win_h);
    set_draw_params();
}

static void mainloop() {
    int mouse_pressed=0;
start:
    while (SDL_PollEvent(&event)) {
	switch(event.type) {
	case SDL_QUIT:
	    quit((Arg){0}); break;
	case SDL_WINDOWEVENT:
	    if(event.window.event==SDL_WINDOWEVENT_RESIZED)
		call_resized = 1;
	    break;
	case SDL_KEYDOWN:
	    keydown_func(event.key.keysym.sym, get_modstate()); break;
	case SDL_MOUSEMOTION:
	    mousex = event.motion.x;
	    mousey = event.motion.y;
	    if(mouse_pressed) {
		if (prog_mode==mousepaint_m) {
		    mousepaint();
		    call_redraw = 1;
		}
		else mousemove();
	    }
	    else
		mousemotion(event.motion.xrel, event.motion.yrel);
	    break;
	case SDL_MOUSEBUTTONDOWN:
	    mouse_pressed=1; break;
	case SDL_MOUSEBUTTONUP:
	    mouse_pressed=0; break;
	case SDL_MOUSEWHEEL:
	    mousewheel(event.wheel.y);
	}
	if(stop) return;
    }

    if (stop)		return;
    if (call_resized)	resized();
    if (call_redraw)	redraw(var);
    if (zid < 0)	play_inv = play_on = 0;
    if (play_inv)	{inc_znum((Arg){.i=-1}); play_on=0;}
    if (play_on)	inc_znum((Arg){.i=1});

    SDL_RenderCopy(rend, base, NULL, NULL);
    SDL_RenderPresent(rend);
    SDL_Delay(sleeptime);
    goto start;
}
