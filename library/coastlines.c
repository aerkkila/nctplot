#ifdef HAVE_SHPLIB // wraps the whole file
#include <shapefil.h>
#include <math.h>
#include "shpname.h"

#ifdef HAVE_PROJ
#include "coastlines_proj.c"
#endif

static int* coastl_lengths = NULL;
static int coastl_nparts = 0, coastl_total = 0;

void no_conversion(float x, float y, double out[2]) {
    out[0] = x;
    out[1] = y;
}

/* This returns the coastlines in the desired coordinates.
   Conversion to pixel indices will be done later, just-in-time when drawing.
   Variable 'coordinates' is the string to pass to proj library, e.g. "+proj=laea lat_0=90".
   Alternatively, a custom conversion function, latlon2other, can be passed as an argument.
   It is not useful to give both arguments. */
static double* make_coastlines(const char* coordinates, void (*conversion)(float,float,double[2])) {
    if (coordinates) {
	coastl_init_proj(coordinates);
	conversion = coastl_proj_convert;
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
		conversion(x, y, coords+point_ind*2);
		point_ind++;
	    }
	}
	obj = (SHPDestroyObject(obj), NULL);
    }

    if (conversion == coastl_proj_convert)
	coastl_proj_destroy();
    SHPClose(shp);
    return coords;
}

static int valid_point(double point[2]) {
    return (isnormal(point[0]) || point[0]==0) && (isnormal(point[1]) || point[1]==0);
}

static void init_coastlines(struct shown_area* area, void* funptr) {
    if (yid < 0)
	return;
    area->coasts = make_coastlines(area->crs, funptr);
    nct_var* var1;
    var1 = nct_get_vardim(plt.var, xid);
    area->x0 = nct_getg_floating(var1, 0);
    area->xspace = nct_getg_floating(var1, 1) - area->x0;
    area->x0 -= 0.5*area->xspace;
    var1 = nct_get_vardim(plt.var, yid);
    area->y0 = nct_getg_floating(var1, 0);
    area->yspace = nct_getg_floating(var1, 1) - area->y0;
    area->y0 -= 0.5*area->yspace;
}

static double tmp_x0, tmp_y0, tmp_xspace, tmp_yspace;

static void coord_to_point(double x, double y, SDL_Point* point) {
    point->x = round((x - tmp_x0) * tmp_xspace);
    point->y = round((y - tmp_y0) * tmp_yspace);
}

static void coord_to_point_inv_y(double x, double y, SDL_Point* point) {
    point->x = round((x - tmp_x0) * tmp_xspace);
    point->y = draw_h - round((y - tmp_y0) * tmp_yspace);
}

static void make_coastlinepoints(struct shown_area *area) {
    /* tmp_x0 is coordinate value, therefore offset is multiplied with coordinate interval, area->xspace */
    tmp_x0 = area->x0 + area->offset_i * area->xspace;
    tmp_y0 = area->y0 + area->offset_j * area->yspace;
    tmp_xspace = 1 / area->xspace / data_per_pixel;
    tmp_yspace = 1 / area->yspace / data_per_pixel;
    double* coords = area->coasts;
    SDL_Point* points = area->points;
    int* breaks = area->breaks;

    int ibreak = 0, ipoint = 0, ind_from = 0;

    void (*coord_to_point_fun)(double, double, SDL_Point*) = 
	globs.invert_y ? coord_to_point_inv_y : coord_to_point;

    for(int e=0; e<coastl_nparts; e++) {
	for(int ipoint_from=0; ipoint_from<coastl_lengths[e]; ipoint_from++) {
	    if (!valid_point(coords + (ind_from+ipoint_from)*2)) {
		if (ipoint-breaks[ibreak-1] >= 2)
		    breaks[ibreak++] = ipoint;
		else
		    ipoint = breaks[ibreak-1]; // single point is omitted
		continue;
	    }
	    coord_to_point_fun(
		    coords[(ind_from+ipoint_from)*2],
		    coords[(ind_from+ipoint_from)*2+1],
		    points+ipoint++);
	}
	breaks[ibreak++] = ipoint;
	ind_from += coastl_lengths[e];
    }

    if (ipoint-breaks[ibreak-1] >= 2)
	breaks[ibreak++] = ipoint;
    area->nbreaks = ibreak;
}

#define putval(buff, val) (buff += (memcpy(buff, &(val), sizeof(val)), sizeof(val)))
static void save_state(char* buff, const struct shown_area *area) {
    putval(buff, area->offset_j);
    putval(buff, area->offset_i);
    putval(buff, data_per_pixel);
    putval(buff, globs.invert_y); // huomio: onko tämä aina globaali?
}
#undef putval

static void check_coastlines(struct shown_area *area) {
    if (!area->points) {
	area->points = malloc(coastl_total * sizeof(SDL_Point));
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

static void draw_coastlines(struct shown_area *area) {
    check_coastlines(area);
    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);
    int nib = area->nbreaks;
    SDL_Point* points = area->points;
    int* breaks = area->breaks;
    int istart = 0;
    for (int ib=0; ib<nib; ib++) {
	SDL_RenderDrawLines(rend, points+istart, breaks[ib]-istart);
	istart = breaks[ib];
    }
}

#undef size_params

static void free_coastlines() {
    coastl_lengths = (free(coastl_lengths), NULL);
}
#endif // HAVE_SHPLIB
