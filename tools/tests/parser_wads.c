/* Actual WAD list handling; filesystem stubs reject any overlong lookup. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define true 1
#define false 0
#define MAX_QPATH 128
#define DLLF_REQUIRED 1
#define fg_halflife 1
#define mod_heightmap 2
typedef int qboolean;
typedef struct {int fromgame,type;} model_t;
static model_t world;
static struct {model_t *worldmodel;} cl={&world};
static char wads[4096];
static int downloads,loads,queries,flushes;
static char existsname[MAX_QPATH]="root.wad";
static char loaded[4][MAX_QPATH];
static int COM_FCheckExists(const char *name) {assert(strlen(name)<MAX_QPATH);queries++;return !strcmp(name,existsname);}
static void CL_CheckOrEnqueDownloadFile(const char *remote,const char *local,int flags) {assert(!strcmp(remote,local));assert(flags==DLLF_REQUIRED);assert(strlen(remote)<MAX_QPATH);downloads++;}
static void Q_strncpyz(char *out,const char *in,size_t n) {assert(strlen(in)<n);strcpy(out,in);}
static void W_LoadTextureWadFile(const char *name,int complain) {assert(!complain && strlen(name)<MAX_QPATH);assert(loads<4);strcpy(loaded[loads++],name);}
static void Wads_Flush(void) {flushes++;}
/* ENGINE_FUNCTIONS */
static void run(int half,int height,size_t badlen) {
    world.fromgame=half?fg_halflife:0;world.type=height?mod_heightmap:0;
    downloads=loads=queries=flushes=0;
    strcpy(wads,"C:\\build\\root.wad;C:\\build\\");size_t start=strlen(wads);
    memset(wads+start,'x',badlen);strcpy(wads+start+badlen,".wad;..\\..\\clip.wad");
    assert(Wad_NextDownload());assert(flushes==1);
    assert(loads==2 && !strcmp(loaded[0],"root.wad") && !strcmp(loaded[1],"textures/clip.wad"));
    assert(downloads==(half||height));
}
int main(void) {
    for(size_t n=MAX_QPATH;n<3800;n+=137){run(0,0,n);run(1,0,n);run(0,1,n);}
    strcpy(wads,";;C:\\\\;root.wad;");world.fromgame=0;world.type=0;loads=queries=downloads=0;
    assert(Wad_NextDownload());assert(loads==1 && !strcmp(loaded[0],"root.wad"));
    /* A valid maximum-length root basename remains usable without prefixes. */
    memset(wads,'a',MAX_QPATH-1);wads[MAX_QPATH-1]=0;strcpy(existsname,wads);
    loads=queries=0;assert(Wad_NextDownload());assert(loads==1 && queries==1 && !strcmp(loaded[0],existsname));
    /* Missing names must also fit the textures/ prefix before that lookup. */
    existsname[0]=0;
    for(size_t n=MAX_QPATH-10;n<MAX_QPATH;n++) {
        memset(wads,'a',n);wads[n]=0;loads=queries=0;assert(Wad_NextDownload());
        assert(queries==1 && loads==(n+strlen("textures/")<MAX_QPATH));
    }
    wads[0]=0;assert(Wad_NextDownload());
    puts("WAD metadata: oversized build basenames skipped, later valid entries/root/textures lookups and Half-Life/heightmap downloads preserved");
}
