#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
enum { false, true, CP_QUAKE3 = 3 };
#define S_COLOR_GRAY ""
static struct { int trying, protocol; } connectinfo;
static struct { char data[1024]; } net_message;
static int net_from, cl_disconnectreason, aborted, assigned;
static int CL_IsPendingServerAddress(int *from) { return *from == 1; }
static int CL_IsPendingServerBaseAddress(int *from) { return *from != 0; }
static void Con_Printf(const char *text, ...) { (void)text; }
#define Con_TPrintf Con_Printf
static void CL_ConnectAbort(const char *format, ...)
{
	assert(!strcmp(format, "%s")); aborted++; connectinfo.trying = false;
}
static void Cvar_Set(int *var, const char *value)
{
	assert(var == &cl_disconnectreason && !strcmp(value, "Duplicate GUID")); assigned++;
}
static void receive(void)
{
	int c = 'p';
	/* Q3_REJECTION */
}
int main(void)
{
	strcpy(net_message.data+4, "print\nDuplicate GUID");
	connectinfo.trying = true; connectinfo.protocol = CP_QUAKE3;
	net_from = 0; receive(); assert(connectinfo.trying && !aborted);
	net_from = 2; receive(); assert(connectinfo.trying && !aborted); // same host, different port
	net_from = 1; receive(); assert(!connectinfo.trying && aborted == 1);
	receive(); assert(aborted == 1);
	connectinfo.protocol = 2; connectinfo.trying = true;
	receive(); assert(connectinfo.trying && aborted == 1);
	assert(assigned == 1); // preserve legacy unrelated-protocol handling
	puts("Q3 rejection: exact pending server stops retries; other sources cannot abort a join");
	return 0;
}
