#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char qbyte;
typedef enum {false, true} qboolean;
typedef struct image_s image_t;
typedef struct shader_s shader_t;
typedef struct entity_s entity_t;
typedef enum {TF_INVALID, TF_RGBA32, TF_8PAL32, TF_TRANS8} uploadfmt_t;
enum {QR_NONE, QR_TEST};
enum {mod_dummy, mod_sprite};
enum {IF_NOMIPMAP = 1, IF_NOGAMMA = 2, IF_CLAMP = 4, IF_PREMULTIPLYALPHA = 8};

/* ENGINE_DISK_SPRITES */
/* ENGINE_SPRITES */

typedef struct {
	char name[64];
	void *meshinfo;
	void *memgroup;
	int type, synctype, numframes;
	float mins[3], maxs[3];
} model_t;

#define QDECL
#define Q_memset memset
#define Q_snprintfz snprintf
#define Con_Printf(...) ((void)0)
#define Con_DPrintf(...) ((void)0)
static const struct { int ival; } r_sprite_backfacing = {0};
static const int qrenderer = QR_TEST;
static unsigned int textures, replacement_textures;
static void *allocations[32];
static size_t allocation_count;

static void *ZG_Malloc(void **group, size_t size)
{
	(void)group;
	assert(allocation_count < sizeof(allocations) / sizeof(allocations[0]));
	void *ptr = calloc(1, size);
	assert(ptr);
	allocations[allocation_count++] = ptr;
	return ptr;
}

static int LittleLong(int value)
{
	const qbyte *b = (const qbyte *)&value;
	return (int)(b[0] | (unsigned)b[1] << 8 | (unsigned)b[2] << 16 | (unsigned)b[3] << 24);
}

static short LittleShort(short value)
{
	const qbyte *b = (const qbyte *)&value;
	return (short)(b[0] | b[1] << 8);
}

static float LittleFloat(float value)
{
	int bits;
	memcpy(&bits, &value, sizeof(bits));
	bits = LittleLong(bits);
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static image_t *Image_GetTexture(const char *name, const char *group, int flags,
                                const void *pixels, const qbyte *palette,
                                int width, int height, uploadfmt_t format)
{
	(void)group;
	(void)flags;
	(void)palette;
	assert(strlen(name) < 65);
	if (format != TF_INVALID)
	{
		assert(width == 1 && height == 1 && *(const qbyte *)pixels == 7);
		textures++;
	}
	else if (width)
	{
		assert(!pixels);
		replacement_textures++;
	}
	return NULL;
}

/* ENGINE_FUNCTIONS */

int main(void)
{
	model_t model = {.name = "fixture"};
	dsprite_t header = {.version = LittleLong(SPRITE_VERSION), .type = LittleLong(SPR_VP_PARALLEL),
	                   .width = LittleLong(1), .height = LittleLong(1), .numframes = LittleLong(2)};
	dspriteframetype_t single = {.type = LittleLong(SPR_SINGLE)}, group = {.type = LittleLong(SPR_GROUP)};
	dspritegroup_t frames = {.numframes = LittleLong(1)};
	dspriteinterval_t interval = {.interval = LittleFloat(0.1f)};
	dspriteframe_t frame = {.width = LittleLong(1), .height = LittleLong(1)};
	_Alignas(dsprite_t) qbyte sprite[128];
	size_t size = 0;

#define APPEND(value) do { memcpy(sprite + size, &(value), sizeof(value)); size += sizeof(value); } while (0)
	APPEND(header);
	APPEND(single);
	APPEND(frame);
	sprite[size++] = 7;
	APPEND(group);
	APPEND(frames);
	APPEND(interval);
	size_t group_header_end = size;
	APPEND(frame);
	sprite[size++] = 7;
#undef APPEND

	assert(Mod_LoadSpriteModel(&model, sprite, size));
	assert(textures == 2 && model.numframes == 2 && model.type == mod_sprite);
	assert(Mod_LoadSpriteModel(&model, sprite, size - 1));
	assert(replacement_textures == 1);
	assert(!Mod_LoadSpriteModel(&model, sprite, group_header_end));
	assert(!Mod_LoadSpriteModel(&model, sprite, sizeof(header) - 1));

	dmd2sprite_t sp2 = {.version = LittleLong(SPRITE2_VERSION), .numframes = LittleLong(1)};
	strcpy(sp2.frames[0].name, "fixture.pcx");
	assert(Mod_LoadSprite2Model(&model, &sp2, sizeof(sp2)));
	assert(!Mod_LoadSprite2Model(&model, &sp2, sizeof(sp2) - 1));

	while (allocation_count)
		free(allocations[--allocation_count]);
	return 0;
}
