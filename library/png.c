#ifdef HAVE_PNG // wraps the whole file

#include <png.h>
#include <string.h>
#include <err.h>
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

    png_set_IHDR(
	    png_p, info_p,
	    draw_w, draw_h, 8, PNG_COLOR_TYPE_RGB,
	    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    FILE* file = fopen(name, "w");
    if(!file) {
	warn("Couldn't open %s (%s)", name, __func__);
	return 1;
    }
    png_init_io(png_p, file);
    png_write_info(png_p, info_p);

    png_byte** pngdata = png_malloc(png_p, draw_h * sizeof(void*));
    for (int j=0; j<draw_h; j++) {
	pngdata[j] = png_malloc(png_p, 3*draw_w);
	memcpy(pngdata[j], rgb + 3*draw_w * j, 3*draw_w);
    }

    png_set_rows(png_p, info_p, pngdata);
    png_write_png(png_p, info_p, PNG_TRANSFORM_IDENTITY, NULL);

    for (int j=0; j<draw_h; j++)
	png_free(png_p, pngdata[j]);
    png_free(png_p, pngdata);
  
    fclose(file);
    png_destroy_write_struct(&png_p, &info_p);

    return 0;
}

static void draw2d_buffer(void* buff, int offset_j) {
    for (int i=0; i<draw_w*draw_h; i++)
	memcpy(buff+i*3, globs.color_bg, 3);
    if (g_only_nans) return;

    void* dataptr = var->data + (plt.area->znum*plt.stepsize_z*(zid>=0) - var->startpos) * g_size1;

    float fdataj = offset_j;
    int idataj = round(fdataj), j;
    if (globs.invert_y)
	for(j=draw_h-g_pixels_per_datum; j>=0; j-=g_pixels_per_datum) {
	    draw_row_buffer(var->dtype,
		    dataptr + g_size1*idataj*g_xlen,
		    buff + j * draw_w*3);
	    idataj = round(fdataj += g_data_per_step);
	}
    else
	for(j=0; j<draw_h; j+=g_pixels_per_datum) {
	    draw_row_buffer(var->dtype,
		    dataptr + g_size1*idataj*g_xlen,
		    buff + j * draw_w*3);
	    idataj = round(fdataj += g_data_per_step);
	}
}

static void save_png(Arg _) {
    void* buffer = malloc(draw_w*draw_h*3);
    draw2d_buffer(buffer, plt.area->offset_j);
    char name[100];
    sprintf(name, "nctplot_%li.png", (long)time(NULL));
    write_png(buffer, name);
    free(buffer);
}

#endif // HAVE_PNG
