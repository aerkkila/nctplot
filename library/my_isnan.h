#include <stdint.h>
#include <string.h>

static inline int my_isnan_float(float f) {
    const uint32_t exponent = ((1u<<31)-1) - ((1u<<(31-8))-1);
    uint32_t bits;
    memcpy(&bits, &f, 4);
    return (bits & exponent) == exponent;
}

static inline int my_isnan_double(double f) {
    const uint64_t exponent = ((1lu<<63)-1) - ((1lu<<(63-11))-1);
    uint64_t bits;
    memcpy(&bits, &f, 8);
    return (bits & exponent) == exponent;
}

#define my_isnan(a) \
    ((typeof(a))1.1 == 1 ? 0 : /* integer */ \
     sizeof(a) == 4 ? my_isnan_float(a) : \
     sizeof(a) == 8 ? my_isnan_double(a) : \
     -1)
