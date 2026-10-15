/* Actual item-stat HUD updates, including the top inventory bit. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define QUAKESTATS
#define false 0
#define Z_EXT_VIEWHEIGHT 1
#define CP_NETQUAKE 1
enum {STAT_ITEMS, STAT_WEAPONMODELI, STAT_VIEWHEIGHT};
static struct {
    struct {int stats[3]; float statsf[3], item_gettime[32], viewheight;} playerview[2];
    float time;
} cl;
static struct {int z_ext, protocol;} cls;
static int bar_changes;
static void Sbar_Changed(void) {++bar_changes;}
static void TP_ExecTrigger(const char *name, int value) {(void)name;(void)value;}
static void TP_StatChanged(int stat, int value) {(void)stat;(void)value;}
/* ENGINE_FUNCTIONS */
int main(void)
{
    uint32_t state = 0x80000000u;
    for (int i = 0; i < 10000; ++i)
    for (int view = 0; view < 2; ++view)
    {
        state = state * 1664525u + 1013904223u;
        uint32_t old = state;
        state = state * 1664525u + 1013904223u;
        uint32_t now = state;
        cl.playerview[view].stats[STAT_ITEMS] = (int)old;
        cl.time = 123;
        for (int bit = 0; bit < 32; ++bit) cl.playerview[view].item_gettime[bit] = -1;
        bar_changes = 0;
        CL_SetStat_Internal(view,STAT_ITEMS,(int)now,17);
        for (int bit = 0; bit < 32; ++bit)
        {
            int added = ((now / (UINT64_C(1)<<bit)) % 2) && !((old / (UINT64_C(1)<<bit)) % 2);
            assert(cl.playerview[view].item_gettime[bit] == (added ? 123 : -1));
        }
        assert((uint32_t)cl.playerview[view].stats[STAT_ITEMS] == now);
        assert(cl.playerview[view].statsf[STAT_ITEMS] == 17 && bar_changes == (old != now));
        CL_SetStat_Internal(view,STAT_ITEMS,(int)now,17);
        assert(bar_changes == (old != now));
    }
    puts("Item-stat HUD updates: all 32 inventory bits and two player views preserve pickup flash times");
    return 0;
}
