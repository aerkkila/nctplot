#include <proj.h>

static PJ_CONTEXT* ctx;
static PJ* pj;
static PJ_COORD pjc = {0};

void coastl_init_proj(const char* to) {
    ctx = proj_context_create();
    pj = proj_create_crs_to_crs(ctx, "+proj=longlat", to, NULL);
}

void coastl_proj_convert(float lon, float lat, double out[2]) {
    pjc.xy.x = lon;
    pjc.xy.y = lat;
    PJ_XY xy = proj_trans(pj, 1, pjc).xy;
    out[0] = xy.x;
    out[1] = xy.y;
}

void coastl_proj_destroy() {
    proj_destroy(pj);
    proj_context_destroy(ctx);
    pj = NULL;
    ctx = NULL;
}
