// Actual remaster archive detector with bounded memory-backed host file I/O.
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_OSPATH 1024
#define true 1
#define false 0
typedef int qboolean;
typedef uint32_t quint32_t;
typedef uint64_t qofs_t;
typedef struct {const unsigned char *data; size_t size, pos;} vfsfile_t;
static vfsfile_t input;
static int exists, opened, closes;
static int Q_snprintfz(char *out, size_t cap, const char *fmt, ...) {
    va_list args; va_start(args,fmt); int n=vsnprintf(out,cap,fmt,args); va_end(args);
    return n < 0 || (size_t)n >= cap;
}
static vfsfile_t *VFSOS_Open(const char *path, const char *mode) {
    assert(!opened && !strcmp(mode,"rb"));
    assert(!strcmp(path,"test/id1/pak0.pak"));
    if (!exists) return NULL;
    input.pos=0; opened=1; return &input;
}
static size_t read_file(vfsfile_t *f, void *out, size_t n) {
    assert(opened && f->pos <= f->size);
    if(n>f->size-f->pos)n=f->size-f->pos;
    memcpy(out,f->data+f->pos,n); f->pos+=n; return n;
}
static int seek_file(vfsfile_t *f, size_t offset) {
    assert(opened); if(offset>f->size)return 0; f->pos=offset; return 1;
}
static void close_file(vfsfile_t *f) {(void)f;assert(opened);opened=0;closes++;}
#define VFS_GETLEN(f) ((f)->size)
#define VFS_READ(f,p,n) read_file(f,p,n)
#define VFS_SEEK(f,o) seek_file(f,o)
#define VFS_CLOSE(f) close_file(f)
static uint32_t LittleLong(uint32_t value) {
    const unsigned char *b=(const unsigned char *)&value;
    return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
}
/* ENGINE_FUNCTIONS */
static void put32(unsigned char *p,uint32_t n) {
    p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);p[2]=(unsigned char)(n>>16);p[3]=(unsigned char)(n>>24);
}
static void makepak(unsigned char *pak) {
    memset(pak,0,208);memcpy(pak,"PACK",4);put32(pak+4,16);put32(pak+8,192);
    const char *names[]={"mapdb.json","localization/loc_english.txt","progs/player.md5mesh"};
    for(size_t i=0;i<3;i++) {
        unsigned char *entry=pak+16+i*64;strcpy((char *)entry,names[i]);
        put32(entry+56,12+(uint32_t)i);put32(entry+60,1);
    }
}
static int check(unsigned char *pak,size_t size) {
    input.data=pak;input.size=size;int previous=closes;
    int result=FS_QuakeRereleasePak("test");assert(!opened);
    assert(closes==previous+exists);return result;
}
int main(void) {
    unsigned char pak[208];makepak(pak);
    exists=0;assert(!check(pak,sizeof(pak)));exists=1;assert(check(pak,sizeof(pak)));
    for(size_t n=0;n<sizeof(pak);n++)assert(!check(pak,n));
    for(size_t entry=0;entry<3;entry++) {
        makepak(pak);pak[16+entry*64]='!';assert(!check(pak,sizeof(pak)));
        makepak(pak);put32(pak+16+entry*64+56,UINT32_MAX);assert(!check(pak,sizeof(pak)));
        makepak(pak);put32(pak+16+entry*64+60,UINT32_MAX);assert(!check(pak,sizeof(pak)));
        makepak(pak);put32(pak+16+entry*64+60,0);assert(!check(pak,sizeof(pak)));
    }
    makepak(pak);strcpy((char *)pak+16,"mapdb.json.extra");assert(!check(pak,sizeof(pak)));
    uint32_t bad[]={0,1,11,17,207,208,209,UINT32_MAX};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        makepak(pak);put32(pak+4,bad[i]);assert(!check(pak,sizeof(pak)));
        makepak(pak);put32(pak+8,bad[i]);assert(!check(pak,sizeof(pak)));
    }
    uint32_t random=0x12345678;
    for(size_t i=0;i<10000;i++) {
        makepak(pak);random=random*1664525u+1013904223u;put32(pak+4,random);
        random=random*1664525u+1013904223u;put32(pak+8,random);assert(!check(pak,sizeof(pak)));
    }
    makepak(pak);assert(check(pak,sizeof(pak)));
    puts("Remaster detection: complete marker set, classic/mod exclusions, all truncations, malformed offsets/lengths and file cleanup passed");
    return 0;
}
