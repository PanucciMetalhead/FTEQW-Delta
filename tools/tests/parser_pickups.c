/* Actual pickup-sound trigger handling with local, remote and spectator seats. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef float vec3_t[3];
typedef int qbool;
typedef struct {char *string;} cvar_t;
typedef struct {cvar_t *cvar; int itemflag;} item_t;
typedef struct {int spectator, stats[2]; vec3_t simorg;} playerview_t;
static struct {playerview_t playerview[4]; int validsequence, oldvalidsequence;} cl;
static struct {int framecount;} cls;
static struct {int stat_framecounts[2], items, olditems;} vars;
enum {STAT_ITEMS,STAT_ARMOR};
enum {it_quad=1,it_pent,it_ring,it_suit,it_health,it_mh,it_weapons,it_lg,it_rl,it_gl,it_sng,it_ng,it_ssg,it_armor,it_ga,it_ya,it_ra,it_ammo=32,it_pack=64,it_runes=128};
enum {IT_LIGHTNING=1,IT_ROCKET_LAUNCHER=2,IT_GRENADE_LAUNCHER=4,IT_SUPER_NAILGUN=8,IT_NAILGUN=16,IT_SUPER_SHOTGUN=32};
static cvar_t tp_name_quad={"quad"},tp_name_pent={"pent"},tp_name_ring={"ring"},tp_name_suit={"suit"},tp_name_health={"health"},tp_name_mh={"mega"},tp_name_lg={"lg"},tp_name_rl={"rl"},tp_name_gl={"gl"},tp_name_sng={"sng"},tp_name_ng={"ng"},tp_name_ssg={"ssg"},tp_name_ga={"ga"},tp_name_ya={"ya"},tp_name_ra={"ra"};
#define countof(a) (sizeof(a)/sizeof((a)[0]))
static int taken, lastseat;
static int FindNearestItem(vec3_t origin, int flags, item_t **item) {(void)origin;(void)flags;*item=NULL;return -1;}
static void TP_ItemTaken(char *name,int flags,vec3_t origin,int number,item_t *item,int seat) {
    (void)name;(void)flags;(void)origin;(void)number;(void)item;++taken;lastseat=seat;
}
/* ENGINE_FUNCTIONS */
int main(void)
{
    vec3_t position={1,2,3};
    int seats[]={-100,-1,0,1,2,3,4,999};
    for (unsigned int i=0;i<countof(seats);++i) {
        taken=0;
        TP_CheckPickupSound("items/damage.wav",position,seats[i]);
        assert(taken==1 && lastseat==(seats[i]>=0 && seats[i]<4 ? seats[i] : -1));
        taken=0;
        TP_CheckPickupSound(NULL,position,seats[i]);
        TP_CheckPickupSound("ambient/water1.wav",position,seats[i]);
        assert(taken==0);
    }
    cl.playerview[0].spectator=1;
    taken=0;
    TP_CheckPickupSound("items/health1.wav",position,0);
    assert(taken==1 && lastseat==-1);
    cl.validsequence=cl.oldvalidsequence=1;
    cls.framecount=vars.stat_framecounts[STAT_ITEMS]=42;
    vars.items=IT_ROCKET_LAUNCHER;
    taken=0;
    TP_CheckPickupSound("weapons/pkup.wav",position,-1);
    assert(taken==0); /* An unknown remote pickup cannot use a local view's fallback. */
    TP_CheckPickupSound("weapons/pkup.wav",position,1);
    assert(taken==1 && lastseat==1);
    puts("Pickup sounds: remote/invalid seats and spectators avoid local view access; local triggers retain attribution");
    return 0;
}
