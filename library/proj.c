#include <proj.h>

struct proj_cookie {
    PJ *pj;
    PJ_CONTEXT *ctx;
};

static inline struct proj_cookie* init_proj(const char *from, const char* to) {
    struct proj_cookie *cookie = malloc(sizeof(struct proj_cookie));
    cookie->ctx = proj_context_create();
    cookie->pj = proj_create_crs_to_crs(cookie->ctx, from, to, NULL);
    return cookie;
}

static inline void convert_proj(void *vcookie, float x, float y, double out[2]) {
    PJ_COORD pjc = {.xy = {x,y}};
    PJ_XY xy = proj_trans(((struct proj_cookie*)vcookie)->pj, 1, pjc).xy;
    out[0] = xy.x;
    out[1] = xy.y;
}

static inline void destroy_proj(struct proj_cookie *cookie) {
    proj_destroy(cookie->pj);
    proj_context_destroy(cookie->ctx);
    free(cookie);
}
