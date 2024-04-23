#if defined HAVE_SHPLIB
#include <shapefil.h>
#include <math.h>
#include <stdlib.h>
#include "shpname.h"

#ifdef HAVE_PROJ
#include "proj.c"
#endif

static int* coastl_lengths = NULL;
static int coastl_nparts = 0, coastl_total = 0;

void no_conversion(void* _, float x, float y, double out[2]) {
    out[0] = x;
    out[1] = y;
}

/* This returns the coastlines in the desired coordinates.
   Conversion to pixel indices will be done later, just-in-time when drawing.
   Variable 'coordinates' is the string to pass to proj library, e.g. "+proj=laea lat_0=90".
   Alternatively, a custom conversion function, latlon2other, can be passed as an argument.
   It is not useful to give both arguments. */
static double* make_coastlines(const char* coordinates, void (*conversion)(void*,float,float,double[2])) {
    void *cookie = NULL;
    if (coordinates) {
	cookie = init_proj(coordinates, "+proj=lonlat");
	conversion = convert_proj;
    }
    else if (!conversion)
	conversion = no_conversion;

    SHPHandle shp = SHPOpen(shpname, "r");
    int nent, shptype;
    double padfmin[4], padfmax[4];
    SHPGetInfo(shp, &nent, &shptype, padfmin, padfmax);

    if (!coastl_total)
	for(int i=0; i<nent; i++) {
	    SHPObject* restrict obj = SHPReadObject(shp, i);
	    coastl_nparts += obj->nParts;
	    coastl_total += obj->nVertices;
	    SHPDestroyObject(obj);
	}
    if (!coastl_lengths)
	coastl_lengths = malloc(coastl_nparts*sizeof(int));

    double* coords = malloc(coastl_total*sizeof(double)*2);

    int point_ind = 0;
    int length_ind = 0;
    for(int i=0; i<nent; i++) {
	SHPObject* restrict obj = SHPReadObject(shp, i);
	for(int p=0; p<obj->nParts; p++) {
	    int end = p<obj->nParts-1 ? obj->panPartStart[p+1] : obj->nVertices;
	    int count = end - obj->panPartStart[p];
	    coastl_lengths[length_ind++] = count;
	    for(int v=obj->panPartStart[p], p=0; v<end; v++, p++) {
		float x = obj->padfX[v];
		float y = obj->padfY[v];
		conversion(cookie, x, y, coords+point_ind*2);
		point_ind++;
	    }
	}
	obj = (SHPDestroyObject(obj), NULL);
    }

    if (cookie)
	cookie = (destroy_proj(cookie), NULL);
    SHPClose(shp);
    return coords;
}

static int __attribute__((pure)) valid_point(const double point[2]) {
    return (isnormal(point[0]) || point[0]==0) && (isnormal(point[1]) || point[1]==0);
}

static int __attribute__((pure)) outside_window(const point_t *point) {
    return point->x < 0 || point->y < 0 || point->x >= win_w || point->y >= win_h;
}

static void init_coastlines(struct shown_area_xy* area, void* funptr) {
    if (yid < 0)
	return;
    area->coasts = make_coastlines(area->crs, funptr);
    nct_var* var1;
    var1 = nct_get_vardim(plt.var, xid);
    area->x0 = nct_getg_floating(var1, 0);
    area->xunits_per_datum = nct_getg_floating(var1, 1) - area->x0;
    area->x0 -= 0.5*area->xunits_per_datum;
    var1 = nct_get_vardim(plt.var, yid);
    area->y0 = nct_getg_floating(var1, 0);
    area->yunits_per_datum = nct_getg_floating(var1, 1) - area->y0;
    area->y0 -= 0.5*area->yunits_per_datum;
}

static double tmp_x0, tmp_y0, tmp_xpixels_per_unit, tmp_ypixels_per_unit;

static void coord_to_point(double x, double y, point_t* point) {
    point->x = round((x - tmp_x0) * tmp_xpixels_per_unit);
    point->y = round((y - tmp_y0) * tmp_ypixels_per_unit);
}

static void coord_to_point_inv_y(double x, double y, point_t* point) {
    point->x = round((x - tmp_x0) * tmp_xpixels_per_unit);
    point->y = draw_h - round((y - tmp_y0) * tmp_ypixels_per_unit);
}

