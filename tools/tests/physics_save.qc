#include "declarations.qh"
float elapsed,phase,resume_time,restheight,blocked_skeleton;
entity falling,slider,sliderjoint,suspension,springjoint,sleeping,actor,pointbody,pointjoint,hull,unlimited,freejoint;
void(float ok,string text) verify = {
    if(!ok) {dprint("SAVE_PHYSICS_FAIL: ");dprint(text);dprint("\n");localcmd("quit force\n");error(text);}
};
entity(vector at) prop = {
    local entity e=spawn();e.movetype=32;e.solid=32;e.geomtype=1;e.mass=2;
    setsize(e,'-8 -8 -8','8 8 8');setorigin(e,at);return e;
};
void() worldspawn = {
    self.dimension_solid=1;self.dimension_hit=1;
    falling=prop('0 0 1800');falling.velocity='32 0 0';falling.avelocity='20 30 40';
    slider=prop('200.123456 0 256');slider.gravity=.000001;
    sliderjoint=spawn();sliderjoint.jointtype=3;sliderjoint.aiment=slider;sliderjoint.movedir='80 -100000 40';
    setorigin(sliderjoint,slider.origin);
    suspension=prop('400.123456 0 256');springjoint=spawn();springjoint.jointtype=5;springjoint.aiment=suspension;
    springjoint.angles='-90 0 0';springjoint.velocity='1 0 0';setorigin(springjoint,suspension.origin);
    springjoint.joint_suspension_stiffness=2000;springjoint.joint_suspension_damping=150;
    sleeping=prop('600 0 256');
    precache_model("progs/physics-rig.md5mesh");
    pointbody=prop('800.123456 0 256');pointbody.gravity=.000001;
    pointjoint=spawn();pointjoint.jointtype=1;pointjoint.aiment=pointbody;
    setorigin(pointjoint,pointbody.origin);pointjoint.velocity='840.123456 0 256';
    hull=prop('1000 0 512');setmodel(hull,"progs/physics-rig.md5mesh");hull.geomtype=12;
    hull.avelocity='0 20 0';
    // A modeled box still has modelindex 0 in the collision bridge. Default
    // slider limits use the engine's large finite unlimited-range sentinel.
    unlimited=prop('1200 0 256');setmodel(unlimited,"progs/physics-rig.md5mesh");
    setsize(unlimited,'-8 -8 -8','8 8 8');unlimited.gravity=.000001;
    freejoint=spawn();freejoint.jointtype=3;freejoint.aiment=unlimited;setorigin(freejoint,unlimited.origin);
};
void() StartFrame = {
    elapsed+=frametime;
    if(!phase && elapsed>.1) {restheight=sleeping.origin_z;physics_enable(sleeping,0);phase=.5;}
    else if(phase==.5 && elapsed>1.1) {
        verify(slider.origin_x>240.05 && slider.origin_x<240.2,"slider at initial limit");
        verify(suspension.origin_z>255.1 && suspension.origin_z<255.3,"initial spring rest height");
        verify(vlen(pointbody.origin-'840.123456 0 256')<.1,"legacy point reaches world anchor");
        phase=1;localcmd("save physics-resume-a\ndefer .3 resume_load_a\n");
    } else if(phase==1 && cvar("checkpoint_loaded")==1) {
        verify(falling.origin_z>1200 && falling.origin_z<1400,"falling body returns to saved region");
        verify(vlen(falling.avelocity)>10,"angular motion restored");
        verify(vlen(sleeping.origin-[600,0,restheight])<.02,"sleep preserved before first resumed tick");
        physics_enable(slider,1);physics_enable(suspension,1);physics_enable(pointbody,1);
        resume_time=time;phase=2;
    } else if(phase==2 && time>resume_time+.5) {
        verify(slider.origin_x>240.05 && slider.origin_x<240.2,"loaded slider retains original frame");
        verify(vlen(pointbody.origin-'840.123456 0 256')<.1,"legacy point retains its original local frame");
        verify(hull.origin_z<50,"loaded convex model retains collision geometry");
        verify(suspension.origin_z>255.1 && suspension.origin_z<255.3,"loaded spring retains original rest height");
        verify(vlen(sleeping.origin-[600,0,restheight])<.02,"sleep remains stable after loading");
        actor=spawn();setmodel(actor,"progs/physics-rig.md5mesh");setorigin(actor,'0 600 256');
        actor.skeletonindex=skel_create(actor.modelindex);skel_build(actor.skeletonindex,actor,actor.modelindex,0,0,0);
        verify(skel_ragupdate(actor,"doll progs/physics-rig.doll",0)==1,"create rig with invalid ownership for save refusal");
        blocked_skeleton=actor.skeletonindex;actor.skeletonindex=0;
        phase=3;localcmd("save physics-resume-a\nset checkpoint_refused 1\n");
    } else if(phase==3 && cvar("checkpoint_refused")==1) {
        actor.skeletonindex=blocked_skeleton;
        verify(skel_ragupdate(actor,"cleardoll",0)==1,"clear unbound rig after refused save");remove(actor);
        phase=4;localcmd("save physics-resume-b\ndefer .3 resume_load_b\n");
    } else if(phase==4 && cvar("checkpoint_loaded")==2) {physics_enable(slider,1);physics_enable(suspension,1);physics_enable(pointbody,1);resume_time=time;phase=5;}
    else if(phase==5 && time>resume_time+.4) {
        verify(slider.origin_x>240.05 && slider.origin_x<240.2,"second load retains original slider frame");
        verify(vlen(unlimited.origin-'1200 0 256')<.1,"unlimited slider and modeled primitive restore");
        verify(vlen(pointbody.origin-'840.123456 0 256')<.1,"second load retains legacy point frame");
        verify(suspension.origin_z>255.1 && suspension.origin_z<255.3,"second load retains spring rest height");
        verify(vlen(sleeping.origin-[600,0,restheight])<.02,"second load retains sleep");
        dprint("SAVE_ENGINE_OK\n");phase=6;localcmd("quit force\n");
    }
};
void() main={};void() PlayerPreThink={};void() PlayerPostThink={};void() ClientKill={};
void() ClientConnect={};void() PutClientInServer={self.health=100;self.movetype=8;};void() ClientDisconnect={};
void() SetNewParms={};void() SetChangeParms={};
