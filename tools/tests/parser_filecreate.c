/* Production stdio location creation through the real filesystem callback type. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define QDECL
typedef enum {qfalse, qtrue} qboolean;
#define FF_NOTFOUND (0u)
#define FF_FOUND (1u)
typedef struct searchpathfuncs_s searchpathfuncs_t;
typedef struct {
    long long len, offset;
    searchpathfuncs_t *fhandle;
    char rawname[128];
} flocation_t;
struct searchpathfuncs_s {
    /* ENGINE_CREATE_CALLBACK */
};
typedef struct {
    searchpathfuncs_t pub;
    char rootpath[32];
} stdiopath_t;
static qboolean fs_readonly;
static unsigned directories;
static int Q_snprintfz(char *dest, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int count = vsnprintf(dest, size, format, args);
    va_end(args);
    return count < 0 || (size_t)count >= size;
}
static void Sys_mkdir(const char *path)
{
    const char *expected[] = {"/fixture", "/fixture/saves", "/fixture/saves/checkpoint"};
    assert(directories < 3 && !strcmp(path, expected[directories]));
    ++directories;
}
/* ENGINE_FUNCTIONS */
int main(void)
{
    stdiopath_t path = {0};
    strcpy(path.rootpath, "/fixture");
    path.pub.CreateFile = FSSTDIO_CreateLoc;
    qboolean (QDECL *volatile create)(searchpathfuncs_t *, flocation_t *, const char *) = path.pub.CreateFile;
    for (int iteration = 0; iteration < 256; ++iteration)
    {
        flocation_t loc;
        memset(&loc, 0xa5, sizeof(loc));
        fs_readonly = qfalse; directories = 0;
        assert(create(&path.pub, &loc, "saves/checkpoint/info.fsv") == qtrue);
        assert(!loc.len && !loc.offset && loc.fhandle == &path.pub && directories == 3);
        assert(!strcmp(loc.rawname, "/fixture/saves/checkpoint/info.fsv"));
        unsigned char before[sizeof(loc)];
        memcpy(before, &loc, sizeof(loc));
        fs_readonly = qtrue; directories = 0;
        assert(create(&path.pub, &loc, "blocked.fsv") == qfalse);
        assert(!directories && !memcmp(before, &loc, sizeof(loc)));
        char overlong[256]; memset(overlong, 'a', sizeof(overlong)-1); overlong[sizeof(overlong)-1] = 0;
        fs_readonly = qfalse;
        assert(create(&path.pub, &loc, overlong) == qfalse && !directories);
        assert(loc.rawname[sizeof(loc.rawname)-1] == 0);
    }
    puts("Filesystem create callback: exact enum return type, nested locations, readonly rejection, bounds and repeated calls passed");
    return 0;
}
