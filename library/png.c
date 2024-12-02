#ifdef HAVE_PNG // wraps the whole file

#include <png.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>
extern int draw_w, draw_h;

static int write_png(unsigned char* rgb, const char* name) {
    png_structp png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png_p)
	return 1;
    png_infop info_p = png_create_info_struct(png_p);
    if(!info_p) {
	png_destroy_write_struct(&png_p, (png_infopp)NULL);
	return 1;
    }

    int height = draw_h + additional_height();

    png_set_IHDR(
	    png_p, info_p,
	    draw_w, height, 8, PNG_COLOR_TYPE_RGB,
	    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    FILE* file = fopen(name, "w");
    if(!file) {
	warn("Couldn't open %s (%s)", name, __func__);
	return 1;
    }
    png_init_io(png_p, file);
    png_write_info(png_p, info_p);

    png_byte** pngdata = png_malloc(png_p, height * sizeof(void*));
    for (int j=0; j<height; j++) {
	pngdata[j] = png_malloc(png_p, 3*draw_w);
	memcpy(pngdata[j], rgb + 3*draw_w * j, 3*draw_w);
    }

    png_set_rows(png_p, info_p, pngdata);
    png_write_png(png_p, info_p, PNG_TRANSFORM_IDENTITY, NULL);

    for (int j=0; j<height; j++)
	png_free(png_p, pngdata[j]);
    png_free(png_p, pngdata);

    fclose(file);
    png_destroy_write_struct(&png_p, &info_p);

    return 0;
}

static void rgba_to_rgb(char *to) {
    char *from = (void*)wlh.data;
    for (int j=0; j<draw_h + additional_height(); j++) {
	int indfrom = j*wlh.xres,
	    indto = j*draw_w;
	for (int i=0; i<draw_w; i++) {
	    to[(indto+i)*3+0] = from[(indfrom+i)*4+2];
	    to[(indto+i)*3+1] = from[(indfrom+i)*4+1];
	    to[(indto+i)*3+2] = from[(indfrom+i)*4+0];
	}
    }
}

#ifdef HAVE_WAYLAND
static void save_png(Arg _) {
    void* buffer = malloc(draw_w * (draw_h+additional_height()) * 3);
    rgba_to_rgb(buffer);
    char name[100];
    sprintf(name, "nctplot_%li.png", (long)time(NULL));
    write_png(buffer, name);
    free(buffer);
}
#else
static void save_png(Arg _) {
    fprintf(stderr, "funktio %s ei toimi ilman waylandia\n", __func__);
}
#endif

#endif // HAVE_PNG
