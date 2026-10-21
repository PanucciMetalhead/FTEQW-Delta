/* Production worker teardown with deliberately delayed worker startup. */
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
typedef int qboolean;
#define false 0
#define true 1
#define WORKERTHREADS 4
enum {WG_MAIN, WG_LOADER, WG_COUNT};
enum {WR_NONE, WR_DIE};
static struct {void *thread; int request;} com_worker[WORKERTHREADS];
static void *com_resourcemutex, *com_workercondition[WG_COUNT];
static int com_liveworkers[WG_COUNT], pending_start, sleeps, destroyed;
static void Sys_LockConditional(void *p) {assert(p);}
static void Sys_UnlockConditional(void *p) {assert(p);}
static void Sys_ConditionBroadcast(void *p) {assert(p);}
static qboolean COM_DoWork(int group, qboolean locked)
{
    (void)locked;
    if(group!=WG_MAIN || pending_start) return false;
    for(int i=0;i<WORKERTHREADS;i++) if(com_worker[i].thread) {
        assert(com_workercondition[WG_MAIN] && com_workercondition[WG_LOADER]);
        assert(com_worker[i].request==WR_DIE);
        com_worker[i].thread=NULL;--com_liveworkers[WG_LOADER];return true;
    }
    return false;
}
static void COM_WorkerFullSync(void)
{
    /* A newly created thread has not entered COM_WorkerThread yet. */
    if(!com_liveworkers[WG_LOADER]) return;
    while(COM_DoWork(WG_MAIN,false)) {}
}
static void Sys_Sleep(double delay)
{
    assert(delay>0 && ++sleeps<10 && pending_start);
    assert(com_workercondition[WG_MAIN] && com_workercondition[WG_LOADER]);
    pending_start=0;
    for(int i=0;i<WORKERTHREADS;i++) if(com_worker[i].thread) ++com_liveworkers[WG_LOADER];
}
static void Sys_DestroyConditional(void *p)
{
    assert(p && !pending_start && !com_liveworkers[WG_LOADER]);
    for(int i=0;i<WORKERTHREADS;i++) assert(!com_worker[i].thread);
    ++destroyed;
}
static void Sys_DestroyMutex(void *p) {assert(p && destroyed==WG_COUNT);}
/* ENGINE_FUNCTIONS */
int main(void)
{
    for(int pass=0;pass<256;pass++) for(int count=0;count<=WORKERTHREADS;count++) {
        int token;
        com_resourcemutex=&token;destroyed=sleeps=0;
        pending_start=count>0;
        for(int g=0;g<WG_COUNT;g++) {com_workercondition[g]=&token;com_liveworkers[g]=0;}
        for(int i=0;i<WORKERTHREADS;i++) {com_worker[i].thread=i<count?&token:NULL;com_worker[i].request=WR_NONE;}
        COM_DestroyWorkerThread();
        assert(!com_resourcemutex && destroyed==WG_COUNT && sleeps==(count>0));
        COM_DestroyWorkerThread(); /* Idempotent after joining all handles. */
    }
    puts("Worker teardown: delayed startup, zero live count, joined handles, condition lifetime and repeated shutdown passed");
    return 0;
}
