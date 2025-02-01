#include <string.h>

#ifndef Abs
#define Abs(a) ((a)<0 ? -(a) : (a))
#endif

#ifdef HAVE_TTRA
static struct ttra ttra;
static int use_ttra = 1;
static int fontheight_ttra = 20;
static const int ttra_space = 8;
#define Printf(...) do { if (use_ttra) ttra_printf(&ttra, __VA_ARGS__); else printf(__VA_ARGS__); } while (0)
#define Nct_print_datum(dtype, datum) do { \
	if (use_ttra) nct_fprint_datum(dtype, (nct_fprint_t)ttra_printf, &ttra, datum); \
	else nct_print_datum(dtype, datum);    \
} while (0)
#define update_printarea() do { \
	if (use_ttra) {             \
		if (!call_redraw) call_redraw = commit_only_e; } \
	else fflush(stdout);        \
} while (0)
#endif

enum redraw_type {commit_only_e=2};

typedef struct {
	int x, y;
} point_t;

static inline uint32_t color_ptr_to_number(unsigned char* c) {
	return
		(0xff << 24) |
		(c[0] << 16 ) |
		(c[1] << 8 ) |
		(c[2] << 0);
}

static void clear_background(uint32_t color) {
	for (int i=0; i<win_h*win_w; i++)
		canvas[i] = color;
}

static void clear_unused_bottom(uint32_t color) {
	for (int j=total_height(); j<win_h; j++)
		for (int i=0; i<win_w; i++)
			canvas[j*win_w+i] = color;
}

static void expand_row_to_yscale(int scale, int jpixel, int istart, int iend) {
	for (int jj=1; jj<scale; jj++)
		memcpy(
			canvas + (jpixel+jj)*win_w + istart,
			canvas + jpixel*win_w + istart,
			(iend-istart) * sizeof(canvas[0]));
}

#define draw_line_$method draw_line_bresenham

/* https://en.wikipedia.org/wiki/Bresenham's_line_algorithm */
/* This method is nice because it uses only integers. */
static void draw_line_bresenham(uint32_t color, const int *xy) {
	int nosteep = Abs(xy[3] - xy[1]) < Abs(xy[2] - xy[0]);
	int backwards = xy[2+!nosteep] < xy[!nosteep]; // m1 < m0
	int m1=xy[2*!backwards+!nosteep], m0=xy[2*backwards+!nosteep],
	n1=xy[2*!backwards+nosteep],  n0=xy[2*backwards+nosteep];

	const int n_add = n1 > n0 ? 1 : -1;
	const int dm = m1 - m0;
	const int dn = n1 > n0 ? n1 - n0 : n0 - n1;
	const int D_add0 = 2 * dn;
	const int D_add1 = 2 * (dn - dm);
	int D = 2*dn - dm;
	if (nosteep) // (m,n) = (x,y)
		for (; m0<=m1; m0++) {
			canvas[n0*win_w + m0] = color;
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
	else // (m,n) = (y,x)
		for (; m0<=m1; m0++) {
			canvas[m0*win_w + n0] = color;
			n0 += D > 0 ? n_add : 0;
			D  += D > 0 ? D_add1 : D_add0;
		}
}

static void draw_lines(uint32_t color, const void *v, int n) {
	const point_t *p = v;
	for (int i=0; i<n-1; i++)
		draw_line_$method(color, (int*)(p+i));
}

#ifdef HAVE_TTRA
static void set_ttra() {
	ttra.canvas = canvas;
	ttra_set_xy0(&ttra, 0, draw_h + cmapspace + cmappix + ttra_space);
	ttra.w = win_w;
	ttra.h = win_h - ttra.y0;
	ttra.realh = win_h;
	ttra.realw = win_w;
}
#endif

