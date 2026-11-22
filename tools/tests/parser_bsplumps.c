#include <assert.h>
#include <stddef.h>
#include <string.h>

typedef unsigned char qbyte;
typedef enum {false, true} qboolean;
typedef struct bspx_header_s bspx_header_t;
typedef struct { void *archive; } model_t;
static const struct { int ival; } mod_loadmappackages = {1};

/* ENGINE_LUMP_TYPE */
/* ENGINE_BSPX_TYPES */

#define max(a, b) ((a) > (b) ? (a) : (b))

static unsigned int LittleLong(unsigned int value)
{
	const qbyte *b = (const qbyte *)&value;
	return b[0] | (unsigned)b[1] << 8 | (unsigned)b[2] << 16 | (unsigned)b[3] << 24;
}

static void Mod_LoadMapArchive(model_t *mod, void *data, size_t size)
{
	assert(size == 24 && !memcmp(data, "PK", 2));
	mod->archive = data;
}

/* ENGINE_FUNCTIONS */

int main(void)
{
	const qbyte entities[] = " \n// comment\n/* comment */{";
	assert(Mod_LumpStartsWithEntity(entities, sizeof(entities) - 1));
	assert(!Mod_LumpStartsWithEntity(entities, sizeof(entities) - 2));
	assert(!Mod_LumpStartsWithEntity(entities, 20));
	assert(!Mod_LumpStartsWithEntity(entities, 0));
	assert(!Mod_LumpStartsWithEntity((const qbyte *)"{word", 5));

	lump_t base = {.fileofs = 8, .filelen = 8};
	assert(BSP_LumpInRange(&base, 16));
	assert(!BSP_LumpInRange(&base, 15));

	const size_t header_offset = 16;
	const size_t directory_end = header_offset + offsetof(bspx_header_t, lumps) + sizeof(bspx_lump_t);
	_Alignas(bspx_header_t) char map[100] = {0};
	bspx_header_t *header = (void *)(map + header_offset);
	memcpy(header->id, "BSPX", 4);
	header->numlumps = LittleLong(1);
	header->lumps[0].fileofs = LittleLong(directory_end);
	header->lumps[0].filelen = LittleLong(4);
	assert(!BSPX_Setup(NULL, map, directory_end - 1, &base, 1));
	assert(!BSPX_Setup(NULL, map, directory_end + 3, &base, 1));
	memcpy(map + directory_end + 4, "PK", 2);
	model_t model = {0};
	assert(BSPX_Setup(&model, map, directory_end + 4 + 24, &base, 1) == header);
	assert(model.archive == map + directory_end + 4);
	return 0;
}
