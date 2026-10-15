/* Actual MD5 replacement preflight/loader; host services are isolated stubs. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_BONES 256
#define MAX_QPATH 128
#define QDECL
#define true 1
#define false 0
#define SKEL_ABSOLUTE 1
#define mod_alias 2
typedef int qboolean;
typedef enum {TTP_EOF,TTP_STRING,TTP_RAWTOKEN} com_tokentype_t;
static char com_token[1024];
static void COM_AssertMainThread(const char *s) {(void)s;}
#define COM_ParseOut(d,o,l) COM_ParseType(d,o,l,NULL)
typedef struct {int skeltype,numposes; void *boneofs; qboolean loop; float rate; char name[64];} galiasanimation_t;
typedef struct galiasinfo_s {struct galiasinfo_s *nextsurf; int numanimations,numbones; galiasanimation_t *ofsanimations;} galiasinfo_t;
typedef struct {char name[MAX_QPATH],publicname[MAX_QPATH];int memgroup,flags,type,numframes;galiasinfo_t *meshinfo;struct {void (*NativeTrace)(void);} funcs;} model_t;
static struct {int ival;} mod_md5_singleanimation = {1};
static const char *anim_source;
static int opened,closed,mesh_calls,anim_calls,notes,mesh_ok=1;
static void *allocations[8];static int allocated;
static galiasanimation_t base = {SKEL_ABSOLUTE,1,NULL,0,0,"base"};
static galiasinfo_t mesh;
static float poses[24];
static void *FS_LoadMallocFile(const char *name,void *size) {
    (void)size;assert(!strcmp(name,"progs/test.md5anim"));if(!anim_source)return NULL;
    size_t n=strlen(anim_source)+1;void *p=malloc(n);assert(p);memcpy(p,anim_source,n);opened++;return p;
}
static void FS_FreeFile(void *p) {if(p){closed++;free(p);}}
static void *ZG_Malloc(int *group,size_t n) {(void)group;assert(allocated<8);void *p=calloc(1,n);assert(p);allocations[allocated++]=p;return p;}
static void COM_StripAllExtensions(const char *in,char *out,size_t n) {assert(strlen(in)<n);strcpy(out,in);char *p=strchr(out,'.');if(p)*p=0;}
static void Q_strncatz(char *out,const char *in,size_t n) {assert(strlen(out)+strlen(in)<n);strcat(out,in);}
static void Con_DPrintf(const char *fmt,...) {(void)fmt;notes++;}
static void Q_snprintfz(char *out,size_t n,const char *fmt,...) {va_list a;va_start(a,fmt);vsnprintf(out,n,fmt,a);va_end(a);}
static galiasinfo_t *Mod_ParseMD5MeshModel(model_t *m,void *b,char *name) {(void)m;(void)b;(void)name;mesh_calls++;mesh=(galiasinfo_t){NULL,1,1,&base};return mesh_ok?&mesh:NULL;}
static int Mod_ParseMD5Anim(model_t *m,void *b,galiasinfo_t *root,void **np,galiasanimation_t *ng) {(void)m;(void)b;(void)root;anim_calls++;*np=poses;*ng=base;ng->numposes=2;return 1;}
static int Mod_ReadFlagsFromMD1(const char *name,int x) {(void)name;return x;}
static void Mod_ClampModelSize(model_t *m) {(void)m;}
static void Mod_ParseModelEvents(model_t *m,galiasanimation_t *a,int n) {(void)m;(void)a;(void)n;}
static void Mod_Trace(void) {}
/* ENGINE_FUNCTIONS */
static void reset(void) {for(int i=0;i<allocated;i++)free(allocations[i]);allocated=0;opened=closed=mesh_calls=anim_calls=notes=0;mesh_ok=1;mod_md5_singleanimation.ival=1;}
static int load(const char *meshtext,const char *animtext,int direct) {
    model_t m={.name="progs/test.md5mesh",.publicname="progs/test.mdl"};
    if(direct)strcpy(m.publicname,m.name);
    anim_source=animtext;
    int result=Mod_LoadMD5MeshModel(&m,(void *)meshtext,strlen(meshtext));assert(opened==closed);return result;
}
int main(void) {
    const char *mesh55="MD5Version 10 // comment\ncommandline \"numJoints 7\"\nnumJoints 55\njoints { }";
    const char *anim39="MD5Version 10\ncommandline \"\"\nnumFrames 147\nnumJoints 39\nhierarchy { }";
    assert(Mod_MD5JointCount(mesh55)==55 && Mod_MD5JointCount(anim39)==39);
    const char *bad[]={"", "MD5Version", "MD5Version 11 numJoints 3", "MD5Version 10 numJoints", "MD5Version 10 numJoints -1", "MD5Version 10 numJoints 0", "MD5Version 10 numJoints 257", "MD5Version 10 numJoints 9999999999999999999999999999999999", "MD5Version 10 numJoints 3junk", "MD5Version 10 hierarchy { numJoints 55 }"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!Mod_MD5JointCount(bad[i]));
    assert(!load(mesh55,anim39,0));assert(notes==1 && !mesh_calls && !anim_calls);reset();
    assert(!load(mesh55,bad[3],0));assert(notes==1 && !mesh_calls);reset();
    assert(load(mesh55,mesh55,0));assert(mesh_calls==1 && anim_calls==1 && !notes);reset();
    assert(load(mesh55,NULL,0));assert(mesh_calls==1 && !anim_calls && !notes);reset();
    assert(load(mesh55,anim39,1));assert(mesh_calls==1 && anim_calls==1 && !notes);reset();
    mod_md5_singleanimation.ival=0;assert(load(mesh55,anim39,0));assert(!opened && !anim_calls);reset();
    mesh_ok=0;assert(!load(mesh55,mesh55,0));assert(opened==closed && !anim_calls);reset();
    puts("MD5 replacement preflight: incompatible/malformed pairs fall back before mesh parsing; compatible, explicit and static models retain their paths; allocations freed");
}
