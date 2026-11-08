#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
enum { false, true, ca_connected, ca_active, CG_SHUTDOWN, CG_DRAW_ACTIVE_FRAME };
typedef struct { int id; } vm_t;
static vm_t vm={1}, *cgvm=&vm;
static struct { double time; int state; } ccs;
static int frames, shutdowns, destroys, closed, catcher=2;
void CG_Stop(void);
static int get_catcher(void) { return catcher; }
static void set_catcher(int value) { catcher=value; }
#define Q3_GetKeyCatcher get_catcher
#define Q3_SetKeyCatcher set_catcher
static void call(vm_t *context, int command, ...)
{
	assert(context==&vm);
	if (command==CG_SHUTDOWN) {
		assert(!cgvm); shutdowns++;
		CG_Stop(); // error cleanup during VM shutdown must be safe
	} else { assert(command==CG_DRAW_ACTIVE_FRAME); frames++; }
}
static void destroy(vm_t *context) { assert(context==&vm); destroys++; }
static struct { void (*Call)(vm_t *,int,...); void (*Destroy)(vm_t *); } vm_api={call,destroy}, *vmfuncs=&vm_api;
static void colour(float r,float g,float b,float a) { assert(r==1 && g==1 && b==1 && a==1); }
static struct { void (*Colour4f)(float,float,float,float); } draw_api={colour}, *drawfuncs=&draw_api;
static void VM_fcloseall(int owner) { assert(owner==1);closed++; }
/* ENGINE_FUNCTIONS */
int main(void)
{
	ccs.state=ca_connected;
	assert(!CG_Refresh(1) && !frames);
	ccs.state=ca_active;
	assert(CG_Refresh(2) && frames==1 && ccs.time==2);
	CG_Stop(); assert(!cgvm && shutdowns==1 && destroys==1 && closed==1 && !catcher);
	assert(!CG_Refresh(3) && frames==1);
	CG_Stop(); assert(shutdowns==1 && destroys==1);
	puts("Q3 cgame lifecycle: no stale frames during reconnect/downloads; reentrant shutdown is safe");
	return 0;
}
