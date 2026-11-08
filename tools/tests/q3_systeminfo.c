#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int qboolean;
enum { false, true, MAX_QPATH = 64 };
static int changed, downloading, forced, cheats, errors;
static int expected_mode = 2;
static const char *expected_mod = "", *expected_pc = "11 22 33", *expected_pn = "pak0 pak1 pak2";
static const char *expected_rc = "11 22", *expected_rn = "baseoa/pak0 baseoa/pak1";
static struct { int fs_key; } ccs = {123};
static void Con_Printf(const char *format, ...) { (void)format; }
static char *get_key(const char *info, const char *key)
{
	static char value[1024];
	char pattern[80];
	snprintf(pattern, sizeof(pattern), "\\%s\\", key);
	const char *start = strstr(info, pattern);
	value[0] = 0;
	if (start) {
		start += strlen(pattern);
		size_t len = strcspn(start, "\\");
		if (len < sizeof(value)) { memcpy(value, start, len); value[len] = 0; }
	}
	return value;
}
static struct { char *(*GetInfoKey)(const char *, const char *); } world_api = {get_key}, *worldfuncs = &world_api;
static void end_game(const char *reason) { assert(strstr(reason, "Oversized")); errors++; }
static struct { void (*EndGame)(const char *); } plug_api = {end_game}, *plugfuncs = &plug_api;
static void force_cheats(int force, int allow) { forced = force; cheats = allow; }
static struct { void (*ForceCheatVars)(int, int); } client_api = {force_cheats}, *clientfuncs = &client_api;
static int CLQ3_SendDownloads(char *crcs, char *names)
{
	assert(!strcmp(crcs, expected_rc) && !strcmp(names, expected_rn));
	return downloading;
}
static void pure_mode(const char *gamedir, int mode, char *pn, char *pc, char *rn, char *rc, int seed)
{
	assert(!strcmp(gamedir, expected_mod));
	assert(mode == expected_mode && seed == 123);
	assert(!strcmp(pc, *expected_pc ? expected_pc : expected_rc) &&
		!strcmp(pn, *expected_pc ? expected_pn : expected_rn));
	assert(!strcmp(rc, expected_rc) && !strcmp(rn, expected_rn));
	changed++;
}
static struct { void (*PureMode)(const char *, int, char *, char *, char *, char *, int); } fs_api = {pure_mode}, *fsfuncs = &fs_api;

/* ENGINE_FUNCTIONS */

static void check(const char *pure, const char *cheat)
{
	char info[20000];
	snprintf(info, sizeof(info), "\\sv_pure\\%s\\sv_cheats\\%s\\fs_game\\%s\\sv_paks\\%s\\sv_pakNames\\%s\\sv_referencedPaks\\%s\\sv_referencedPakNames\\%s",
		pure, cheat, expected_mod, expected_pc, expected_pn, expected_rc, expected_rn);
	int old = changed;
	assert(CLQ3_SystemInfoChanged(info) == !downloading);
	assert(changed == old + !downloading);
	assert(forced == !!(atoi(pure) || atoi(cheat)) && cheats == atoi(cheat));
}
int main(void)
{
	check("1", "0");
	expected_mod = "custommod";
	check("1", "0");
	expected_mode = 0;
	check("0", "1");
	downloading = 1;
	check("0", "0");
	downloading = 0;
	// A51 OA advertises sv_pure but no package list. Base assets stay visible.
	expected_mod = "a51oa";
	expected_pc = expected_pn = expected_rc = expected_rn = "";
	check("1", "0");
	expected_rc = "11 22"; expected_rn = "a51oa/mod a51oa/map";
	expected_mode = 1;
	check("1", "0"); // non-pure download-cache mounting without an allowlist
	// Real Q3 systeminfo supports values larger than GetInfoKey's 1024 bytes.
	char long_list[4000];
	memset(long_list, '7', sizeof(long_list)-1); long_list[sizeof(long_list)-1] = 0;
	expected_pc = expected_rc = long_list;
	expected_pn = expected_rn = long_list;
	expected_mode = 2;
	check("1", "0");
	char small[4] = "old";
	assert(!CLQ3_InfoValue("\\k\\1234", "k", small, sizeof(small)) && !*small);
	assert(CLQ3_InfoValue("\\k\\123", "k", small, sizeof(small)) && !strcmp(small, "123"));
	assert(CLQ3_InfoValue("\\broken", "k", small, sizeof(small)) && !*small);
	assert(!CLQ3_SystemInfoChanged("\\fs_game\\abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"));
	assert(errors == 1);
	puts("Q3 systeminfo: base/mod selection, empty pure lists, long pak lists and download deferral passed");
	return 0;
}
