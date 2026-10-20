// Original CSQC side: no authoritative entities or player-physics code.
#include "declarations.qh"
float physics_mode;
void(string s) print=#339;
float(float skel) skel_get_numbones=#265;
void(float skel,float bone,vector org,optional vector fwd,optional vector right,optional vector up) skel_set_bone=#271;
void(float skel) skel_delete=#275;
float(float n) fabs=#43;
string(string a,string b,optional string c) strcat=#115;
void() clearscene=#300;
void(entity e) addentity=#302;
float(float property,...) setproperty=#303;
void() renderscene=#304;
entity actor,shown,dropped,masked,recycled,lift,rider;
float source,elapsed,phase,touches,deleted,modelid,last_x,release_z;
float ready,views,simulation_ticks,run_id,capture_mask;
float blend_started,soft_z;
void(string label) capture = {
    if(cvar("csqc_fixture_capture")) localcmd(strcat("screenshot ",strcat(cvar("csqc_fixture_shared")?"csqc-listen-":"csqc-remote-",ftos(run_id),strcat("-",label,".png\n"))));
};
void(float ok,string what) verify = {
    if(!ok) {print("CSQC_PHYSICS_FAIL: ");print(what);print("\n");error(what);}
};
void() contact = {touches+=1;};
void() remove_contact = {deleted+=1;remove(self);};
entity(vector at) prop = {
    local entity e=spawn();e.movetype=32;e.solid=32;e.geomtype=1;e.mass=1;
    setsize(e,'-8 -8 -8','8 8 8');setorigin(e,at);return e;
};
void() make_rig = {
    actor=spawn();shown=spawn();
    setmodel(actor,"progs/physics-rig.md5mesh");setmodel(shown,"progs/physics-rig.md5mesh");
    modelid=actor.modelindex;
    setorigin(actor,'0 0 120');setorigin(shown,actor.origin);
    source=skel_create(modelid);actor.skeletonindex=skel_create(modelid);
    shown.skeletonindex=skel_create(modelid);
    verify(skel_build(source,actor,modelid,0,0,0)>0,"client source builds");
    verify(skel_build(actor.skeletonindex,actor,modelid,0,0,0)>0,"client physics pose builds");
    verify(skel_ragupdate(actor,"doll progs/physics-csqc.doll",source)==1,"client mixed rig instantiates");
    actor.modelindex=0;
};
void() CSQC_Init = {physics_mode=2;print("CSQC_PHYSICS_INIT\n");};
void() CSQC_WorldLoaded = {
    verify(physics_supported()==1,"client backend enabled after world load");
    world.dimension_solid=1;world.dimension_hit=1;
    run_id=cvar("csqc_fixture_run")+1;localcmd(strcat("set csqc_fixture_run ",ftos(run_id),"\n"));
    if(cvar("csqc_fixture_shared")) {
        verify(cvar("csqc_fixture_server_skel")>0,"shared server handle published");
        verify(skel_get_numbones(cvar("csqc_fixture_server_skel"))==0,"client cannot borrow server skeleton");
    }
    precache_model("progs/physics-rig.md5mesh");precache_model("*1");make_rig();
    if(cvar("csqc_fixture_shared")) localcmd(strcat("set csqc_fixture_client_skel ",ftos(actor.skeletonindex),"\n"));
    dropped=prop('-240 0 128');dropped.touch=contact;
    masked=prop('-360 0 128');masked.dimension_solid=2;masked.dimension_hit=2;
    local entity doomed=prop('-480 0 16');doomed.touch=remove_contact;
    local entity old=prop('400 0 300');physics_addforce(old,'100000 0 0',old.origin);remove(old);
    recycled=prop('400 0 300');recycled.gravity=.000001;
    lift=spawn();setmodel(lift,"*1");lift.solid=4;lift.movetype=7;lift.nextthink=1000;
    setorigin(lift,'240 0 64');lift.velocity='0 0 24';
    rider=prop('240 0 80');
    ready=1;
};
void() StartFrame = {
    local vector top,mid,tip,draw;
    local float i;
    if(!ready || phase==7) return;
    elapsed+=frametime;
    if(phase==2) {
        verify(physics_supported()==0,"client backend remains stopped across server ticks");
        if(elapsed>1.6) {
            verify(physics_supported(1)==1,"client backend restarts independently");
            verify(skel_ragupdate(actor,"cleardoll",source)==1,"client clear after backend teardown");
            skel_set_bone(source,1,'0 0 64','1 0 0','0 -1 0','0 0 1');
            verify(skel_ragupdate(actor,"doll progs/physics-csqc.doll",source)==1,"client rig recovers after backend restart");
            phase=3;
        }
        return;
    }
    verify(fabs(frametime-.01)<.00001,"CSQC frametime matches fixed physics tick");
    simulation_ticks+=1;
    if(phase==0) skel_set_bone(source,1,'0 0 64'+elapsed*40*'1 0 0','1 0 0','0 -1 0','0 0 1');
    verify(skel_ragupdate(actor,"",source)==1,"derive client mixed pose");
    top=skel_get_boneabs(actor.skeletonindex,1);mid=skel_get_boneabs(actor.skeletonindex,2);
    tip=skel_get_boneabs(actor.skeletonindex,3);
    skel_copybones(shown.skeletonindex,actor.skeletonindex,0,0);
    for(i=1;i<=3;i+=1) {
        draw=skel_get_boneabs(shown.skeletonindex,i);
        verify(vlen(draw-skel_get_boneabs(actor.skeletonindex,i))<.05,"client render pose matches physics");
    }
    if(phase==0) {
        if(elapsed>.6 && capture_mask==0) {capture_mask=1;capture("mixed");}
        if(elapsed>.1) {
            verify(fabs(top_x-last_x)<.15 && fabs(top_z-64)<.1,"client driven root follows target");
            verify(fabs(vlen(mid-top)-32)<1 && fabs(vlen(tip-mid)-32)<1,"client passive bones remain connected");
        }
        last_x=elapsed*40;
        if(elapsed>1) {
            verify(dropped.origin_z>7 && dropped.origin_z<9 && touches>0,"client contacts and floor collision");
            verify(masked.origin_z<-100,"client dimension filter excludes floor");
            verify(deleted==1,"deferred client touch may safely remove entity");
            verify(fabs(recycled.origin_x-400)<.05,"removed client edict does not receive queued force");
            verify(rider.origin_z>103 && rider.origin_z<105,"client brush lift carries dynamic rider");
            release_z=top_z;
            verify(skel_ragupdate(actor,"animate 0",source)==1,"client rig releases");phase=1;
        }
    } else if(phase==1) {
        if(elapsed>1.2 && capture_mask==1) {capture_mask=2;capture("released");}
        if(elapsed<=1.4) return;
        verify(top_z<release_z-40,"released client rig falls");
        // Shutting down this world must not stop the listen server's Jolt world.
        verify(physics_supported(0)==0,"client backend stops independently");
        phase=2;
    } else if(phase==3 && elapsed>2.1) {
        verify(fabs(top_z-64)<.1,"restarted client animation target is valid");
        verify(views>0 && simulation_ticks>180,"view and physics callbacks both execute");
        capture("restored");
        verify(skel_ragupdate(actor,"animatebody top .5",source)==1,"client fractional strength accepted");
        skel_set_bone(source,1,'80 0 96','1 0 0','0 -1 0','0 0 1');
        blend_started=elapsed;phase=4;
    } else if(phase==4 && elapsed>blend_started+.7) {
        verify(fabs(top_x-80)<2 && top_z>85 && top_z<97,"client fractional motor follows target with gravity compliance");
        verify(fabs(vlen(mid-top)-32)<2 && fabs(vlen(tip-mid)-32)<2,"client fractional rig retains joints");
        capture("blended");soft_z=top_z;
        verify(skel_ragupdate(actor,"animate 0",source)==1,"client fractional motors release");
        blend_started=elapsed;phase=5;
    } else if(phase==5 && elapsed>blend_started+.35) {
        verify(top_z<soft_z-25,"client fractional release falls");capture("blend-released");
        skel_set_bone(source,1,top,'1 0 0','0 -1 0','0 0 1');
        verify(skel_ragupdate(actor,"animatebody top .5",source)==1,"client fractional motor reacquires physical pose");
        blend_started=elapsed;phase=6;
    } else if(phase==6 && elapsed>blend_started+.3) {
        print("CSQC_FRACTIONAL_OK\nCSQC_PHYSICS_OK\n");phase=7;
        localcmd("csqc_next\n");
    }
};
void(float width,float height,float do2d) CSQC_UpdateView = {
    views+=1;
    verify(width>0 && height>0,"client viewport is finite and nonzero");
    clearscene();setproperty(1,'0 0 0');setproperty(4,[width,height,0]);
    setproperty(11,'80 -300 160');setproperty(15,'8 90 0');setproperty(19,1);
    if(ready) {addentity(shown);}
    renderscene();
};
void() CSQC_Shutdown = {
    if(ready) {
        verify(skel_get_numbones(source)==3,"client skeleton valid until shutdown callback");
        // Deliberately leave live bodies, joints and skeletons to native teardown.
        print("CSQC_PHYSICS_SHUTDOWN\n");
    }
};
