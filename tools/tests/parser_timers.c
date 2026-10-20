/* Execute production timers with absent and copied callback payloads. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned live, called[3];
static double realtime;
static void *Z_Malloc(size_t size)
{
    void *p = calloc(1, size); assert(p); ++live; return p;
}
static void Z_Free(void *p) { assert(p && live); --live; free(p); }
/* Enforce glibc's nonnull memcpy contract on macOS too. */
static void *checked_memcpy(void *dest, const void *src, size_t size)
{
    assert(dest && src); return memcpy(dest, src, size);
}
#define memcpy checked_memcpy
#define FTE_Atomic_Insert(head, item, link) ((link) = (head), (head) = (item))
/* ENGINE_TIMER_TYPE */
static cmdtimer_t *cmdtimers;
/* ENGINE_FUNCTIONS */
static void callback(int iarg, void *data)
{
    assert(iarg >= 0 && iarg < 3 && data);
    if (iarg == 2) assert(!strcmp(data, "copied"));
    else assert(!*(char *)data);
    ++called[iarg];
}
int main(void)
{
    for (unsigned run = 0; run < 256; ++run)
    {
        char payload[] = "copied";
        realtime = 10;
        Cmd_AddTimer(0, callback, 0, NULL, 0);
        Cmd_AddTimer(0, callback, 1, payload, 0);
        Cmd_AddTimer(2, callback, 2, payload, strlen(payload));
        memset(payload, 'X', sizeof(payload));
        assert(live == 3);
        Cmd_ExecuteTimers();
        assert(called[0] == run && called[1] == run && called[2] == run);
        realtime = 10.5;
        Cmd_ExecuteTimers();
        assert(called[0] == run+1 && called[1] == run+1 && called[2] == run && live == 1);
        realtime = 12.5;
        Cmd_ExecuteTimers();
        assert(called[2] == run+1 && !cmdtimers && !live);
    }
    puts("Timers: absent/empty payloads, copied bytes and terminators, deadlines and repeated cleanup passed");
    return 0;
}
