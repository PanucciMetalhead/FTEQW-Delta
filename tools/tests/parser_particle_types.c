/* Actual effect-index lookup/fallback prefix, before the particle simulation. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#define FALLBACKBIAS 1024
#define false 0
typedef float vec3_t[3];
typedef int trailkey_t;
typedef struct {int marker;} part_type_t;
static part_type_t *part_type;
static int numparticletypes;
typedef struct {trailkey_t fallbackkey;} trailstate_t;
static int fallback_index,fallback_calls;
static trailstate_t trail;
static trailstate_t *P_FetchTrailstate(trailkey_t *tk) {return tk?&trail:NULL;}
static int delegate(vec3_t org,vec3_t dir,float count,int index,trailkey_t *tk) {
    assert(!org && !dir && count==2 && !tk);fallback_index=index;fallback_calls++;return 7;
}
static int delegate_trail(vec3_t startpos,vec3_t end,int type,float timeinterval,int dlkey,vec3_t axis[3],trailkey_t *tk) {
    assert(!startpos && !end && timeinterval==2 && dlkey==3 && !axis);
    assert(!tk || tk==&trail.fallbackkey);fallback_index=type;return 8;
}
static struct {int (*ParticleTrail)(vec3_t,vec3_t,int,float,int,vec3_t[3],trailkey_t *);int (*RunParticleEffectState)(vec3_t,vec3_t,float,int,trailkey_t *);} delegate_api={delegate_trail,delegate},*fallback;
static int lookup(vec3_t org,vec3_t dir,float count,int typenum,trailkey_t *tk) {
    (void)tk;
    /* ENGINE_PARTICLE_LOOKUP */
    return ptype->marker;
}
static int trail_lookup(vec3_t startpos,vec3_t end,int type,float timeinterval,int dlkey,vec3_t axis[3],trailkey_t *tk) {
    /* ENGINE_TRAIL_LOOKUP */
    return ptype->marker;
}
static int query_lookup(int typenum) {
    /* ENGINE_QUERY_LOOKUP */
    return ptype->marker;
}
int main(void) {
    int bad[]={INT_MIN,-100,-1,0,1,INT_MAX};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        assert(lookup(NULL,NULL,2,bad[i],NULL)==1);
        assert(trail_lookup(NULL,NULL,bad[i],2,3,NULL,NULL)==1);
        assert(!query_lookup(bad[i]));
    }
    fallback=&delegate_api;
    assert(lookup(NULL,NULL,2,FALLBACKBIAS+3,NULL)==7 && fallback_index==3 && fallback_calls==1);
    assert(lookup(NULL,NULL,2,INT_MAX,NULL)==7 && fallback_index==INT_MAX-FALLBACKBIAS);
    assert(lookup(NULL,NULL,2,-1,NULL)==1);
    trailkey_t key=0;
    assert(trail_lookup(NULL,NULL,FALLBACKBIAS+4,2,3,NULL,&key)==8 && fallback_index==4);
    assert(trail_lookup(NULL,NULL,FALLBACKBIAS+5,2,3,NULL,NULL)==8 && fallback_index==5);
    fallback=NULL;part_type_t effects[]={{42},{43}};part_type=effects;numparticletypes=2;
    assert(lookup(NULL,NULL,2,0,NULL)==42 && lookup(NULL,NULL,2,1,NULL)==43);
    assert(query_lookup(1)==43 && !query_lookup(-1));
    assert(trail_lookup(NULL,NULL,0,2,3,NULL,NULL)==42);
    assert(lookup(NULL,NULL,2,2,NULL)==1 && lookup(NULL,NULL,2,-1,NULL)==1);
    puts("Particle lookup: absent type table, negative/oversized indices and fallback delegation validated before pointer arithmetic; valid effects preserved");
}
