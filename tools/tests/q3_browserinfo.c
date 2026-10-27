#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
enum { NP_DGRAM, NA_IP = 1, NA_IPV6 };
typedef struct { const char *info; } detail_t;
typedef struct {
	int players, maxplayers, numhumans;
	struct { int type, prot; } adr;
	const char *name, *map;
	detail_t *moreinfo;
} serverinfo_t;
static char *va(const char *format, ...)
{
	static char out[80]; va_list ap;
	va_start(ap, format); vsnprintf(out, sizeof(out), format, ap); va_end(ap); return out;
}
static char *get_key(const char *info, const char *key)
{
	static char values[4][1024]; static unsigned int index;
	char *out = values[index++ % 4]; char pattern[1024];
	snprintf(pattern, sizeof(pattern), "\\%s\\", key);
	const char *value = strstr(info, pattern); *out = 0;
	if (value) {
		value += strlen(pattern); size_t len = strcspn(value, "\\");
		assert(len < sizeof(values[0])); memcpy(out, value, len); out[len] = 0;
	}
	return out;
}
static void set_key(char *out, const char *key, const char *value, size_t size)
{
	size_t len = strlen(out);
	assert(len + strlen(key) + strlen(value) + 2 < size);
	assert(!*get_key(out, key));
	snprintf(out+len, size-len, "\\%s\\%s", key, value);
}
static struct {
	void (*SetInfoKey)(char *, const char *, const char *, size_t);
	char *(*GetInfoKey)(const char *, const char *);
} world_api = {set_key, get_key}, *worldfuncs = &world_api;
/* ENGINE_FUNCTIONS */
int main(void)
{
	char raw[4000], out[1024];
	memset(raw, 'x', sizeof(raw));
	strcpy(raw, "\\clients\\999\\hostname\\stale\\gametype\\4\\g_needpass\\1\\optional\\");
	size_t prefix = strlen(raw);
	memset(raw+prefix, 'x', sizeof(raw)-prefix-1); raw[sizeof(raw)-1] = 0;
	detail_t detail = {raw};
	serverinfo_t info = {.players=5, .maxplayers=16, .numhumans=3, .adr={NA_IP, NP_DGRAM}, .name="OA Test", .map="oa_ctf4ish"};
	info.moreinfo = &detail;
	UI_WriteServerInfo(out, sizeof(out), &info);
	assert(!strcmp(get_key(out, "clients"), "5"));
	assert(!strcmp(get_key(out, "hostname"), "OA Test"));
	assert(!strcmp(get_key(out, "mapname"), "oa_ctf4ish"));
	assert(!strcmp(get_key(out, "gametype"), "4"));
	assert(!strcmp(get_key(out, "g_needpass"), "1"));
	assert(!*get_key(out, "optional"));
	for (size_t size = 1; size < sizeof(out); size++) {
		memset(out, '?', sizeof(out));
		UI_WriteServerInfo(out, size, &info);
		assert(strlen(out) < size && (size == sizeof(out) || out[size] == '?'));
	}
	UI_WriteServerInfo(out, sizeof(out), NULL); assert(!*out);
	info.adr.type = NA_IPV6;
	UI_WriteServerInfo(out, sizeof(out), &info); assert(!strcmp(get_key(out, "nettype"), "2"));
	puts("Q3 browser info: long replies, authoritative fields and small-buffer boundaries passed");
	return 0;
}
