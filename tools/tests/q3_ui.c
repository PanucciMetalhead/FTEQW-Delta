#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

enum { false, true, NA_INVALID = 0, NP_DGRAM = 0, NP_INVALID = 10 };
typedef struct { int isopaque; } menu_t;
static menu_t uimenu;
static int keycatcher, linked, unlinks;
static char queued[2048];
static struct {
	unsigned int startms;
	struct { int type, prot; } adr;
	char adrstring[64];
	const char *broker;
} ui_pings[32];

static void push(menu_t *menu, int prompt)
{
	assert(menu == &uimenu && !prompt);
	linked = 1;
}
static void unlink_menu(menu_t *menu, int forced)
{
	assert(menu == &uimenu && !forced);
	linked = 0;
	unlinks++;
}
static struct {
	void (*Menu_Push)(menu_t *, int);
	void (*Menu_Unlink)(menu_t *, int);
} input_api = {push, unlink_menu}, *inputfuncs = &input_api;

// The parser is an injected engine interface; exercise its normal token contract.
static const char *parse(const char *text, char *out, size_t size, void *type)
{
	size_t n = 0;
	int quoted;
	assert(!type && size > 0);
	while (*text && isspace((unsigned char)*text)) text++;
	quoted = *text == '"';
	if (quoted) text++;
	while (*text && (quoted ? *text != '"' : !isspace((unsigned char)*text))) {
		if (n + 1 < size) out[n++] = *text;
		text++;
	}
	out[n] = 0;
	if (quoted && *text == '"') text++;
	return text;
}
static void add_text(const char *text, int insert)
{
	assert(!insert && strlen(text) < sizeof(queued));
	strcpy(queued, text);
}
static void UI_Restart_f(void) {}
static int add_command(const char *name, void (*fn)(void), const char *description)
{
	assert(!strcmp(name, "ui_restart") && fn == UI_Restart_f && description);
	return true;
}
static struct {
	const char *(*ParseToken)(const char *, char *, size_t, void *);
	void (*AddText)(const char *, int);
	int (*AddCommand)(const char *, void (*)(void), const char *);
} command_api = {parse, add_text, add_command}, *cmdfuncs = &command_api;

enum { CVAR_ARCHIVE = 1 };
static void *cl_shownet_ptr, *cl_c2sdupe_ptr, *cl_nodelta_ptr;
static int max_ping_default;
static void *get_cvar(const char *name, const char *value, int flags, const char *description, const char *group)
{
	assert(!strcmp(group, "Q3 Compat"));
	if (!strcmp(name, "cl_maxPing")) {
		assert(flags == CVAR_ARCHIVE && description);
		max_ping_default = atoi(value);
	}
	return &max_ping_default;
}
static struct { void *(*GetNVFDG)(const char *, const char *, int, const char *, const char *); } cvar_api = {get_cvar}, *cvarfuncs = &cvar_api;

#define CL_MASTER 1
typedef int qboolean;
static qboolean ui_masterquery_pending;
static unsigned int ui_masterquery_startms, now_ms;
static unsigned int discovered;
static int queries, polls;
static int query_servers(void) { queries++; return 1; }
static void check_sockets(void) { polls++; }
static unsigned int total_count(void) { return discovered; }
static unsigned int milliseconds(void) { return now_ms; }
static struct {
	int (*QueryServers)(void);
	void (*CheckPollSockets)(void);
	unsigned int (*TotalCount)(void);
} master_api = {query_servers, check_sockets, total_count}, *masterfuncs = &master_api;
static struct { unsigned int (*GetMilliseconds)(void); } clock_api = {milliseconds}, *plugfuncs = &clock_api;

/* ENGINE_FUNCTIONS */

#define HAVE_IPV6 1
enum { NA_IP = 2, NA_IPV6 = 3, SS_GETINFO = 128 };
static struct { unsigned char data[128]; int currentbit, cursize; } net_message;
static int parsed, family;
static void CL_MasterListParse(int address_type, int flags, int padded)
{
	int pos = net_message.currentbit/8;
	int bytes = family == NA_IP ? 4 : 16;
	assert(address_type == NA_IP || address_type == NA_IPV6);
	assert(flags == SS_GETINFO && padded);
	// This is where the real parser consumes the prefix's final byte, then
	// the first endpoint. A bit/byte mix-up skips four IPv4 entries entirely.
	assert(pos + 2 + bytes + 2 <= net_message.cursize);
	assert(net_message.data[pos] == 'e' || net_message.data[pos] == '6');
	pos++;
	assert(net_message.data[pos++] == (family == NA_IP ? '\\' : '/'));
	for (int i = 0; i < bytes; i++) assert(net_message.data[pos++] == i + 1);
	assert(net_message.data[pos++] == 0x6d);
	assert(net_message.data[pos] == 0x38);
	parsed++;
}
static void master_response(const char *s)
{
	int c = 4*8; // The engine cursor is already measured in bits.
	/* MASTER_RESPONSES */
}
static void check_master(const char *prefix, int address_family)
{
	int pos = 4;
	family = address_family;
	memset(net_message.data, 255, 4);
	memcpy(net_message.data + pos, prefix, strlen(prefix));
	pos += (int)strlen(prefix);
	net_message.data[pos++] = family == NA_IP ? '\\' : '/';
	for (int i = 0; i < (family == NA_IP ? 4 : 16); i++) net_message.data[pos++] = i + 1;
	net_message.data[pos++] = 0x6d;
	net_message.data[pos++] = 0x38;
	net_message.cursize = pos;
	net_message.data[pos] = 0;
	master_response((const char *)net_message.data + 4);
}

