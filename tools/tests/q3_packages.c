#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int qboolean;
enum { false, true, MAX_OSPATH = 1024, SPF_REFERENCED = 1, SPF_COPYPROTECTED = 2 };
typedef struct searchpath_s {
	struct searchpath_s *next, *nextpure;
	int flags, crc_check;
	char *purepath;
} searchpath_t;
static searchpath_t *com_searchpaths, *com_purepaths;
static char *fs_refcrcs, *fs_refnames;
static char *va(const char *format, ...)
{
	static char out[2048]; va_list ap;
	va_start(ap, format); vsnprintf(out, sizeof(out), format, ap); va_end(ap);
	return out;
}
static void Q_strncatz(char *out, const char *text, size_t size)
{
	size_t len = strlen(out); if (len < size) snprintf(out+len, size-len, "%s", text);
}
static void COM_StripExtension(const char *text, char *out, size_t size)
{
	snprintf(out, size, "%s", text); char *dot = strrchr(out, '.'); if (dot) *dot = 0;
}
static void COM_DefaultExtension(char *out, const char *ext, size_t size)
{
	if (!strrchr(out, '.')) Q_strncatz(out, ext, size);
}
static char *COM_ParseOut(char *text, char *out, size_t size)
{
	if (!text) { *out = 0; return NULL; }
	while (*text == ' ') text++;
	size_t len = strcspn(text, " "); assert(len < size);
	memcpy(out, text, len); out[len] = 0;
	return *text ? text+len : NULL;
}
/* ENGINE_FUNCTIONS */
static void reference_lookup(int rebuild, int checksum, const char *expected)
{
	char crctok[64], nametok2[1024];
	const char *pname = "unreferenced";
	int crc = checksum;
	snprintf(crctok, sizeof(crctok), "%i", crc);
	if (rebuild) {
		/* REBUILD_REFERENCES */
	} else {
		/* CHECK_REFERENCES */
	}
	assert(atoi(crctok) == checksum); // reference lookup must not destroy the allowlist token
	assert(!strcmp(pname, expected));
}
int main(void)
{
	searchpath_t b = {NULL, NULL, 0, 22, "baseoa/pak2-players.pk3"};
	searchpath_t a = {&b, &b, SPF_REFERENCED, 11, "baseoa/pak0.pk3"};
	char out[1024];
	com_searchpaths = &a;
	for (int pure = 0; pure < 2; pure++) {
		com_purepaths = pure ? &a : NULL;
		assert(!strcmp(FS_GetPackNames(out, sizeof(out), 0, false), "baseoa/pak0 baseoa/pak2-players "));
		assert(!strcmp(FS_GetPackHashes(out, sizeof(out), false), "11 22 "));
		assert(!strcmp(FS_GetPackNames(out, sizeof(out), 1, false), "baseoa/pak0 "));
		assert(!strcmp(FS_GetPackHashes(out, sizeof(out), true), "11 "));
		assert(!strcmp(FS_GetPackNames(out, sizeof(out), 2, true), "*baseoa/pak0.pk3 baseoa/pak2-players.pk3 "));
	}
	fs_refcrcs = "11 33"; fs_refnames = "baseoa/pak0 baseoa/pak3";
	for (int rebuild = 0; rebuild < 2; rebuild++) {
		reference_lookup(rebuild, 11, "baseoa/pak0.pk3");
		reference_lookup(rebuild, 22, "");
		reference_lookup(rebuild, 33, "baseoa/pak3.pk3");
	}
	puts("Q3 packages: aligned name/hash lists and unreferenced allowed-package checksums passed");
	return 0;
}
