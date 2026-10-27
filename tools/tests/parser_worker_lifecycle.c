/* Delay every thread entry until shutdown waits, reproducing a pending-start race. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
typedef int qboolean;
enum { false, true, WG_MAIN, WG_LOADER, WG_COUNT, WORKERTHREADS=4 };
static struct condition { int locked, destroyed; } conditions[WG_COUNT];
static void *com_workercondition[WG_COUNT], *com_resourcemutex;
static int com_liveworkers[WG_COUNT], com_workeracksequence, com_workererror;
static struct { float value; } worker_sleeptime;
struct com_worker_s {
	void *thread;
	volatile enum { WR_NONE, WR_DIE, WR_ACK } request;
	volatile int ackseq;
};
static struct com_worker_s com_worker[WORKERTHREADS];
static struct pending {
	int (*entry)(void *);
	void *arg;
	int done, joined;
} pending[WORKERTHREADS];
static int fail_create, launched, joined, waits;
static void (*queued[WORKERTHREADS])(void *,void *,size_t,size_t);
static void *contexts[WORKERTHREADS];
static size_t groups[WORKERTHREADS];
static int queued_count;
static void Sys_LockConditional(void *ptr)
{
	struct condition *c=ptr; assert(c && !c->destroyed && !c->locked); c->locked=1;
}
static void Sys_UnlockConditional(void *ptr)
{
	struct condition *c=ptr; assert(c && !c->destroyed && c->locked); c->locked=0;
}
static void Sys_ConditionBroadcast(void *ptr) { assert(ptr && !((struct condition *)ptr)->destroyed); }
static void Sys_Sleep(float seconds) { (void)seconds; assert(!"unexpected sleep"); }
static char *va(const char *format, ...)
{
	static char text[64]; va_list ap; va_start(ap,format); vsnprintf(text,sizeof(text),format,ap);va_end(ap);return text;
}
static void *Sys_CreateThread(const char *name, int (*entry)(void *), void *arg, int priority, int stack)
{
	assert(name && !priority && stack==256*1024);
	assert(conditions[WG_LOADER].locked && com_liveworkers[WG_LOADER]>launched);
	if (fail_create) return NULL;
	struct pending *p=&pending[launched++]; p->entry=entry;p->arg=arg;return p;
}
static void Sys_WaitOnThread(void *ptr)
{
	struct pending *p=ptr;assert(p->done && !p->joined);p->joined=1;joined++;
}
static void COM_AddWork(int group, void (*fn)(void *,void *,size_t,size_t), void *ctx, void *data, size_t a, size_t b)
{
	assert(group==WG_MAIN && !data && !a && queued_count<WORKERTHREADS);
	queued[queued_count]=fn;contexts[queued_count]=ctx;groups[queued_count++]=b;
}
static int COM_DoWork(int group, int locked)
{
	if (group==WG_LOADER || !queued_count) return false;
	assert(locked==conditions[WG_MAIN].locked);
	if (locked) Sys_UnlockConditional(com_workercondition[WG_MAIN]);
	queued_count--;
	queued[queued_count](contexts[queued_count],NULL,0,groups[queued_count]);
	if (locked) Sys_LockConditional(com_workercondition[WG_MAIN]);
	return true;
}
static int COM_HasWork(void) { return queued_count!=0; }
static int Sys_ConditionWait(void *ptr)
{
	assert(ptr==com_workercondition[WG_MAIN]);waits++;
	Sys_UnlockConditional(ptr);
	for (int i=0;i<launched;i++) if (!pending[i].done) {
		assert(((struct com_worker_s *)pending[i].arg)->request==WR_DIE);
		pending[i].entry(pending[i].arg);pending[i].done=1;
	}
	Sys_LockConditional(ptr);return true;
}
static void Sys_DestroyConditional(void *ptr)
{
	struct condition *c=ptr;
	assert(c && !c->locked && joined==launched && !com_liveworkers[WG_LOADER]);
	c->destroyed=1;
}
static void Sys_DestroyMutex(void *ptr) { assert(ptr==&com_resourcemutex && joined==launched); }
static void Con_Printf(const char *text) { (void)text;assert(!"worker joined twice"); }
void COM_WorkerFullSync(void);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
/* ENGINE_FUNCTIONS */
#pragma GCC diagnostic pop
static void init(void)
{
	memset(conditions,0,sizeof(conditions));memset(com_worker,0,sizeof(com_worker));
	memset(pending,0,sizeof(pending)); memset(com_liveworkers,0,sizeof(com_liveworkers));
	for (int i=0;i<WG_COUNT;i++) com_workercondition[i]=&conditions[i];
	com_resourcemutex=&com_resourcemutex;com_liveworkers[WG_MAIN]=1;
	launched=joined=waits=queued_count=0;
}
int main(void)
{
	init();COM_StartWorkerThread(0);COM_StartWorkerThread(1);
	assert(launched==2 && !pending[0].done && com_liveworkers[WG_LOADER]==2);
	COM_DestroyWorkerThread();
	assert(waits && joined==2 && !com_resourcemutex && !com_liveworkers[WG_LOADER]);
	assert(!com_worker[0].thread && !com_worker[1].thread);
	COM_DestroyWorkerThread();assert(joined==2);
	init();fail_create=1;COM_StartWorkerThread(0);
	assert(!com_worker[0].thread && !com_liveworkers[WG_LOADER]);
	COM_DestroyWorkerThread();assert(!waits && !launched);
	puts("Worker lifecycle: pending starts joined before teardown; failed creation and repeated shutdown passed");
	return 0;
}