#define POLLTOTALSOCKETS 1
#define countof(a) (sizeof(a)/sizeof((a)[0]))
enum { MP_QUAKE3, MP_DPMASTER };
typedef struct { int unused; } netadr_t;
static struct { int protocol; struct { const char *name, *string; } cv; } net_masterlist[] = {
	{MP_QUAKE3, {"net_q3master1", "master"}},
	{MP_DPMASTER, {"net_master2", "master"}}
};
static struct { const char *string; } com_protocolname = {"Quake3"};
static char com_token[1024], master_packet[1024];
static int sent;
#define COM_Parse(data) parse(data, com_token, sizeof(com_token), NULL)
#define COM_ParseOut(data, token, size) parse(data, token, size, NULL)
#define Q_strcasecmp strcmp
#define Q_snprintfz snprintf
static size_t NET_StringToAdr2(const char *text, int port, netadr_t *adr, size_t count, const char **path)
{
	assert(!strcmp(text, "master") && !port && count && !path);
	adr->unused = 0;
	return 1;
}
static char *va(const char *format, ...)
{
	static char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	return buffer;
}
static void NET_SendPollPacket(size_t length, const void *data, netadr_t adr)
{
	assert(length < sizeof(master_packet) && !adr.unused);
	memcpy(master_packet, data, length);
	master_packet[length] = 0;
	sent++;
}

/* MASTER_REQUEST */

int main(void)
{
	UI_Init();
	assert(max_ping_default == 800);
	const char *commands[] = {"connect 127.0.0.1:27960\n", "  connect \"[::1]:27960\"\n"};
	for (size_t i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
		Q3_SetKeyCatcher(1|2|8);
		assert(linked);
		UI_QueueCommand(commands[i]);
		assert(!linked && keycatcher == (1|8));
		assert(!strcmp(queued, commands[i]));
	}
	assert(unlinks == 2);
	Q3_SetKeyCatcher(2);
	UI_QueueCommand("connect\n");
	assert(linked && keycatcher == 2); // Invalid usage must retain the menu.
	UI_QueueCommand("set password secret\n");
	assert(linked && !strcmp(queued, "set password secret\n"));
	UI_QueueCommand("disconnect\n");
	assert(linked && keycatcher == 2);
	for (int i = 0; i < 32; i++) {
		ui_pings[i].startms = 123;
		ui_pings[i].adr.type = 2;
		ui_pings[i].adr.prot = NP_DGRAM;
		strcpy(ui_pings[i].adrstring, "127.0.0.1:27960");
		ui_pings[i].broker = "stale broker";
		UI_ClearPing(i);
		assert(ui_pings[i].adr.type == NA_INVALID && ui_pings[i].adr.prot == NP_INVALID);
		assert(!ui_pings[i].adrstring[0] && !ui_pings[i].broker && !ui_pings[i].startms);
	}
	check_master("getserversResponse", NA_IP);
	check_master("getserversResponse6", NA_IPV6);
	check_master("getserversExtResponse", NA_IP);
	check_master("getserversExtResponse", NA_IPV6);
	assert(parsed == 4);
	NetQ3_GlobalServers_Request(0, 68, "empty full");
	assert(!strcmp(master_packet, "\xff\xff\xff\xffgetservers 68 empty full\n"));
	com_protocolname.string = "Quake3Arena";
	NetQ3_GlobalServers_Request(0, 71, "empty full");
	assert(!strcmp(master_packet, "\xff\xff\xff\xffgetservers Quake3Arena 71 empty full\n"));
	NetQ3_GlobalServers_Request(countof(net_masterlist), 71, "");
	assert(sent == 2);
	assert(NetQ3_MasterForSlot(0) == 0);
	assert(NetQ3_MasterForSlot(1) == 1);
	assert(NetQ3_MasterForSlot(2) == countof(net_masterlist));
	assert(NetQ3_MasterForSlot((size_t)-1) == countof(net_masterlist));
	now_ms = 100;
	UI_QueueCommand("globalservers 0 71 empty full\n");
	assert(ui_masterquery_pending && UI_ServerCount() == -1);
	now_ms = 1000;
	assert(UI_ServerCount() == -1); // A slow master reply is not an empty list.
	discovered = 42;
	assert(UI_ServerCount() == 42 && !ui_masterquery_pending);
	assert(queries == 0 && polls == 3); // Do not fan out to unrelated masters.
	discovered = 0;
	now_ms = 2000;
	UI_QueueCommand("globalservers 0 71\n");
	now_ms = 6999;
	assert(UI_ServerCount() == -1);
	now_ms = 7000;
	assert(UI_ServerCount() == 0 && !ui_masterquery_pending);
	UI_QueueCommand("globalservers 0 71\n");
	UI_QueueCommand("localservers\n");
	assert(!ui_masterquery_pending && UI_ServerCount() == 0);
	puts("Q3 UI: connection handoff, ping cleanup and IPv4/IPv6 master reply cursors passed");
	return 0;
}