static int line_break(int *breaks, int ibreak, int ipoint) {
    int last_break = ibreak>0 ? breaks[ibreak-1] : 0;
    if (ipoint-last_break >= 2)
	breaks[ibreak++] = ipoint;
    return ibreak;
}

static void make_coastlinepoints(struct shown_area_xy *area) {
    /* tmp_x0 is coordinate value, therefore offset is multiplied with coordinate interval, area->xunits_per_datum */
    tmp_x0 = area->x0 + area->offset_i * area->xunits_per_datum;
    tmp_y0 = area->y0 + area->offset_j * area->yunits_per_datum;
    tmp_xpixels_per_unit = 1 / area->xunits_per_datum / data_per_pixel[0];
    tmp_ypixels_per_unit = 1 / area->yunits_per_datum / data_per_pixel[1];
    double* coords = area->coasts;
    int* breaks = area->breaks;

    int ibreak = 0, ipoint = 0, ind_from = 0;

    point_t* points = area->points;
    void (*coord_to_point_fun)(double, double, point_t*) =
	globs.invert_y ? coord_to_point_inv_y : coord_to_point;

    for(int e=0; e<coastl_nparts; e++) {
	int irun = 0;
	for(int ipoint_from=0; ipoint_from<coastl_lengths[e]; ipoint_from++) {
	    if (!valid_point(coords + (ind_from+ipoint_from)*2))
		goto not_valid_point; // coordinates cannot be presented in the used projection
	    coord_to_point_fun(
		coords[(ind_from+ipoint_from)*2],
		coords[(ind_from+ipoint_from)*2+1],
		points + ipoint + irun);
	    if (outside_window(points + ipoint + irun)) // coordinates are not in the region
		goto not_valid_point;
	    if (irun && !memcmp(&points[ipoint+irun], &points[ipoint+irun-1], sizeof(points[0]))) // same as before
		continue;
	    irun++; // temporarily accept the point
	    continue;
not_valid_point:
	    ibreak = line_break(breaks, ibreak, ipoint+irun);
	    ipoint = ibreak ? breaks[ibreak-1] : 0;
	    irun = 0;
	}
	ibreak = line_break(breaks, ibreak, ipoint+irun);
	ipoint = ibreak ? breaks[ibreak-1] : 0;
	irun = 0;
	ind_from += coastl_lengths[e];
    }
    area->nbreaks = ibreak;
}

#define putval(buff, val) (buff += (memcpy(buff, &(val), sizeof(val)), sizeof(val)))
static void save_state(char* buff, const struct shown_area_xy *area) {
    putval(buff, area->offset_j);
    putval(buff, area->offset_i);
    putval(buff, data_per_pixel);
    putval(buff, globs.invert_y); // huomio: onko tämä aina globaali?
    putval(buff, win_w);
    putval(buff, win_h);
}
#undef putval

static void check_coastlines(struct shown_area_xy *area) {
    if (!area->points) {
	area->points = malloc(coastl_total * sizeof(point_t));
	area->breaks = malloc(coastl_total * sizeof(int));
	make_coastlinepoints(area);
	save_state(area->coastl_params, area);
    }
    else {
	char new_params[size_coastl_params];
	save_state(new_params, area);
	/* If current state differs from previous, save the new one and remake the coastlines. */
	if (memcmp(area->coastl_params, new_params, size_coastl_params)) {
	    memcpy(area->coastl_params, new_params, size_coastl_params);
	    make_coastlinepoints(area);
	}
    }
}

static void draw_coastlines(struct shown_area_xy *area) {
    check_coastlines(area); // must be before the assignment of the variables
    int nib = area->nbreaks;
    int* breaks = area->breaks;
    int istart = 0;
    set_color(globs.color_fg);
    point_t* points = area->points;
    for (int ib=0; ib<nib; ib++) {
	draw_lines(points+istart, breaks[ib]-istart);
	istart = breaks[ib];
    }
}

#undef size_params
#undef coastl_get_point

static void free_coastlines() {
    coastl_lengths = (free(coastl_lengths), NULL);
}

#else // not HAVE_SHPLIB
#define init_coastlines(...) // ignored
#define draw_coastlines(...) // ignored
#define free_coastlines(...) // ignored
#endif // HAVE_SHPLIB
