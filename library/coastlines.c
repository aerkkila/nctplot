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

    double* points = malloc(coastl_total*sizeof(double)*2);

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
		conversion(x, y, points+point_ind*2);
		point_ind++;
	    }
	}
	obj = (SHPDestroyObject(obj), NULL);
    }

    if (conversion == coastl_proj_convert)
	coastl_proj_destroy();
    SHPClose(shp);
    return points;
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
    point->y = draw_h - g_pixels_per_datum - round((y - tmp_y0) * tmp_yspace);
}

static void draw_coastlines(plottable* plott) {
    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);
    int ind = 0;
    /* tmp_x0 is coordinate value, therefore offset is multiplied with coordinate interval, plott->xspace */
    tmp_x0 = plott->x0 + offset_i * plott->xspace;
    tmp_y0 = plott->y0 + offset_j * plott->yspace;
    tmp_xspace = 1 / plott->xspace / data_per_pixel;
    tmp_yspace = 1 / plott->yspace / data_per_pixel;
    double* points = plott->coasts;
    for(int e=0; e<coastl_nparts; e++) {
	int ipoint_to = 0;
	SDL_Point pnts[coastl_lengths[e]];
	/* These two if-else loops are the same except for the few last lines. */
	if (globs.invert_y)
	    for(int ipoint_from=0; ipoint_from<coastl_lengths[e]; ipoint_from++) {
		if (!valid_point(points + (ind+ipoint_from)*2)) {
		    if (ipoint_to > 1)
			SDL_RenderDrawLines(rend, pnts, ipoint_to);
		    ipoint_to = 0;
		    continue;
		}
		coord_to_point_inv_y(
			points[(ind+ipoint_from)*2],
			points[(ind+ipoint_from)*2+1],
			pnts+ipoint_to++);
	    }
	else
	    for(int ipoint_from=0; ipoint_from<coastl_lengths[e]; ipoint_from++) {
		if (!valid_point(points + (ind+ipoint_from)*2)) {
		    if (ipoint_to > 1)
			SDL_RenderDrawLines(rend, pnts, ipoint_to);
		    ipoint_to = 0;
		    continue;
		}
		coord_to_point(
			points[(ind+ipoint_from)*2],
			points[(ind+ipoint_from)*2+1],
			pnts+ipoint_to++);
	    }
	SDL_RenderDrawLines(rend, pnts, ipoint_to);
	ind += coastl_lengths[e];
    }
}

static void free_coastlines() {
    coastl_lengths = (free(coastl_lengths), NULL);
}
