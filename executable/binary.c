#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h> // fstat
#include <err.h>
#include <sys/mman.h>
#include <math.h>

static nct_set* read_binary(const char* name, long x, long y, nc_type dtype) {
    int fd = open(name, O_RDONLY);
    if (fd < 0)
	err(1, "Error, open %s", name);
    struct stat st;
    if (fstat(fd, &st))
	err(1, "fstat");
    long length = st.st_size / nctypelen(dtype);

    long z = 1;
    if (x > length) x = length;
    if (y > length) y = length;
    if (x && y) {
	if (x*y > length)
	    y = length / x;
	z = length / (x*y);
    }
    else if (x)
	y = length / x;
    else if (y)
	x = length / y;
    else {
	x = round(sqrt(length));
	y = length / x;
    }

    length = z*y*x;

    void* data = mmap(NULL, length*nctypelen(dtype), PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    nct_set* set = nct_create_simple(data, dtype, z, y, x);
    nct_var* var = nct_firstvar(set);
    var->not_freeable = 1;
    long varlen = var->len;
    if (varlen != length) {
	warnx("Length calculated wrong for %s: %li (calculated) ≠ %li (obtained).", name, length, varlen);
	if (varlen > length)
	    exit(1);
    }
    return set;
}

/* Variables are flagged as not_freeable if they were allocated here using mmap. */
static void free_binary(nct_set* set) {
    nct_foreach(set, var)
	if (var->not_freeable) {
	    munmap(var->data, var->len * nctypelen(var->dtype));
	    var->data = NULL;
	}
}
