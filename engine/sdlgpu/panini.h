/* Panini source allocation limits. GPL-2.0-or-later. */
#ifndef FTE_GPU_PANINI_H
#define FTE_GPU_PANINI_H
#include <math.h>

static void GPU_PaniniDimensions(unsigned width, unsigned height, float requested,
    unsigned *sourcewidth, unsigned *sourceheight)
{
    double scale = isfinite(requested) ? fmin(2.0, fmax(1.0, requested)) : 1.0;
    double pixels = (double)width * height;
    double budget = fmax(16000000.0, pixels);
    double largest = fmax(width, height);
    /* Never subsample an already large native view. */
    if (!width || !height) { *sourcewidth = width; *sourceheight = height; return; }
    scale = fmin(scale, sqrt(budget / pixels));
    scale = fmin(scale, fmax(8192.0, largest) / largest);
    *sourcewidth = (unsigned)floor(width * scale);
    *sourceheight = (unsigned)floor(height * scale);
}
static unsigned GPU_PaniniLevels(unsigned width, unsigned height)
{
    unsigned levels = 1, dimension = width > height ? width : height;
    while (dimension > 1) { dimension >>= 1; ++levels; }
    return levels;
}
#endif
