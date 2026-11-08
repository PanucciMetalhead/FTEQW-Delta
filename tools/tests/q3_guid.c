// Exercise the production GUID policy with deterministic key/hash/I/O fixtures.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define Q3CLIENT 1
#define DIGEST_MAXSIZE 32
#define FS_ROOT 0
#define Q_strncpyz(out, text, size) snprintf(out, size, "%s", text)
#ifndef alloca
#define alloca __builtin_alloca
#endif
typedef unsigned char qbyte;
typedef struct { int port; } netadr_t;
typedef int vfsfile_t;
enum { CP_QUAKEWORLD, CP_QUAKE3 };
static struct { const char *string; int ival; } cl_sendguid = {"", 0};
static struct {
	int protocol, istransfer;
	struct { char guidsalt[64]; } ext;
	char guid[64];
} connectinfo;
static vfsfile_t keyfile;
static int keyloads;
static vfsfile_t *FS_OpenVFS(const char *name, const char *mode, int root)
{
	assert(!strcmp(name, "qkey") && !strcmp(mode, "rb") && root == FS_ROOT);
	keyloads++;
	return &keyfile;
}
#define VFS_GETLEN(file) ((void)(file), 2048)
#define VFS_CLOSE(file) ((void)(file))
#define VFS_WRITE(file, data, len) ((void)(file), (void)(data), (void)(len))
static int VFS_READ(vfsfile_t *file, void *data, int len)
{
	assert(file == &keyfile);
	memset(data, 0x91, (size_t)len);
	return len;
}
static int Sys_RandomBytes(void *data, int len)
{
	(void)data; (void)len;
	assert(!"The fixture already supplies a key");
	return 0;
}
static void NET_AdrToString(char *out, size_t size, netadr_t *adr)
{
	snprintf(out, size, "127.0.0.1:%d", adr->port);
}
static void hash_init(void *ctx) { *(uint32_t *)ctx = 2166136261u; }
static void hash_process(void *ctx, const void *data, size_t len)
{
	const unsigned char *bytes = data;
	for (size_t i = 0; i < len; i++) *(uint32_t *)ctx = (*(uint32_t *)ctx ^ bytes[i])*16777619u;
}
static void hash_end(void *digest, void *ctx)
{
	unsigned char *out = digest;
	for (int i = 0; i < 16; i++) out[i] = (unsigned char)(*(uint32_t *)ctx >> ((i%4)*8));
}
static struct {
	size_t contextsize, digestsize;
	void (*init)(void *);
	void (*process)(void *, const void *, size_t);
	void (*terminate)(void *, void *);
} hash_md4 = {sizeof(uint32_t), 16, hash_init, hash_process, hash_end};
static void Base16_EncodeBlock(const void *data, size_t len, char *out, size_t size)
{
	const unsigned char *bytes = data;
	assert(size > len*2);
	for (size_t i = 0; i < len; i++) snprintf(out + i*2, size - i*2, "%02x", bytes[i]);
}

/* ENGINE_FUNCTIONS */

int main(void)
{
	netadr_t first = {27960}, second = {27961};
	char saved[64];
	connectinfo.protocol = CP_QUAKEWORLD;
	assert(!CL_GUIDString(&first));
	connectinfo.protocol = CP_QUAKE3;
	assert(strlen(CL_GUIDString(&first)) == 32);
	strcpy(saved, connectinfo.guid);
	assert(!strcmp(saved, CL_GUIDString(&first)));
	assert(strcmp(saved, CL_GUIDString(&second)));
	assert(keyloads == 1);
	cl_sendguid.string = "0";
	assert(!CL_GUIDString(&first));
	cl_sendguid.string = "1"; cl_sendguid.ival = 1;
	connectinfo.protocol = CP_QUAKEWORLD;
	assert(!strcmp(saved, CL_GUIDString(&first)));
	cl_sendguid.string = "2"; cl_sendguid.ival = 2;
	strcpy(saved, CL_GUIDString(&first));
	assert(!strcmp(saved, CL_GUIDString(&second)));
	cl_sendguid.string = ""; cl_sendguid.ival = 0;
	strcpy(connectinfo.ext.guidsalt, "existing-server-salt");
	strcpy(saved, CL_GUIDString(&first));
	assert(!strcmp(saved, CL_GUIDString(&second)));
	puts("Q3 GUID: default policy, stable per-server IDs, explicit opt-out and existing modes passed");
	return 0;
}
