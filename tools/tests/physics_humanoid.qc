// Original isolated humanoid fixture. All bodies are mod opt-ins; no player physics.
#include "physics-declarations.qh"
#include "physics-model.qh"
float(float skel) skel_get_numbones=#265;
float(float skel,string name) skel_find_bone=#268;
void(float skel) skel_delete=#275;
float(float n) floor=#36;
.entity visible;
.float animskel;
entity first,second,obstacles;
float elapsed,scenario,last_active,modelid;
vector start_hip,initial_hand;
float animation_changed;
void(entity client,string command) stuffcmd=#21;
string(string a,string b,optional string c,optional string d,optional string e) strcat=#115;
entity network_players;
float network_count,network_elapsed,network_phase;
.float capture_id;
void(string label) network_capture = {
    local entity p;
    for(p=network_players;p!=world;p=p.chain)
        stuffcmd(p,strcat("echo HUMAN_NETWORK_",label,";skel_info;screenshot jolt-network-",
                         ftos(p.capture_id),strcat("-",label,".png\n")));
};
void(float ok,string what) verify = {
    if(!ok) {dprint("HUMANOID_FAIL: ");dprint(what);dprint("\n");localcmd("quit force\n");error(what);}
};
void(entity actor) animate_actor = {
    actor.frame=floor(elapsed*10)-floor(elapsed*10/8)*8;
    verify(skel_build(actor.animskel,actor,modelid,0,0,0)>0,"animation skeleton builds");
};
void(entity actor,float active) set_active = {
    local float i;
    local vector before,forward,right,up,after;
    if(active) {
        animate_actor(actor);
        verify(skel_build(actor.skeletonindex,actor,modelid,0,0,0)>0,"simulation pose builds");
        verify(skel_ragupdate(actor,"doll progs/physics-human.doll",actor.animskel)==1,"humanoid rig instantiates");
        // Validate all bone positions and bases, not just the root. This detects
        // incorrect transform order, orient/offset inversion and unweighted bones.
        for(i=1;i<=HUMAN_BONES;i+=1) {
            before=skel_get_boneabs(actor.animskel,i);forward=v_forward;right=v_right;up=v_up;
            after=skel_get_boneabs(actor.skeletonindex,i);
            verify(vlen(before-after)<.05,"no bone translation jump on activation");
            verify(vlen(forward-v_forward)<.02 && vlen(right-v_right)<.02 && vlen(up-v_up)<.02,
                   "no bone orientation jump on activation");
        }
    } else verify(skel_ragupdate(actor,"cleardoll",0)==1,"animation resumes after clearing physics");
};
entity(vector at) spawn_actor = {
    local entity actor=spawn(),shown=spawn();
    actor.classname="humanoid_sim";actor.visible=shown;
    setmodel(actor,HUMAN_MODEL);setmodel(shown,HUMAN_MODEL);
    actor.angles='0 23 0';shown.angles=actor.angles;
    setorigin(actor,at);setorigin(shown,at);
    actor.skeletonindex=skel_create(modelid);actor.animskel=skel_create(modelid);
    shown.skeletonindex=skel_create(modelid);shown.basebone=-1;
    verify(skel_get_numbones(actor.skeletonindex)==HUMAN_BONES,"expected humanoid hierarchy");
    actor.modelindex=0; // Only the relative render skeleton is networked/drawn.
    animate_actor(actor);skel_copybones(shown.skeletonindex,actor.animskel,0,0);
    return actor;
};
void(entity actor) destroy_actor = {
    if(actor==world) return;
    skel_ragupdate(actor,"cleardoll",0);
    skel_delete(actor.skeletonindex);skel_delete(actor.animskel);skel_delete(actor.visible.skeletonindex);
    remove(actor.visible);remove(actor);
};
void(vector at,vector size,vector ang,string modelname) obstacle = {
    local entity e=spawn();e.chain=obstacles;obstacles=e;
    setmodel(e,modelname);e.solid=32;e.geomtype=1;
    setsize(e,size*-.5,size*.5);e.angles=ang;setorigin(e,at);
};
void(float index) reset_scene = {
    local entity old;
    destroy_actor(first);destroy_actor(second);first=second=world;
    while(obstacles!=world) {old=obstacles;obstacles=old.chain;remove(old);}
    scenario=index;elapsed=0;last_active=0;animation_changed=0;
    if(scenario==1) {
        obstacle('-48 0 4','32 80 8','0 0 0',"progs/physics-step0.md5mesh");
        obstacle('-16 0 8','32 80 16','0 0 0',"progs/physics-step1.md5mesh");
        obstacle('16 0 12','32 80 24','0 0 0',"progs/physics-step2.md5mesh");
        obstacle('48 0 16','32 80 32','0 0 0',"progs/physics-step3.md5mesh");
    }
    if(scenario==2) obstacle('0 0 24','160 96 8','0 0 20',"progs/physics-ramp.md5mesh");
    if(scenario==3) obstacle('18 0 48','8 96 96','0 0 0',"progs/physics-wall.md5mesh");
    first=spawn_actor('0 0 100');
    if(scenario==4) second=spawn_actor('4 0 100');
    initial_hand=skel_get_boneabs(first.animskel,skel_find_bone(first.animskel,"Hand_L"));
    start_hip=skel_get_boneabs(first.animskel,skel_find_bone(first.animskel,"Hip"));
};
void(entity actor,float active) update_actor = {
    local float i;
    local vector p,draw,forward,right,up;
    if(actor==world) return;
    if(active) {
        verify(skel_ragupdate(actor,"",actor.animskel)==1,"derive humanoid physics pose");
        skel_copybones(actor.visible.skeletonindex,actor.skeletonindex,0,0);
    } else {
        animate_actor(actor);skel_copybones(actor.visible.skeletonindex,actor.animskel,0,0);
    }
    for(i=1;i<=HUMAN_BONES;i+=1) {
        p=skel_get_boneabs(active?actor.skeletonindex:actor.animskel,i);
        forward=v_forward;right=v_right;up=v_up;
        verify(vlen(p)<1024,"all humanoid bones stay finite and bounded");
        draw=skel_get_boneabs(actor.visible.skeletonindex,i);
        verify(vlen(p-draw)<.05,"relative render pose matches simulated translation");
        verify(vlen(forward-v_forward)<.02 && vlen(right-v_right)<.02 && vlen(up-v_up)<.02,
               "relative render pose matches simulated orientation");
    }
};
void() worldspawn = {
    verify(physics_supported()==1,"Jolt backend is enabled");
    self.dimension_solid=1;self.dimension_hit=1;
    precache_model(HUMAN_MODEL);
    // precache_model returns a string in the classic ABI, so read a modelindex.
    local entity temp=spawn();setmodel(temp,HUMAN_MODEL);modelid=temp.modelindex;remove(temp);
    precache_model("progs/physics-step0.md5mesh");precache_model("progs/physics-step1.md5mesh");
    precache_model("progs/physics-step2.md5mesh");precache_model("progs/physics-step3.md5mesh");
    precache_model("progs/physics-ramp.md5mesh");precache_model("progs/physics-wall.md5mesh");
    reset_scene(0);
};
void() StartFrame = {
    local float active;
    elapsed+=frametime;
    if(cvar("physics_human_network")) {
        if(network_count==2) network_elapsed+=frametime;
        active=network_elapsed>1 && network_elapsed<5;
        if(network_phase==0 && network_elapsed>.5) {network_capture("animation");network_phase=1;}
        if(network_phase==1 && network_elapsed>1.3) {network_capture("falling");network_phase=2;}
        if(network_phase==2 && network_elapsed>4) {network_capture("settled");network_phase=3;}
        if(network_phase==3 && network_elapsed>6) {network_capture("recovered");network_phase=4;}
        if(network_elapsed>8) {dprint("HUMANOID_NETWORK_SERVER_OK\n");localcmd("quit force\n");}
    }
    else if(cvar("physics_human_test")) active=elapsed>.15;
    else {
        local float requested=cvar("physics_human_scenario");
        requested=requested-floor(requested/5)*5;
        if(requested!=scenario) reset_scene(requested);
        active=cvar("physics_human_active");
    }
    if(active!=last_active) {
        set_active(first,active);if(second!=world) set_active(second,active);
        last_active=active;
    }
    update_actor(first,active);update_actor(second,active);
    if(!active) {
        local vector hand=skel_get_boneabs(first.animskel,skel_find_bone(first.animskel,"Hand_L"));
        if(vlen(hand-initial_hand)>.001) animation_changed=1;
    }
    if(!cvar("physics_human_test")) return;
    if(elapsed>.12) verify(animation_changed,"animation changes the skeleton before activation");
    if(elapsed>3) {
        local vector hip=skel_get_boneabs(first.skeletonindex,skel_find_bone(first.skeletonindex,"Hip"));
        verify(hip_z<start_hip_z-20,"humanoid falls from animation pose");
        verify(hip_z+first.origin_z>-8,"humanoid stays above the world floor");
        dprint("HUMANOID_TERRAIN_OK ");dprint(ftos(scenario));dprint("\n");
        if(scenario<4) {reset_scene(scenario+1);return;}
        // Exercise a live rig across backend teardown and then repeat ownership
        // changes to expose stale joint/body handles and delayed skeleton deletion.
        verify(physics_supported(0)==0,"live-rig backend shutdown");
        verify(physics_supported(1)==1,"live-rig backend restart");
        local float i;
        for(i=0;i<12;i+=1) {
            reset_scene(0);set_active(first,1);set_active(first,0);set_active(first,1);
        }
        destroy_actor(first);destroy_actor(second);first=second=world;
        dprint("HUMANOID_LIFECYCLE_OK\n");localcmd("quit force\n");
    }
};
void() PutClientInServer = {
    self.classname="player";self.movetype=8;self.solid=0;self.flags=8;self.health=100;
    self.angles='16 90 0';self.fixangle=1;
    setsize(self,'-16 -16 -24','16 16 32');setorigin(self,'0 -160 90');
    dprint("HUMANOID_CLIENT_READY\n");
    if(cvar("physics_human_network")) {
        self.capture_id=network_count;network_count+=1;
        self.chain=network_players;network_players=self;
    }
    if(cvar("physics_human_capture")) {
        localcmd("defer .5 screenshot jolt-human-animation.png\n");
        localcmd("defer .7 set physics_human_active 1\n");
        localcmd("defer 1 screenshot jolt-human-falling.png\n");
        localcmd("defer 3 screenshot jolt-human-settled.png\n");
        localcmd("defer 3.1 skel_info\n");
        localcmd("defer 3.3 set physics_human_active 0\n");
        localcmd("defer 4 screenshot jolt-human-recovered.png\n");
        localcmd("defer 5 quit force\n");
    }
};
void() info_player_start={};void() main={};
void() PlayerPreThink={};void() PlayerPostThink={};void() ClientKill={};
void() ClientConnect={};void() ClientDisconnect={};void() SetNewParms={};void() SetChangeParms={};
