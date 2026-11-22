#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char qbyte;
typedef qbyte byte_vec4_t[4];
typedef enum {false, true} qboolean;
typedef enum {
	PTI_INVALID, PTI_L8, PTI_L8A8, PTI_RGBA8, PTI_RGBX8,
	PTI_LLLX8, PTI_LLLA8, PTI_R16F, PTI_RGBA16F
} uploadfmt_t;

#define BZF_Malloc malloc
#define BZ_Free free
#define Con_Printf(...) ((void)0)
#define NTSC_RED 0.299
#define NTSC_GREEN 0.587
#define NTSC_BLUE 0.114

/* ENGINE_TGA_READER */

static void check_pixels(qbyte *image, int length, const qbyte *expected, size_t size)
{
	int width, height;
	uploadfmt_t format;
	void *pixels = ReadTargaFile(image, length, &width, &height, &format, false, PTI_INVALID);
	assert(pixels && width == image[12] && height == image[14]);
	assert(!memcmp(pixels, expected, size));
	free(pixels);

	assert(!ReadTargaFile(image, length - 1, &width, &height, &format, false, PTI_INVALID));
}

int main(void)
{
	qbyte rgb[18 + 140 + 3] = {
		[0] = 140, [2] = 2, [12] = 1, [14] = 1, [16] = 24, [17] = 32,
		[158] = 7, 8, 9
	};
	const qbyte rgba[] = {9, 8, 7, 255};
	check_pixels(rgb, sizeof(rgb), rgba, sizeof(rgba));
	int width, height;
	uploadfmt_t format;
	assert(!ReadTargaFile(rgb, 18, &width, &height, &format, false, PTI_INVALID));

	qbyte indexed[] = {
		[1] = 1, [2] = 1, [3] = 5, [5] = 1, [7] = 24,
		[12] = 1, [14] = 1, [16] = 8, [17] = 32,
		[18] = 7, 8, 9, 5
	};
	check_pixels(indexed, sizeof(indexed), rgba, sizeof(rgba));

	qbyte rle[] = {
		[2] = 10, [12] = 2, [14] = 2, [16] = 24, [17] = 32,
		[18] = 0x82, 7, 8, 9, 0, 1, 2, 3
	};
	const qbyte rle_rgba[] = {9, 8, 7, 255, 9, 8, 7, 255, 9, 8, 7, 255, 3, 2, 1, 255};
	check_pixels(rle, sizeof(rle), rle_rgba, sizeof(rle_rgba));

	_Alignas(unsigned short) qbyte half[] = {
		[0] = 1, [2] = 0x83, [12] = 1, [14] = 1, [16] = 16, [17] = 32,
		[19] = 0, 0x3c
	};
	const unsigned short half_white = 0x3c00;
	check_pixels(half, sizeof(half), (const qbyte *)&half_white, sizeof(half_white));
	return 0;
}
