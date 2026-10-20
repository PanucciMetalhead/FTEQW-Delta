// Original three-bone mixed rig, authored animation and passive followers.
#include "declarations.qh"
void(float skel,float bone,vector org,optional vector fwd,optional vector right,optional vector up) skel_set_bone=#271;
void(float skel) skel_delete=#275;
float(float n) fabs=#43;
entity actor,shown,prop,blocker;
float source,elapsed,phase,previous_x,release_z,recover_x,recover_z;
vector released;
float blend_started,soft_z;
void(float ok,string what) verify = {
    if(!ok) {dprint("MIXED_FAIL: ");dprint(what);dprint("\n");localcmd("quit force\n");error(what);}
};
void(vector root) pose = {skel_set_bone(source,1,root,'1 0 0','0 -1 0','0 0 1');};
void() create_actor = {
    actor=spawn();shown=spawn();
    setmodel(actor,"progs/physics-rig.md5mesh");setmodel(shown,"progs/physics-rig.md5mesh");
    setorigin(actor,'0 0 120');setorigin(shown,actor.origin);
    source=skel_create(actor.modelindex);actor.skeletonindex=skel_create(actor.modelindex);
    shown.skeletonindex=skel_create(actor.modelindex);shown.basebone=-1;
    verify(skel_build(source,actor,actor.modelindex,0,0,0)>0,"source pose builds");
    verify(skel_build(actor.skeletonindex,actor,actor.modelindex,0,0,0)>0,"physics pose builds");
    actor.modelindex=0;
    if(elapsed==0) verify(skel_ragupdate(actor,"doll progs/physics-mixed-invalid.doll",source)==0,"out-of-range authored drive rejected");
    verify(skel_ragupdate(actor,"doll progs/physics-mixed.doll",source)==1,"mixed rig loads");
};
void() destroy_actor = {
    verify(skel_ragupdate(actor,"cleardoll",source)==1,"clear mixed rig");
    skel_delete(source);skel_delete(actor.skeletonindex);skel_delete(shown.skeletonindex);
    remove(actor);remove(shown);
};
void() worldspawn = {
    verify(physics_supported()==1,"backend enabled");self.dimension_solid=1;self.dimension_hit=1;
    precache_model("progs/physics-rig.md5mesh");create_actor();
    // Driven root travels through this independent dynamic prop.
    prop=spawn();prop.movetype=32;prop.solid=32;prop.geomtype=1;prop.mass=1;prop.gravity=.000001;
    setsize(prop,'-4 -4 -10','4 4 10');setorigin(prop,'48 0 184');
};
void() StartFrame = {
    local vector top,mid,tip,draw;
    local float x,i;
    elapsed+=frametime;
    if(phase==0) pose('0 0 64'+elapsed*120*'1 0 0');
    verify(skel_ragupdate(actor,"",source)==1,"derive mixed pose");
    top=skel_get_boneabs(actor.skeletonindex,1);
    mid=skel_get_boneabs(actor.skeletonindex,2);tip=skel_get_boneabs(actor.skeletonindex,3);
    skel_copybones(shown.skeletonindex,actor.skeletonindex,0,0);
    for(i=1;i<=3;i+=1) {
        draw=skel_get_boneabs(shown.skeletonindex,i);
        verify(vlen(draw-skel_get_boneabs(actor.skeletonindex,i))<.05,"render skeleton matches physics");
    }
    verify(vlen(top)<1024 && vlen(mid)<1024 && vlen(tip)<1024,"pose bounded");
    if(phase==0) {
        if(elapsed>.05) {
            verify(vlen(top-'0 0 64'-previous_x*'1 0 0')<.1,"driven root follows previous target");
            verify(fabs(vlen(mid-top)-32)<1 && fabs(vlen(tip-mid)-32)<1,"passive followers remain connected");
        }
        x=elapsed*120;previous_x=x;
        if(elapsed>.7) {
            verify(prop.origin_x>60,"driven bone pushes independent dynamic prop");
            verify(skel_ragupdate(actor,"doll progs/physics-mixed-invalid.doll",source)==0,"invalid authored mask preserves live rig");
            verify(skel_ragupdate(actor,"animate 1.5",source)==0,"out-of-range whole-rig strength rejected");
            verify(skel_ragupdate(actor,"animatebody top nan",source)==0,"nonfinite strength rejected");
            verify(skel_ragupdate(actor,"animatebody top garbage",source)==0,"malformed strength rejected");
            verify(skel_ragupdate(actor,"animatebody top -.5",source)==0,"negative body strength rejected");
            verify(skel_ragupdate(actor,"animatebody mid 0",source)==1,"invalid commands retain driven root");
            phase=1;
        }
    } else if(phase==1 && elapsed>1) {
        release_z=top_z;released=top;
        verify(skel_ragupdate(actor,"animatebody top 0",source)==0,"last driven bone released");
        phase=2;
    } else if(phase==2 && elapsed>1.4) {
        verify(top_z<release_z-40,"released rig falls under gravity");
        verify(vlen(top-released)>40,"release advances physical pose");
        recover_x=top_x;recover_z=top_z;pose(top);
        verify(skel_ragupdate(actor,"animate 1",source)==1,"restore default animation mask");
        verify(skel_ragupdate(actor,"animatebody mid 0",source)==1,"default mask drives only root");
        verify(skel_ragupdate(actor,"animatebody mid 1",source)==2,"second bone takes animation control");
        verify(skel_ragupdate(actor,"animatebody mid 0",source)==1,"second bone returns to physics");
        phase=3;
    } else if(phase==3 && elapsed>1.7) {
        verify(vlen(top-'0 0 1'*recover_z-'1 0 0'*recover_x)<.1,"reacquired root holds physical pose");
        // Mixed rig survives backend teardown and repeated entity/skeleton reuse.
        verify(physics_supported(0)==0,"mixed backend stops");
        verify(physics_supported(1)==1,"mixed backend restarts");
        verify(skel_ragupdate(actor,"cleardoll",source)==1,"clear after backend teardown is idempotent");
        verify(skel_ragupdate(actor,"doll progs/physics-mixed.doll",source)==1,"mixed rig recovers after backend restart");
        for(i=0;i<8;i+=1) {
            destroy_actor();create_actor();
            verify(skel_ragupdate(actor,"animate 0",source)==1,"release default mask");
            verify(skel_ragupdate(actor,"animate 1",source)==1,"restore default mask repeatedly");
        }
        remove(prop);pose('0 0 64');
        verify(skel_ragupdate(actor,"doll progs/physics-blended.doll",source)==1,"authored fractional mask loads");
        verify(skel_ragupdate(actor,"animate 1",source)==1,"authored fractional defaults restored");
        blocker=spawn();blocker.solid=2;blocker.movetype=7;
        setsize(blocker,'-8 -64 -64','8 64 64');setorigin(blocker,'96 0 184');
        pose('128 0 64');
        blend_started=elapsed;phase=4;
    } else if(phase==4 && elapsed>blend_started+.8) {
        verify(top_x>75 && top_x<86,"solid geometry resists fractional animation target");
        verify(fabs(vlen(mid-top)-32)<2 && fabs(vlen(tip-mid)-32)<2,"soft driven rig retains authored joints");
        remove(blocker);
        verify(skel_ragupdate(actor,"animate .75",source)==1,"fractional whole-rig scaling accepted");
        verify(skel_ragupdate(actor,"animatebody mid .25",source)==2,"fractional per-body strength accepted");
        blend_started=elapsed;phase=5;
    } else if(phase==5 && elapsed>blend_started+.8) {
        verify(fabs(top_x-128)<2 && top_z>50 && top_z<65,"fractional drive recovers after blocker removal with gravity compliance");
        verify(skel_ragupdate(actor,"animate 2",source)==0,"invalid strength preserves soft rig");
        verify(skel_ragupdate(actor,"animatebody top inf",source)==0,"nonfinite soft edit rejected");
        soft_z=top_z;
        verify(skel_ragupdate(actor,"animate 0",source)==1,"release all fractional motors");
        blend_started=elapsed;phase=6;
    } else if(phase==6 && elapsed>blend_started+.35) {
        verify(top_z<soft_z-25,"fractional release resumes free physical motion");
        pose(top);
        verify(skel_ragupdate(actor,"animatebody top .5",source)==1,"passive pose can reacquire soft drive");
        verify(physics_supported(0)==0 && physics_supported(1)==1,"restart with live fractional motor");
        verify(skel_ragupdate(actor,"cleardoll",source)==1,"clear after fractional teardown");
        verify(skel_ragupdate(actor,"doll progs/physics-blended.doll",source)==1,"fractional rig restores after backend restart");
        destroy_actor();
        dprint("FRACTIONAL_ENGINE_OK\nMIXED_ENGINE_OK\n");localcmd("mixed_next\n");phase=7;
    }
};
void() info_player_start={};void() main={};void() PutClientInServer={};
void() PlayerPreThink={};void() PlayerPostThink={};void() ClientKill={};
void() ClientConnect={};void() ClientDisconnect={};void() SetNewParms={};void() SetChangeParms={};
