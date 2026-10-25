// Original rigs and shared animation resources, resumed through real save files.
#include "declarations.qh"
#include "inline_rig.qh"
void(float skel,float bone,vector org,optional vector fwd,optional vector right,optional vector up) skel_set_bone=#271;
void(float skel) skel_delete=#275;
string(string a,string b) strcat=#115;
entity passive,exact,soft,detached,suspended,shown,blocker,companion;
float source,elapsed,phase,resume_time,source_changed;
vector saved_passive,saved_soft,saved_tip,saved_spring,released;
void(float ok,string what) verify = {
    if(!ok) {dprint("SAVE_RIG_FAIL: ");dprint(what);dprint("\n");localcmd("quit force\n");error(what);}
};
void(vector at) drive_pose = {skel_set_bone(source,1,at,'1 0 0','0 -1 0','0 0 1');};
entity(vector at,string command,float animation) rig = {
    local entity e=spawn();setmodel(e,"progs/physics-rig.md5mesh");setorigin(e,at);
    e.skeletonindex=skel_create(e.modelindex);
    verify(skel_build(e.skeletonindex,e,e.modelindex,0,0,0)>0,"build saved physics skeleton");
    verify(skel_ragupdate(e,command,animation)==1,"create saved rig");return e;
};
void() worldspawn = {
    // Missing-backend loads must reach the engine's checkpoint rejection path.
    // Creating rigs here would raise unrelated QC errors before that check.
    if(!physics_supported()) return;
    self.dimension_solid=1;self.dimension_hit=1;precache_model("progs/physics-rig.md5mesh");
    shown=spawn();setmodel(shown,"progs/physics-rig.md5mesh");
    source=skel_create(shown.modelindex);shown.skeletonindex=skel_create(shown.modelindex);shown.basebone=-1;
    verify(skel_build(source,shown,shown.modelindex,0,0,0)>0,"shared animation source builds");
    verify(skel_build(shown.skeletonindex,shown,shown.modelindex,0,0,0)>0,"render skeleton builds");
    passive=rig('-800.123456 0 800',"doll progs/physics-rig.doll",0);
    exact=rig('0 0 256',"doll progs/physics-saved-mixed.doll",source);
    soft=rig('400 0 256',strcat("dollstring ",CHECKPOINT_DOLL),source);
    detached=rig('-400 0 1400',"doll progs/physics-saved-mixed.doll",source);
    verify(skel_ragupdate(detached,"enablejoint link1 0",source)==1,"disable saved joint by name");
    suspended=rig('0 400 512',"doll progs/physics-rig-suspension.doll",0);
    blocker=spawn();blocker.solid=2;blocker.movetype=7;
    setsize(blocker,'-8 -64 -64','8 64 64');setorigin(blocker,'496 0 320');
    companion=spawn();companion.movetype=32;companion.solid=32;companion.geomtype=1;companion.mass=2;
    setsize(companion,'-8 -8 -8','8 8 8');setorigin(companion,'900 0 256');
    drive_pose('128 0 64');
};
void() update = {
    local float i;
    verify(skel_ragupdate(passive,"",0)==1,"derive restored passive rig");
    verify(skel_ragupdate(exact,"",source)==1,"derive restored exact rig");
    verify(skel_ragupdate(soft,"",source)==1,"derive restored fractional rig");
    verify(skel_ragupdate(detached,"",source)==1,"derive restored disabled-joint rig");
    verify(skel_ragupdate(suspended,"",0)==1,"derive restored suspension rig");
    skel_copybones(shown.skeletonindex,soft.skeletonindex,0,0);
    for(i=1;i<=3;i+=1)verify(vlen(skel_get_boneabs(shown.skeletonindex,i)-skel_get_boneabs(soft.skeletonindex,i))<.05,"restored render copy matches physics");
};
void() StartFrame = {
    local vector p,s,t,h;
    if(!physics_supported()) return;
    elapsed+=frametime;
    if(phase==1 && !source_changed && elapsed>resume_time+.08) {drive_pose('256 0 64');source_changed=1;}
    if(phase==1 && cvar("checkpoint_loaded")==1) {
        verify(vlen(skel_get_boneabs(source,1)-'128 0 64')<.001,"shared animation handle/pose restored before callbacks");
        verify(vlen(skel_get_boneabs(passive.skeletonindex,1)-saved_passive)<.1,"passive render pose restored");
        verify(vlen(skel_get_boneabs(soft.skeletonindex,1)-saved_soft)<.1,"fractional render pose restored");
        verify(vlen(skel_get_boneabs(suspended.skeletonindex,1)-saved_spring)<.1,"suspension render pose restored");
        resume_time=elapsed;phase=2;
    } else if(phase==4 && cvar("checkpoint_loaded")==2) {
        verify(vlen(skel_get_boneabs(soft.skeletonindex,1)-saved_soft)<.1,"second save retains recovered rig pose");
        verify(skel_ragupdate(soft,"enablejoint link0 0",source)==1 && skel_ragupdate(soft,"enablejoint link0 1",source)==1,"restored inline rig retains named joints");
        drive_pose('192 0 64');resume_time=elapsed;phase=5;
    }
    update();p=skel_get_boneabs(passive.skeletonindex,1);s=skel_get_boneabs(soft.skeletonindex,1);
    t=skel_get_boneabs(detached.skeletonindex,3);h=skel_get_boneabs(suspended.skeletonindex,1);
    // A local full client may forward save to its server one tick later.
    // Keep the QC observations aligned with the pose actually serialized.
    if(phase==1 && cvar("checkpoint_loaded")!=1) {
        saved_passive=p;saved_soft=s;saved_tip=t;saved_spring=h;
    }
    if(phase==4 && cvar("checkpoint_loaded")!=2) saved_soft=s;
    if(!phase && elapsed>1.1) {
        verify(s_x>75 && s_x<87,"soft target resisted before saving");
        verify(vlen(skel_get_boneabs(exact.skeletonindex,1)-'128 0 64')<.1,"exact target before saving");
        saved_passive=p;saved_soft=s;saved_tip=t;saved_spring=h;
        resume_time=elapsed;phase=1;localcmd("save physics-resume-a\ndefer .3 resume_load_a\n");
    } else if(phase==2 && elapsed>resume_time+.25) {
        verify(p_z<saved_passive_z-20,"passive rig resumes falling with saved momentum");
        verify(s_x>75 && s_x<87,"fractional control resumes against existing wall");
        verify(t_z<saved_tip_z-20,"disabled joint stays disabled after loading");
        verify(vlen(h-saved_spring)<.2,"suspension retains original world rest frame after loading");
        verify(vlen(skel_get_boneabs(exact.skeletonindex,1)-'128 0 64')<.1,"kinematic control retains original target");
        remove(blocker);resume_time=elapsed;phase=3;
    } else if(phase==3 && elapsed>resume_time+.8) {
        verify(s_x>125 && s_x<131 && s_z>58 && s_z<66,"restored soft motor recovers after wall removal");
        verify(skel_ragupdate(soft,"animatebody mid .25",source)==2,"restored inline rig accepts per-body fractional edit");
        verify(skel_ragupdate(soft,"",source)==1,"derive pose after changing animation mask");
        saved_soft=skel_get_boneabs(soft.skeletonindex,1);resume_time=elapsed;phase=4;localcmd("save physics-resume-b\ndefer .3 resume_load_b\n");
    } else if(phase==5 && elapsed>resume_time+.8) {
        verify(vlen(skel_get_boneabs(exact.skeletonindex,1)-'192 0 64')<.1,"restored source drives exact rig to new target");
        verify(s_x>185 && s_x<199,"restored source drives fractional rig to new target");
        released=s;verify(skel_ragupdate(soft,"animate 0",source)==1,"restored fractional rig releases");
        resume_time=elapsed;phase=6;
    } else if(phase==6 && elapsed>resume_time+.3) {
        verify(s_z<released_z-20,"released restored rig continues passive simulation");
        verify(physics_supported(0)==0 && physics_supported(1)==1,"restored worlds teardown and restart safely");
        verify(skel_ragupdate(soft,"cleardoll",source)==1,"clear restored rig after backend restart");
        dprint("SAVE_RIG_OK\n");phase=7;localcmd("quit force\n");
    }
};
void() main={};void() PlayerPreThink={};void() PlayerPostThink={};void() ClientKill={};
void() ClientConnect={};void() PutClientInServer={self.health=100;self.movetype=8;};void() ClientDisconnect={};
void() SetNewParms={};void() SetChangeParms={};
