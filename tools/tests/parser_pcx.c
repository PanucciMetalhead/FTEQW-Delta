#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char qbyte;
typedef enum {false, true} qboolean;

/* ENGINE_PCX_TYPE */

#define BZF_Malloc malloc
#define BZ_Malloc malloc
#define BZ_Free free
#define min(a, b) ((a) < (b) ? (a) : (b))
#define Sys_Error(...) abort()
static const struct { int fullbright; } vid = {0};
static const struct { int ival; } r_fb_models = {0};

static void Con_Printf(const char *format, ...)
{
	(void)format;
}

static unsigned short LittleShort(unsigned short value)
{
	const qbyte *bytes = (const qbyte *)&value;
	return bytes[0] | (bytes[1] << 8);
}

/* ENGINE_FUNCTIONS */

int main(void)
{
	pcx_t header = {0};
	const qbyte runs[] = {1, 0xc2, 2, 0, 3, 2, 1, 0};
	const qbyte indices[] = {1, 2, 2, 3, 2, 1};
	const qbyte rgba[] = {
		10, 0, 0, 255, 20, 0, 0, 255, 20, 0, 0, 255,
		30, 0, 0, 255, 20, 0, 0, 255, 10, 0, 0, 255
	};
	_Alignas(pcx_t) qbyte file[sizeof(header) + sizeof(runs) + 769] = {0};
	qbyte unpacked[sizeof(indices)], palette[768];
	int width, height;

	header.manufacturer = 10;
	header.version = 5;
	header.encoding = 1;
	header.bits_per_pixel = 8;
	header.color_planes = 1;
	header.xmax = LittleShort(2);
	header.ymax = LittleShort(1);
	header.bytes_per_line = LittleShort(4);
	memcpy(file, &header, sizeof(header));
	memcpy(file + sizeof(header), runs, sizeof(runs));
	file[sizeof(file) - 769] = 12;
	file[sizeof(file) - 768 + 3] = 10;
	file[sizeof(file) - 768 + 6] = 20;
	file[sizeof(file) - 768 + 9] = 30;

	qbyte *pixels = ReadPCXFile(file, sizeof(file), &width, &height);
	assert(pixels && width == 3 && height == 2 && !memcmp(pixels, rgba, sizeof(rgba)));
	free(pixels);
	assert(ReadPCXData(file, sizeof(file), 3, 2, unpacked) == unpacked);
	assert(!memcmp(unpacked, indices, sizeof(indices)));
	assert(ReadPCXPalette(file, sizeof(file), palette) == palette && palette[9] == 30);
	pixels = Skin_ParsePCX("fixture", (const pcx_t *)file, sizeof(file), &width, &height);
	assert(pixels && width == 3 && height == 2 && !memcmp(pixels, indices, sizeof(indices)));
	free(pixels);

	assert(!ReadPCXFile(file, sizeof(header), &width, &height));
	assert(!ReadPCXPalette(file, sizeof(header), palette));
	assert(!ReadPCXData(file, sizeof(header) - 1, 3, 2, unpacked));
	assert(!ReadPCXData(file, sizeof(header) + 2, 3, 2, unpacked));
	assert(!Skin_ParsePCX("fixture", (const pcx_t *)file, sizeof(header), &width, &height));
	return 0;
}
