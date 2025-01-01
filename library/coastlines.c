#if defined HAVE_SHPLIB
#include <shapefil.h>
#include <math.h>
#include <stdlib.h>
#include "config.h" // sharedir

#ifdef HAVE_PROJ
#include "proj.c"
#endif

#define arrlen(a) (sizeof(a)/sizeof((a)[0]))

static const char *features_dirnames[] = {
	[0]="ne_10m_coastline",
};
static const char *features_shown_names[] = {
	[0]="coastlines, ne, 10m",
};
enum features_e {coastlines_e};

void no_conversion(void* _, float x, float y, double out[2]) {
	out[0] = x;
	out[1] = y;
}

/* This returns the feature in the desired coordinates.
   Conversion to pixel indices will be done later, just-in-time when drawing.
   Variable 'coordinates' is the string to pass to proj library, e.g. "+proj=laea lat_0=90".
   Alternatively, a custom conversion function, latlon2other, can be passed as an argument.
   It is not useful to give both arguments. */
static double* make_feature(struct feature *feature, const char *shpname, const char* coordinates, void (*conversion)(void*,float,float,double[2])) {
	void *cookie = NULL;
	if (coordinates) {
		cookie = init_proj("+proj=lonlat", coordinates);
		conversion = convert_proj;
	}
	else if (!conversion)
		conversion = no_conversion;

	SHPHandle shp = SHPOpen(shpname, "r");
	int nent, shptype;
	double padfmin[4], padfmax[4];
	SHPGetInfo(shp, &nent, &shptype, padfmin, padfmax);

	if (!feature->total)
		for (int i=0; i<nent; i++) {
			SHPObject* restrict obj = SHPReadObject(shp, i);
			feature->nparts += obj->nParts;
			feature->total += obj->nVertices;
			SHPDestroyObject(obj);
		}
	if (!feature->lengths)
		feature->lengths = malloc(feature->nparts*sizeof(int));

	double* coords = malloc(feature->total*sizeof(double)*2);

	int point_ind = 0;
	int length_ind = 0;
	for (int i=0; i<nent; i++) {
		SHPObject* restrict obj = SHPReadObject(shp, i);
		for (int p=0; p<obj->nParts; p++) {
			int end = p<obj->nParts-1 ? obj->panPartStart[p+1] : obj->nVertices;
			int count = end - obj->panPartStart[p];
			feature->lengths[length_ind++] = count;
			for (int v=obj->panPartStart[p], p=0; v<end; v++, p++) {
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

static struct feature __attribute__((malloc))* init_feature(int ifeature, void* funptr, char *crs) {
	if (yid < 0)
		return NULL;
	struct feature *feature = calloc(1, sizeof(struct feature));
	feature->nusers = 1;
	char shpname[1024];
	snprintf(shpname, sizeof(shpname), "%s/%s/%s.nc", sharedir, features_dirnames[ifeature], features_dirnames[ifeature]);
	feature->coords = make_feature(feature, shpname, crs, funptr);
	free(feature->points); feature->points = NULL;
	nct_var* var1;
	var1 = nct_get_vardim(plt.var, xid);
	feature->x0 = nct_getg_floating(var1, 0);
	feature->xunits_per_datum = nct_getg_floating(var1, 1) - feature->x0;
	feature->x0 -= 0.5*feature->xunits_per_datum;
	var1 = nct_get_vardim(plt.var, yid);
	feature->y0 = nct_getg_floating(var1, 0);
	feature->yunits_per_datum = nct_getg_floating(var1, 1) - feature->y0;
	feature->y0 -= 0.5*feature->yunits_per_datum;
	return feature;
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

static void _make_featurepoints(int offset_i, int offset_j, struct feature *feature) {
	/* tmp_x0 is coordinate value, therefore offset is multiplied with coordinate interval, feature->xunits_per_datum */
	tmp_x0 = feature->x0 + offset_i * feature->xunits_per_datum;
	tmp_y0 = feature->y0 + offset_j * feature->yunits_per_datum;
	tmp_xpixels_per_unit = 1 / feature->xunits_per_datum / data_per_pixel[0];
	tmp_ypixels_per_unit = 1 / feature->yunits_per_datum / data_per_pixel[1];
	double* coords = feature->coords;
	int* breaks = feature->breaks;

	int ibreak = 0, ipoint = 0, ind_from = 0;

	point_t* points = feature->points;
	void (*coord_to_point_fun)(double, double, point_t*) =
		shared.invert_y ? coord_to_point_inv_y : coord_to_point;

	for (int e=0; e<feature->nparts; e++) {
		int irun = 0;
		for (int ipoint_from=0; ipoint_from<feature->lengths[e]; ipoint_from++) {
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
		ind_from += feature->lengths[e];
	}
	feature->nbreaks = ibreak;
}

static void make_featurepoints(struct shown_area_xy *area) {
	uint64_t features = shared.used_features;
	for (int i=0; i<sizeof(features)*8 && features; i++)
		if (features & (1L<<i)) {
			_make_featurepoints(area->offset_i, area->offset_j, area->features[i]);
			features ^= 1L<<i;
		}
}

#define putval(buff, val) (buff += (memcpy(buff, &(val), sizeof(val)), sizeof(val)))
static void save_state(char* buff, const struct shown_area_xy *area) {
	putval(buff, area->offset_j);
	putval(buff, area->offset_i);
	putval(buff, data_per_pixel);
	putval(buff, shared.invert_y); // huomio: onko tämä aina globaali?
	putval(buff, win_w);
	putval(buff, win_h);
}
#undef putval

static void check_features(struct shown_area_xy *area, struct feature *feature) {
	if (!feature->points) {
		feature->points = malloc(feature->total * sizeof(point_t));
		feature->breaks = malloc(feature->total * sizeof(int));
		make_featurepoints(area);
		save_state(area->featureparams, area);
	}
	else {
		char new_params[size_feature_params];
		save_state(new_params, area);
		/* If current state differs from previous, save the new one and remake the coastlines. */
		if (memcmp(area->featureparams, new_params, size_feature_params)) {
			memcpy(area->featureparams, new_params, size_feature_params);
			make_featurepoints(area);
		}
	}
}

static void draw_feature(struct shown_area_xy *area, int ifeature) {
	check_features(area, area->features[ifeature]); // must be before the assignment of the variables
	struct feature *feature = area->features[ifeature];
	int nib = feature->nbreaks;
	int* breaks = feature->breaks;
	int istart = 0;
	set_color(shared.color_fg);
	point_t* points = feature->points;
	for (int ib=0; ib<nib; ib++) {
		draw_lines(points+istart, breaks[ib]-istart);
		istart = breaks[ib];
	}
}

#undef size_params
#undef coastl_get_point

#else // not HAVE_SHPLIB
#define init_feature(...) // ignored
#define draw_feature(...) // ignored
#define free_feature(...) // ignored
#endif // HAVE_SHPLIB
