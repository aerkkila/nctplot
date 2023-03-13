#include <shapefil.h>
#include <math.h>
#include "shpname.h"

static float lat0, lon0, latspace, lonspace;

static int init_default_conversion() {
    nct_var* v;

    if (!(v=nct_get_var(var->super, "lat")))
	if(!(v=nct_get_var(var->super, "latitude")))
	    return 1;
    if (!v->data) {
	nct_load(v);
	if (!v->data)
	    return 1;
    }
    lat0 = nct_get_floating(v, 0);
    latspace = 1 / (nct_get_floating(v, 1) - lat0) / space;

    if (!(v=nct_get_var(var->super, "lon")))
	if(!(v=nct_get_var(var->super, "longitude")))
	    return 2;
    if (!v->data) {
	nct_load(v);
	if (!v->data)
	    return 2;
    }
    lon0 = nct_get_floating(v, 0);
    lonspace = 1 / (nct_get_floating(v, 1) - lon0) / space;

    return 0;
}

static SDL_Point default_conversion(float lat, float lon) {
    return (SDL_Point) {
	.x = round((lon-lon0) * lonspace),
	.y = round((lat-lat0) * latspace),
    };
}

static void coastlines(SDL_Point (*latlon2point)(float,float)) {
    if (!latlon2point) {
	if (init_default_conversion()) {
	    nct_puterror("Failed initializing default conversion.");
	    return;
	}
	latlon2point = default_conversion;
    }

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
