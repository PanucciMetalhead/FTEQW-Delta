#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { true=1, DL_Q3=3, DLFAIL_SERVERCVAR, DLFAIL_CLIENTFILE };
typedef struct { unsigned size, ratebytes; int method, percent; const char *remotename; void *file; } qdownload_t;
static struct { qdownload_t *download; int downloadchunknum, servercount; } ccs;
static int numbers[8], number_count, number_index, data_count, data_index;
static const char *data, *error_text;
static int finished, failed, ended, begin_failure, writes;
static char commands[1024];
static int read_number(void) { assert(number_index < number_count); return numbers[number_index++]; }
static int read_byte(void) { assert(data_index < data_count); return (unsigned char)data[data_index++]; }
static char *read_string(void) { return (char *)error_text; }
static struct { int (*ReadShort)(void), (*ReadLong)(void), (*ReadByte)(void); char *(*ReadString)(void); } msg_api = {read_number, read_number, read_byte, read_string}, *msgfuncs=&msg_api;
typedef struct { const char *name; } cvar_t;
static cvar_t cv = {"cl_downloadSize"};
static cvar_t *get_cvar(const char *n, const char *v, int f, const char *d, const char *g) { (void)n;(void)v;(void)f;(void)d;(void)g;return &cv; }
static void set_float(const char *n, float v) { assert(n == cv.name && v == 4); }
static struct { void (*SetFloat)(const char *, float); cvar_t *(*GetNVFDG)(const char *,const char *,int,const char *,const char *); } cv_api={set_float,get_cvar}, *cvarfuncs=&cv_api;
static void CLQ3_SendClientCommand(const char *format, ...)
{
	va_list ap; va_start(ap, format); size_t len=strlen(commands);
	vsnprintf(commands+len,sizeof(commands)-len,format,ap); va_end(ap); strcat(commands,";");
}
static void Con_Printf(const char *format, ...) { (void)format; }
#define Con_DPrintf Con_Printf
static int begin(qdownload_t *dl) { if (!begin_failure) dl->file=&writes; return !begin_failure; }
static void finish(qdownload_t *dl) { assert(!ccs.download); finished++; free(dl); }
static void fail(const char *name, qdownload_t *dl, int reason) { assert(name == dl->remotename && !ccs.download); assert(reason==DLFAIL_SERVERCVAR || reason==DLFAIL_CLIENTFILE); failed++;free(dl); }
static struct { int (*DownloadBegun)(qdownload_t *); void (*DownloadFinished)(qdownload_t *); void (*DownloadFailed)(const char *,qdownload_t *,int); } client_api={begin,finish,fail}, *clientfuncs=&client_api;
static void end_game(const char *format, ...) { (void)format; ended++; }
static struct { void (*EndGame)(const char *,...); } plug_api={end_game}, *plugfuncs=&plug_api;
static void write_file(void *file, const void *bytes, unsigned size) { assert(file==&writes && !memcmp(bytes,"test",size));writes+=size; }
#define VFS_WRITE write_file
#define VFS_TELL(file) (*(int *)(file))
/* ENGINE_FUNCTIONS */
static void start(void)
{
	ccs.download=calloc(1,sizeof(*ccs.download)); assert(ccs.download);
	ccs.download->method=DL_Q3;ccs.download->remotename="mod/test.pk3";ccs.downloadchunknum=0;
	writes=0;commands[0]=0;
}
static void packet(int chunk, int size, int count)
{
	number_index=number_count=data_index=0;
	numbers[number_count++]=chunk;
	if (!chunk) numbers[number_count++]=size;
	if (size!=-1) numbers[number_count++]=count;
	data="test";data_count=count;error_text="Server disallows download";
	CLQ3_ParseDownload();
	assert(number_index==number_count && data_index==data_count);
}
int main(void)
{
	start(); packet(0,4,4); assert(writes==4 && !strcmp(commands,"nextdl 0;"));
	packet(1,4,0); assert(!ccs.download && finished==1 && ccs.servercount==-1);
	assert(!strcmp(commands,"nextdl 0;nextdl 1;donedl;"));
	packet(1,4,0); // repeated EOF after the object was freed
	packet(0,4,4); // unsolicited first chunk must be consumed, never dereferenced
	packet(0,-1,0); // unsolicited error response
	start();packet(0,-1,0);assert(!ccs.download && failed==1 && ended==1);
	start();begin_failure=1;packet(0,4,4);assert(!ccs.download && failed==2);
	puts("Q3 downloads: EOF acknowledgement, released state, duplicate chunks and error responses passed");
	return 0;
}
