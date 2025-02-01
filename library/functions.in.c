/* This file defines those functions which must be repeated for each data type.
   They are further processed with Perl program make_functions.pl: in=functions.in.c, out=functions.c.

   No wrapper function is made if function name begins with underscore,
   or function returns ctype.
   */

/* The typedef is needed because the perl program can't recognize function pointers
   and doing a typedef is easier than fixing a perl program. */
typedef void cmapfun_t(unsigned char*, const void*);
#include "my_isnan.h"

@startperl // entry for the perl program

#define ctype @ctype
#define form @form
#define __nctype__ @nctype

static ctype* g_minmax_@nctype = (ctype*)g_minmax;

/* max-min can be larger than a signed number can handle.
   Therefore we cast to the corresponding unsigned type. */
#define CVAL(val,minmax) ((val) <  (minmax)[0] ? 0   :			\
	(val) >= (minmax)[1] ? 255 :			\
	(@uctype)((val)-(minmax)[0])*255 / (@uctype)((minmax)[1]-(minmax)[0]) )

static int draw_row_threshold_@nctype(uint32_t *canvas, int jpixel, int istart, int iend, const void* vdataptr, double dthr) {
	const ctype thr = dthr;
	const int cvals[] = {255*1/10, 255*9/10, 255*1/10};
	int count = 0;

	float idata_f = 0;
	int wdatum = g_pixels_per_datum[0], ipixel = istart;
	for (; ipixel<=iend-wdatum; idata_f+=g_data_per_step[0]) {
loop:
		long ind = round(idata_f);
		ctype val = ((const ctype*)vdataptr)[ind];
		if (my_isnan(val) || (shared.usenan && val==shared.nanval))
			continue;
		count += val >= thr;
		int value = cvals[(val >= thr) + shared.invert_c];
		uint32_t color = color_ptr_to_number(cmh_colorvalue(shared.cmapnum, value));
		for (int ii=0; ii<wdatum; ii++)
			canvas[jpixel*win_w + ipixel++] = color;
	}
	if ((wdatum = iend - ipixel) > 0)
		goto loop; // draw a partial wide pixel
	return count;
}

static void draw_row_@nctype(uint32_t *canvas, int jpixel, int istart, int iend, const void* vdataptr) {
	float idata_f = 0;
	int wdatum = g_pixels_per_datum[0], ipixel = istart;
	for (; ipixel<=iend-wdatum; idata_f+=g_data_per_step[0]) {
loop:
		long ind = round(idata_f);
		ctype val = ((const ctype*)vdataptr)[ind];
		if (my_isnan(val) || (shared.usenan && val==shared.nanval))
			continue;
		int value = CVAL(val, g_minmax_@nctype);
		if (shared.invert_c) value = 0xff-value;
		uint32_t color = color_ptr_to_number(cmh_colorvalue(shared.cmapnum,value));
		for (int ii=0; ii<wdatum; ii++)
			canvas[jpixel*win_w + ipixel++] = color;
	}
	if ((wdatum = iend - ipixel) > 0)
		goto loop; // draw a partial wide pixel
}

static void draw_row_cmapfun_@nctype(uint32_t *canvas, int jpixel, int istart, int iend, const void* vdataptr, cmapfun_t cmapfun) {
	float idata_f = 0;
	int wdatum = g_pixels_per_datum[0], ipixel = istart;
	for (; ipixel<=iend-wdatum; idata_f+=g_data_per_step[0]) {
loop:
		long ind = round(idata_f);
		const ctype *val = (const ctype*)vdataptr+ind;
		if (my_isnan(*val) || (shared.usenan && *val==shared.nanval))
			continue;
		unsigned char c[4];
		cmapfun(c, val);
		uint32_t color = color_ptr_to_number(c);
		for (int ii=0; ii<wdatum; ii++)
			canvas[jpixel*win_w + ipixel++] = color;
	}
	if ((wdatum = iend - ipixel) > 0)
		goto loop; // draw a partial wide pixel
}
#undef CVAL

static int make_minmax_@nctype() {
	@uctype range;
	memcpy(g_minmax_@nctype, plt.minmax, 2*sizeof(ctype));
	range = g_minmax_@nctype[1] - g_minmax_@nctype[0];
	if (minshift_abs != 0) {
		plt.minshift += minshift_abs/range;
		minshift_abs = 0;
	}
	if (maxshift_abs != 0) {
		plt.maxshift += maxshift_abs/range;
		maxshift_abs = 0;
	}
	g_minmax_@nctype[0] += (@uctype)(range*plt.minshift);
	g_minmax_@nctype[1] += (@uctype)(range*plt.maxshift);

	return g_minmax_@nctype[0] == g_minmax_@nctype[1];
}

static double get_min_@nctype() {
	return g_minmax_@nctype[0];
}

static double get_max_@nctype() {
	return g_minmax_@nctype[1];
}

static void minmax_at_@nctype(long start, long end, void *buff) {
	size_t length;
	ctype *data = get_data(start, &length), min, max;

	long ind = 0;
#if __nctype__ == NC_FLOAT || __nctype__ == NC_DOUBLE
	while (1) {
		ind = 0;
		long len = Min(length, end-start);
		for (; ind<len && my_isnan(data[ind]); ind++);
		if ((start += ind) >= end) {
			min = max = (ctype)0/0.0;
			goto Ret;
		}
		if (ind < len)
			break; // start found
		data = get_data(start, &length);
	}
#endif

	max = data[ind];
	min = data[ind];
	start = ind+1;
	while (start < end) {
		data = get_data(start, &length);
		long len = Min(length, end-start);
		for (long i=0; i<len; i++)
			if (my_isnan(data[i]));
			else if (data[i] < min)
				min = data[i];
			else if (data[i] > max)
				max = data[i];
		start += len;
	}

Ret: __attribute__((unused));
	 ((ctype*)buff)[0] = min;
	 ((ctype*)buff)[1] = max;
}

void minmax_nan_at_@nctype(long nanval_long, long start, long end, void* buff) {
	size_t length;
	ctype *data = get_data(start, &length);
	ctype nanval = nanval_long, min, max;

#define _my_isnan(a) (my_isnan(a) || (a) == nanval)
	long ind = 0;
	while (1) {
		ind = 0;
		long len = Min(length, end-start);
		for (; ind<len && _my_isnan(data[ind]); ind++);
		if ((start += ind) >= end) {
			min = max = nanval;
			goto Ret;
		}
		if (ind < len)
			break; // start found
		data = get_data(start, &length);
	}

	max = data[ind];
	min = data[ind];
	start = ind+1;
	while (start < end) {
		data = get_data(start, &length);
		long len = Min(length, end-start);
		for (long i=0; i<len; i++)
			if (_my_isnan(data[i]));
			else if (data[i] < min)
				min = data[i];
			else if (data[i] > max)
				max = data[i];
		start += len;
	}

Ret:
	((ctype*)buff)[0] = min;
	((ctype*)buff)[1] = max;
#undef _my_isnan
}

#undef ctype
#undef form
#undef __nctype__
