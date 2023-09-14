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

static void init_coastlines(plottable* plott, void* funptr) {
    if (yid < 0)
	return;
    plott->coasts = make_coastlines(plott->crs, funptr);
    nct_var* var;
    var = nct_get_vardim(plott->var, xid);
    plott->x0 = nct_getg_floating(var, 0);
    plott->xspace = nct_getg_floating(var, 1) - plott->x0;
    plott->x0 -= 0.5*plott->xspace;
    var = nct_get_vardim(plott->var, yid);
    plott->y0 = nct_getg_floating(var, 0);
    plott->yspace = nct_getg_floating(var, 1) - plott->y0;
    plott->y0 -= 0.5*plott->yspace;
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

static void make_coastlinepoints(plottable* plott) {
    /* tmp_x0 is coordinate value, therefore offset is multiplied with coordinate interval, plott->xspace */
    tmp_x0 = plott->x0 + offset_i * plott->xspace;
    tmp_y0 = plott->y0 + offset_j * plott->yspace;
    tmp_xspace = 1 / plott->xspace / data_per_pixel;
    tmp_yspace = 1 / plott->yspace / data_per_pixel;
    double* coords = plott->coasts;
    SDL_Point* points = plott->points;
    int* breaks = plott->breaks;

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
    plott->nbreaks = ibreak;
}

#define putval(buff, val) (buff += (memcpy(buff, &(val), sizeof(val)), sizeof(val)))
static void save_state(char* buff) {
    putval(buff, offset_j);
    putval(buff, offset_i);
    putval(buff, data_per_pixel);
    putval(buff, pltind);
    putval(buff, globs.invert_y);
}
#undef putval

#define size_params (sizeof(offset_j)*2 + sizeof(data_per_pixel) + sizeof(pltind) + sizeof(globs.invert_y))

static void check_coastlines(plottable* plott) {
    static char old_params[size_params];
    if (!plott->points) {
	plott->points = malloc(coastl_total * sizeof(SDL_Point));
	plott->breaks = malloc(coastl_total * sizeof(int));
	make_coastlinepoints(plott);
	save_state(old_params);
    }
    else {
	char new_params[size_params];
	save_state(new_params);
	if (memcmp(old_params, new_params, size_params)) {
	    memcpy(old_params, new_params, size_params);
	    make_coastlinepoints(plott);
	}
    }
}

static void draw_coastlines(plottable* plott) {
    check_coastlines(plott);
    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);
    int nib = plott->nbreaks;
    SDL_Point* points = plott->points;
    int* breaks = plott->breaks;
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
