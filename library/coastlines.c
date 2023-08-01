#include <shapefil.h>
#include <math.h>
#include "shpname.h"

#ifdef HAVE_PROJ
#include "coastlines_proj.c"
#endif

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

void no_conversion(float x, float y, double out[2]) {
    out[0] = x;
    out[1] = y;
}

/* Tämän kuuluu palauttaa rantaviivat halutuissa koordinaateissa.
   Muuntaminen pikseleiksi tehdään vasta piirtohetkellä.
   coordinates on proj-kirjastolle annettava tunniste, esim "+proj=laea, lat_0=90"
   vaihtoehtoisesti voidaan antaa oma muunnosfunktio latlon2other.
   Molempia ei ole hyödyllistä antaa. */
static double* make_coastlines(const char* coordinates, void (*conversion)(float,float,double[2])) {
    if (coordinates) {
	coastl_init_proj(coordinates);
	conversion = coastl_proj_convert;
    }
    else if (!conversion)
	conversion = no_conversion;

    /*
	if (init_default_conversion()) {
	    nct_puterror("Failed initializing default conversion.\n");
	    globs.coastlines = 0;
	    return NULL;
	}
	latlon2other = default_conversion;
    }
    lat0 = lat0_;
    lon0 = lon0_;
    */

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
		/* if (globs.invert_y)
		    points[point_ind].y = draw_h - points[point_ind].y;*/
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

static void init_coastlines(plottable* plott, void* funptr) {
    if (yid < 0)
	return;
    plott->coasts = make_coastlines(plott->crs, funptr);
    nct_var* var;
    var = nct_get_vardim(plott->var, xid);
    plott->x0 = nct_getg_floating(var, 0);
    plott->xspace = nct_getg_floating(var, 1) - plott->x0;
    var = nct_get_vardim(plott->var, yid);
    plott->y0 = nct_getg_floating(var, 0);
    plott->yspace = nct_getg_floating(var, 1) - plott->y0;
}

static double tmp_x0, tmp_y0, tmp_xspace, tmp_yspace;

static void coord_to_point(double x, double y, SDL_Point* point) {
    point->x = round((x - tmp_x0) * tmp_xspace);
    point->y = round((y - tmp_y0) * tmp_yspace);
}

static void draw_coastlines(plottable* plott) {
    SDL_SetRenderDrawColor(rend, globs.color_fg[0], globs.color_fg[1], globs.color_fg[2], 255);
    int ind = 0;
    tmp_x0 = plott->x0 + offset_i;
    tmp_y0 = plott->y0 + offset_j;
    tmp_xspace = 1 / plott->xspace / space;
    tmp_yspace = 1 / plott->yspace / space;
    double* points = plott->coasts;
    for(int e=0; e<coastl_nparts; e++) {
	SDL_Point pnts[coastl_lengths[e]];
	if (globs.invert_y)
	    for(int i=0; i<coastl_lengths[e]; i++) {
		coord_to_point(
			points[(ind+i)*2],
			points[(ind+i)*2+1],
			pnts+i);
		pnts[i].y = draw_h - pnts[i].y;
	    }
	else
	    for(int i=0; i<coastl_lengths[e]; i++)
		coord_to_point(
			points[(ind+i)*2],
			points[(ind+i)*2+1],
			pnts+i);
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
