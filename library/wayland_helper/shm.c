#include <sys/mman.h> // mmap, shm_*
#include <unistd.h> // ftruncate
#include <fcntl.h> // O_*
#include <err.h>
#include <sys/time.h>
#include <stdio.h>

static struct wl_shm* shared_memory; // used in the main file
static size_t imagebuffer_size;

static int _init_shm_file(int koko) {
    char name[48];
    struct timeval t;
    gettimeofday(&t, NULL);
    sprintf(name, "wl_%lx%lx", (long unsigned)t.tv_sec, (long unsigned)t.tv_usec/1000);
    int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
    if (fd < 0)
	err(1, "shm_open %s", name);
    shm_unlink(name); // File stays until fd is closed.
    if (ftruncate(fd, koko) < 0)
	err(1, "ftruncate %i: %s, koko = %i t", fd, name, koko);
    return fd;
}

static void destroy_imagebuffer(struct wayland_helper*);

/* Called from xdgconfigure */
static struct wl_buffer* attach_imagebuffer(struct wayland_helper *image) {
    destroy_imagebuffer(image);
    imagebuffer_size = image->xres*image->yres*4;
    int fd = _init_shm_file(imagebuffer_size);
    assert(fd >= 0);
    image->data = mmap(NULL, imagebuffer_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (image->data == MAP_FAILED)
	err(1, "mmap %zu (fd=%i) %s: %i", imagebuffer_size, fd, __FILE__, __LINE__);
    image->redraw = 1;

    struct wl_shm_pool* pool  = wl_shm_create_pool(shared_memory, fd, imagebuffer_size);
    struct wl_buffer* buffer = wl_shm_pool_create_buffer(pool, 0, image->xres, image->yres, image->xres*4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    return buffer;
}

static void destroy_imagebuffer(struct wayland_helper *image) {
    if (image->data) {
	munmap(image->data, imagebuffer_size);
	image->data = NULL;
    }
}
