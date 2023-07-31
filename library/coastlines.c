#include <shapefil.h>
#include <math.h>
#include "shpname.h"

static float lat0_, lon0_, latspace, lonspace, lat0, lon0;
static int* coastl_lengths = NULL;
static int coastl_nparts = 0, coastl_total = 0;

static int init_default_conversion() {
    nct_var* v;

    if (!(v=nct_get_var(var->super, "lat")))
	if(!(v=nct_get_var(var->super, "latitude")))
	    return 1;
    double (*getfunk)(const nct_var*, size_t) = v->data ? nct_get_floating : nct_getl_floating;
    lat0_ = getfunk(v, 0);
    latspace = 1 / (getfunk(v, 1) - lat0_) / space;

    if (!(v=nct_get_var(var->super, "lon")))
	if(!(v=nct_get_var(var->super, "longitude")))
	    return 2;
    getfunk = v->data ? nct_get_floating : nct_getl_floating;
    lon0_ = getfunk(v, 0);
    lonspace = 1 / (getfunk(v, 1) - lon0_) / space;

    return 0;
}

static SDL_Point default_conversion(float lat, float lon) {
    return (SDL_Point) {
	.x = round((lon-lon0) * lonspace),
	.y = round((lat-lat0) * latspace),
    };
}

static SDL_Point* init_coastlines(SDL_Point (*latlon2point)(float,float)) {
    if (!latlon2point) {
	if (init_default_conversion()) {
	    nct_puterror("Failed initializing default conversion.\n");
	    globs.coastlines = 0;
	    return NULL;
	}
	latlon2point = default_conversion;
    }

    lat0 = lat0_;
    lon0 = lon0_;

    SHPHandle shp = SHPOpen(shpname, "r");
    int nent, shptype;
    double padfmin[4], padfmax[4];
    SHPGetInfo(shp, &nent, &shptype, padfmin, padfmax);

    if (!coastl_total) {
	for(int i=0; i<nent; i++) {
	    SHPObject* restrict obj = SHPReadObject(shp, i);
	    coastl_nparts += obj->nParts;
	    coastl_total += obj->nVertices;
	    SHPDestroyObject(obj);
	}
	coastl_lengths = malloc(coastl_nparts*sizeof(int));
    }
    SDL_Point* points = malloc(coastl_total*sizeof(SDL_Point));

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
		points[point_ind] = latlon2point(y,x);
		if (globs.invert_y)
		    points[point_ind].y = draw_h - points[point_ind].y;
		point_ind++;
	    }
	}
	obj = (SHPDestroyObject(obj), NULL);
    }

    SHPClose(shp);
    return points;
}

static void draw_coastlines(SDL_Point* points) {
    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);
    int ind = 0;
    for(int e=0; e<coastl_nparts; e++) {
	SDL_Point pnts[coastl_lengths[e]];
	for(int i=0; i<coastl_lengths[e]; i++) {
	    pnts[i] = points[ind+i];
	    pnts[i].x -= offset_i;
	    pnts[i].y -= offset_j;
	}
	//SDL_RenderDrawLines(rend, points+ind, coastl_lengths[e]),
	SDL_RenderDrawLines(rend, pnts, coastl_lengths[e]);
	ind += coastl_lengths[e];
    }
}

static void free_coastlines() {
    coastl_lengths = (free(coastl_lengths), NULL);
}

static void vanha_coastlines(SDL_Point (*latlon2point)(float,float)) {
    if (!latlon2point) {
	if (init_default_conversion()) {
	    nct_puterror("Failed initializing default conversion.\n");
	    globs.coastlines = 0;
	    return;
	}
	latlon2point = default_conversion;
    }

    lat0 = lat0_ + offset_j;
    lon0 = lon0_ + offset_i;

    SHPHandle shp = SHPOpen(shpname, "r");
    int nent, shptype;
    double padfmin[4], padfmax[4];
    SHPGetInfo(shp, &nent, &shptype, padfmin, padfmax);

    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);

    for(int i=0; i<nent; i++) {
	SHPObject* restrict obj = SHPReadObject(shp, i);
	for(int p=0; p<obj->nParts; p++) {
	    int end = p<obj->nParts-1 ? obj->panPartStart[p+1] : obj->nVertices;
	    int count = end - obj->panPartStart[p];
	    SDL_Point points[count];
	    for(int v=obj->panPartStart[p], p=0; v<end; v++, p++) {
		float x = obj->padfX[v];
		float y = obj->padfY[v];
		points[p] = latlon2point(y,x);
		if (globs.invert_y)
		    points[p].y = draw_h - points[p].y;
	    }
	    SDL_RenderDrawLines(rend, points, count);
	}
	obj = (SHPDestroyObject(obj), NULL);
    }

    SHPClose(shp);
}
