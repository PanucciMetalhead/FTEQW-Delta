// Original brush-pusher fixture; no base game code or owned assets.
#include "declarations.qh"
.float friction;
entity lift,reference,refbody,rider,rig,door,doorprop,rotator,rotateprop,blocker;
float elapsed,stage,active,blocked_count;
vector rig_start;
void(float ok,string why) verify={
    if(!ok) {dprint("KINEMATIC_FAIL: ");dprint(why);dprint("\n");localcmd("quit force\n");error(why);}
};
entity(vector at) platform={
    local entity e=spawn();setmodel(e,"*1");e.solid=4;e.movetype=7;e.friction=1;e.nextthink=10000;
    setorigin(e,at);return e;
};
entity(vector at,float movement) crate={
    local entity e=spawn();e.solid=2;e.movetype=movement;e.geomtype=1;e.mass=2;e.friction=1;
    setsize(e,'-8 -8 -8','8 8 8');setorigin(e,at);return e;
};
void() door_blocked={blocked_count=blocked_count+1;};
void() worldspawn={
    active=cvar("kinematic_test");elapsed=0;stage=0;
    self.dimension_solid=1;self.dimension_hit=1;
    verify(physics_supported()==active,"plugin selection");
    precache_model("progs/physics-rig.md5mesh");
    reference=platform('-500 0 64');refbody=crate('-500 0 80',6);
    if(!active)return;
    lift=platform('0 0 64');rider=crate('0 0 80',32);
    rig=spawn();setmodel(rig,"progs/physics-rig.md5mesh");setorigin(rig,'30 0 82');
    rig.skeletonindex=skel_create(rig.modelindex);skel_build(rig.skeletonindex,rig,rig.modelindex,0,0,0);
    verify(skel_ragupdate(rig,"doll progs/physics-rig.doll",0)==1,"rig creation");rig.modelindex=0;
    door=platform('250 350 64');door.blocked=door_blocked;
    blocker=crate('350 350 64',5); // Deliberately trapped between the door and a static brush.
    local entity wall=platform('480 350 64');wall.movetype=0;
    doorprop=crate('250 350 80',32); // Rider must stop with accepted door motion.
    rotator=platform('500 -400 64');rotateprop=crate('560 -320 64',32);rotateprop.gravity=.000001;
};
void() StartFrame={
    elapsed=elapsed+frametime;
    if(!stage && elapsed>1.5) {
        reference.velocity='0 0 32';
        if(active) {
            verify(rider.origin_z>79 && rider.origin_z<81,"rider rests on brush");
            physics_enable(rider,0);lift.velocity='0 0 32';
            skel_ragupdate(rig,"",0);rig_start=skel_get_boneabs(rig.skeletonindex,3);
            door.velocity='64 0 0';rotator.avelocity='0 60 0';
        }
        stage=1;
    }
    if(stage==1 && elapsed>3) {
        reference.velocity='0 0 0';
        dprint("KINEMATIC_CLASSIC_Z ");dprint(ftos(refbody.origin_z));dprint("\n");
        verify(refbody.origin_z>125 && refbody.origin_z<130,"classic lift carrying retained");
        if(active) {
            lift.velocity='0 0 0';rotator.avelocity='0 0 0';door.velocity='0 0 0';
            verify(rider.origin_z>125 && rider.origin_z<130,"Jolt lift carries sleeping prop without double movement");
            skel_ragupdate(rig,"",0);
            local vector moved=skel_get_boneabs(rig.skeletonindex,3);
            verify(moved_z>rig_start_z+35,"lift carries skeletal bodies");
            verify(vlen(rotateprop.origin-'560 -320 64')>10,"rotating brush pushes prop");
            verify(blocked_count>0,"classic blocking rejects door motion");
            verify(vlen(doorprop.velocity)<2,"blocked door does not create spurious impulse");
            remove(lift);
        }
        stage=2;
    }
    if(stage==2 && elapsed>3.6) {
        if(active)verify(rider.origin_z<100,"deleted lift releases rider");
        dprint("KINEMATIC_ENGINE_OK\n");stage=3;localcmd("kinematic_next\n");
    }
};
void() main={};void() info_player_start={};void() PlayerPreThink={};void() PlayerPostThink={};
void() ClientKill={};void() ClientConnect={};void() PutClientInServer={};void() ClientDisconnect={};
void() SetNewParms={};void() SetChangeParms={};
