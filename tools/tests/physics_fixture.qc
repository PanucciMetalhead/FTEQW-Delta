// Original asset-free SSQC test. Public NQ ABI declarations, no game code.
#pragma target fte
#pragma noref 1
entity self, other, world;
float time, frametime, force_retouch;
string mapname;
float deathmatch, coop, teamplay, serverflags;
float total_secrets, total_monsters, found_secrets, killed_monsters;
float parm1,parm2,parm3,parm4,parm5,parm6,parm7,parm8;
float parm9,parm10,parm11,parm12,parm13,parm14,parm15,parm16;
vector v_forward,v_up,v_right;
float trace_allsolid,trace_startsolid,trace_fraction;
vector trace_endpos,trace_plane_normal;
float trace_plane_dist;
entity trace_ent;
float trace_inopen,trace_inwater;
entity msg_entity;
void() main,StartFrame,PlayerPreThink,PlayerPostThink,ClientKill,ClientConnect;
void() PutClientInServer,ClientDisconnect,SetNewParms,SetChangeParms;
void end_sys_globals;
.float modelindex;
.vector absmin,absmax;
.float ltime,movetype,solid;
.vector origin,oldorigin,velocity,angles,avelocity,punchangle;
.string classname,model;
.float frame,skin,effects;
.vector mins,maxs,size;
.void() touch,use,think,blocked;
.float nextthink;
.entity groundentity;
.float health,frags,weapon;
.string weaponmodel;
.float weaponframe,currentammo,ammo_shells,ammo_nails,ammo_rockets,ammo_cells;
.float items,takedamage;
.entity chain;
.float deadflag;
.vector view_ofs;
.float button0,button1,button2,impulse,fixangle;
.vector v_angle;
.float idealpitch;
.string netname;
.entity enemy;
.float flags,colormap,team,max_health,teleport_time,armortype,armorvalue;
.float waterlevel,watertype,ideal_yaw,yaw_speed;
.entity aiment,goalentity;
.float spawnflags;
.string target,targetname;
.float dmg_take,dmg_save;
.entity dmg_inflictor,owner;
.vector movedir;
.string message;
.float sounds;
.string noise,noise1,noise2,noise3;
void end_sys_fields;
.float mass,geomtype,gravity,dimension_solid,dimension_hit,jointtype,skeletonindex,basebone;
.float joint_suspension_stiffness,joint_suspension_damping;
void(entity e,vector p) setorigin=#2;
void(entity e,string modelname) setmodel=#3;
string(string modelname) precache_model=#20;
float(float modelindex) skel_create=#263;
float(float skel,entity ent,float modelindex,float retain,float first,float last) skel_build=#264;
vector(float skel,float bone) skel_get_boneabs=#270;
void(float target,float source,float first,float last) skel_copybones=#274;
float(entity ent,string command,float animskel) skel_ragupdate=#281;
void(entity e,vector lo,vector hi) setsize=#4;
void(string s) error=#10;
entity() spawn=#14;
void(entity e) remove=#15;
void(string s) dprint=#25;
string(float n) ftos=#26;
float(vector v) vlen=#12;
void(string s) localcmd=#46;
float(string name) cvar=#45;
float(optional float state) physics_supported = #0:physics_supported;
void(entity e,float state) physics_enable=#540;
void(entity e,vector force,vector point) physics_addforce=#541;
void(entity e,vector torque) physics_addtorque=#542;
float simulated,phase,touches,deleted,mode,joint_enabled;
vector held;
entity dropped,classic,masked,pushed,recycled,constrained,jointent,rig;
vector initial_tip;
float render_skeleton;
entity visible_rig,offsetrig,universal_body,hinge2_body,springbody,springent,springrig;
float springphase;
void(float ok,string what) check = {
    if (!ok) {dprint("PHYSICS_FAIL: ");dprint(what);dprint("\n");localcmd("quit force\n");error(what);}
};
entity(vector pos,float move) prop = {
    local entity e=spawn();e.classname="test_prop";e.movetype=move;
    e.solid=32;e.geomtype=1;e.mass=2;
    setsize(e,'-16 -16 -16','16 16 16');setorigin(e,pos);return e;
};
void() contact = {touches=touches+1;};
void() delete_contact = {deleted=deleted+1;remove(self);};
void() worldspawn = {
    simulated=0;mode=cvar("physics_fixture_mode");joint_enabled=1;
    self.dimension_solid=1;self.dimension_hit=1;
    classic=prop('-200 0 128',6);classic.solid=2;
    check(physics_supported()==(mode!=0),"backend opt-in state");
    if (!mode) return;
    dropped=prop('0 0 128',32);dropped.touch=contact;
    constrained=prop('-400 0 128',32);
    jointent=spawn();jointent.jointtype=-1;jointent.enemy=constrained;
    if (mode!=1) return; // ODE comparison uses just the common collision baseline.
    masked=prop('200 0 128',32);masked.dimension_hit=2;masked.dimension_solid=2;
    pushed=prop('300 0 512',32);pushed.gravity=0.000001;
    physics_addforce(pushed,'480 0 0',pushed.origin);
    local entity old=prop('500 0 512',32);
    physics_addforce(old,'100000 0 0',old.origin);remove(old);
    recycled=prop('500 0 512',32);recycled.gravity=0.000001;
    rig=spawn();precache_model("progs/physics-rig.md5mesh");setmodel(rig,"progs/physics-rig.md5mesh");
    setorigin(rig,'0 200 192');
    rig.skeletonindex=skel_create(rig.modelindex);
    check(rig.skeletonindex>0,"create MD5 skeleton");
    check(skel_build(rig.skeletonindex,rig,rig.modelindex,0,0,0)>0,"build reference skeleton");
    local vector bindtip=skel_get_boneabs(rig.skeletonindex,3);
    local vector bindroot=skel_get_boneabs(rig.skeletonindex,1);
    check(vlen(bindtip)<0.01 && bindroot_z>63.99 && bindroot_z<64.01,"MD5 absolute bind pose converts to relative QC skeleton");
    check(skel_ragupdate(rig,"doll progs/physics-rig.doll",0)==1,"instantiate three-bone ragdoll");
    initial_tip=skel_get_boneabs(rig.skeletonindex,3);
    render_skeleton=skel_create(rig.modelindex);
    skel_copybones(render_skeleton,rig.skeletonindex,0,0);
    if(cvar("physics_fixture_demo")) {
        visible_rig=spawn();setmodel(visible_rig,"progs/physics-rig.md5mesh");
        setorigin(visible_rig,rig.origin);visible_rig.skeletonindex=render_skeleton;visible_rig.basebone=-1;
        rig.modelindex=0;
    }
    offsetrig=spawn();setmodel(offsetrig,"progs/physics-rig.md5mesh");
    setorigin(offsetrig,'500 250 256');offsetrig.angles='20 70 -15';
    offsetrig.skeletonindex=skel_create(offsetrig.modelindex);
    skel_build(offsetrig.skeletonindex,offsetrig,offsetrig.modelindex,0,0,0);
    local vector original=skel_get_boneabs(offsetrig.skeletonindex,2);
    local vector forward=v_forward,right=v_right,up=v_up;
    check(skel_ragupdate(offsetrig,"doll progs/physics-rig-offset.doll",0)==1,"offset/rotation/orient rig builds");
    check(vlen(skel_get_boneabs(offsetrig.skeletonindex,2)-original)<.01,"offset body round-trip preserves bone origin");
    check(vlen(v_forward-forward)<.01 && vlen(v_right-right)<.01 && vlen(v_up-up)<.01,"offset body round-trip preserves basis");
    universal_body=prop('-650 0 128',32);hinge2_body=prop('-750 0 128',32);
    local entity universal_joint=spawn();universal_joint.jointtype=4;universal_joint.aiment=universal_body;
    setorigin(universal_joint,universal_body.origin);universal_joint.movedir='1 -100000 .4';
    local entity hinge2_joint=spawn();hinge2_joint.jointtype=5;hinge2_joint.aiment=hinge2_body;
    setorigin(hinge2_joint,hinge2_body.origin);hinge2_joint.velocity='0 1 0';hinge2_joint.movedir='1 -100000 .4';
    springbody=prop('650 0 128',32);
    springent=spawn();springent.jointtype=5;springent.aiment=springbody;
    springent.angles='-90 0 0';springent.velocity='1 0 0';setorigin(springent,springbody.origin);
    springent.joint_suspension_stiffness=2000;springent.joint_suspension_damping=150;
    springrig=spawn();setmodel(springrig,"progs/physics-rig.md5mesh");
    setorigin(springrig,'-300 600 256');springrig.angles='0 37 0';
    springrig.skeletonindex=skel_create(springrig.modelindex);
    skel_build(springrig.skeletonindex,springrig,springrig.modelindex,0,0,0);
    check(skel_ragupdate(springrig,"doll progs/physics-rig-suspension.doll",0)==1,"create world-anchored suspension rig");
    local entity doomed=prop('400 0 80',32);doomed.touch=delete_contact;
};
void() StartFrame = {
    simulated=simulated+frametime;
    if (mode==1 && rig!=world && physics_supported()) {
        check(skel_ragupdate(rig,"",0)==1,"derive simulated skeleton");
        skel_copybones(render_skeleton,rig.skeletonindex,0,0);
        local vector simtip=skel_get_boneabs(rig.skeletonindex,3);
        local vector drawtip=skel_get_boneabs(render_skeleton,3);
        check(vlen(simtip-drawtip)<0.01,"relative render skeleton matches absolute simulated pose");
    }
    if (cvar("physics_fixture_demo")) {
        local float enabled=cvar("physics_fixture_joint");
        if (enabled!=joint_enabled && physics_supported()) {
            skel_ragupdate(rig,enabled?"enablejoint link0 1":"enablejoint link0 0",0);
            joint_enabled=enabled;
        }
        return;
    }
    if (mode==1) {
        if (!springphase && simulated>1.5) {
            check(springbody.origin_z>127.1 && springbody.origin_z<127.3,"QC suspension supports expected static load");
            check(skel_ragupdate(springrig,"",0)==1,"derive world-anchored suspension rig");
            local vector p=skel_get_boneabs(springrig.skeletonindex,1);
            check(vlen(p-'0 0 62.8')<.15,"ragdoll world anchor includes translated/rotated entity and total rig mass");
            springent.joint_suspension_stiffness=4000;springphase=1;
        }
        if (springphase==1 && simulated>1.9) {
            check(springbody.origin_z>127.5 && springbody.origin_z<127.7,"QC stiffness edit preserves rest position");
            springent.joint_suspension_damping=60;springphase=2;
        }
        if (springphase==2 && simulated>2.1) {
            check(springbody.origin_z>127.5 && springbody.origin_z<127.7,"QC damping edit preserves equilibrium");
            springent.joint_suspension_stiffness=-1;springphase=3;
        }
        if (springphase==3 && simulated>2.3) {
            check(springbody.origin_z>127.5 && springbody.origin_z<127.7,"invalid live spring edit retains valid constraint");
            springent.joint_suspension_stiffness=0;springent.joint_suspension_damping=0;springphase=4;
        }
        if (springphase==4 && simulated>2.7) {
            check(springbody.origin_z>127.99 && springbody.origin_z<128.01,"zero spring settings restore rigid pivot");springphase=5;
        }
    }
    if (!phase && simulated>1.5) {
        // ODE's default per-substep damping settles more slowly than Jolt.
        // Test collision, not identical solver response; retain a bounded deadline.
        if (mode==2 && dropped.origin_z>17 && simulated<5) return;
        check(classic.origin_z>15 && classic.origin_z<17,"classic toss floor collision");
        dprint("PHYSICS_CLASSIC_Z ");dprint(ftos(classic.origin_z));dprint("\n");
        if (mode) {
            check(dropped.origin_z>14 && dropped.origin_z<18,"dynamic prop lands on BSP mesh");
            check(touches>0,"deferred QC touch callback");
            check(constrained.origin_z>127 && constrained.origin_z<129,"QC fixed joint anchors body to world");
            dprint("PHYSICS_DROP_Z ");dprint(ftos(dropped.origin_z));dprint("\n");
        }
        if (mode==1) {
            local vector tip=skel_get_boneabs(rig.skeletonindex,3);
            local vector top=skel_get_boneabs(rig.skeletonindex,2);
            local vector delta=top-tip;
            check(tip_z<initial_tip_z-20,"ragdoll bones fall under gravity");
            check(vlen(delta)>28 && vlen(delta)<36,"ragdoll keeps adjacent bone anchor separation");
            check(skel_ragupdate(rig,"enablejoint link0 0",0)==1,"disable named skeletal joint");
            check(skel_ragupdate(rig,"enablejoint link0 1",0)==1,"enable named skeletal joint");
            check(skel_ragupdate(offsetrig,"",0)==1,"derive offset rig after simulation");
            local vector offsettop=skel_get_boneabs(offsetrig.skeletonindex,1);
            local vector offsettip=skel_get_boneabs(offsetrig.skeletonindex,3);
            check(vlen(offsettop-offsettip)>63.5 && vlen(offsettop-offsettip)<64.5,"fixed rig keeps bone spacing with local offsets and rotated entity");
            check(universal_body.origin_z>127.9 && universal_body.origin_z<128.1,"QC universal world anchor holds");
            check(hinge2_body.origin_z>127.9 && hinge2_body.origin_z<128.1,"QC hinge2 world anchor holds");
            dprint("PHYSICS_RAGDOLL_OK\n");
            check(masked.origin_z < -100,"dimension mask excludes map collision");
            check(pushed.velocity_x>0,"force before first body creation");
            check(recycled.velocity_x==0,"removed entity force cannot reach reused edict");
            check(deleted==1,"QC contact can remove its own body safely");
            physics_enable(pushed,0);held=pushed.origin;phase=1;return;
        }
        phase=3;
    }
    if (phase==1 && simulated>1.8) {
        check(pushed.origin==held,"explicit sleep persists across frames");
        physics_enable(pushed,1);
        setsize(pushed,'-20 -20 -20','20 20 20');
        physics_addforce(pushed,'480 0 0',pushed.origin);
        phase=2;return;
    }
    if (phase==2 && simulated>3.1) {
        check(pushed.velocity_x>0,"shape rebuild retains pending force");
        check(skel_ragupdate(rig,"doll progs/physics-rig-invalid.doll",0)==0,"invalid rig rolls back partial bodies");
        check(skel_ragupdate(rig,"doll progs/physics-rig.doll",0)==1,"valid rig recovers after rejected rig");
        check(skel_ragupdate(rig,"doll progs/physics-rig-unsupported.doll",0)==0,"unsupported skeletal joint rejects the entire rig");
        check(skel_ragupdate(rig,"doll progs/physics-rig.doll",0)==1,"valid rig recovers after unsupported joint");
        check(skel_ragupdate(rig,"doll progs/physics-rig-bodyless.doll",0)==0,"bodyless joint rejects safely");
        check(skel_ragupdate(rig,"doll progs/physics-rig-badname.doll",0)==0,"unknown body update rejects safely");
        check(skel_ragupdate(rig,"doll progs/physics-rig-badtype.doll",0)==0,"unknown joint type rejects safely");
        check(skel_ragupdate(rig,"doll progs/physics-rig-badspring.doll",0)==0,"negative rig spring rolls back whole rig");
        check(skel_ragupdate(rig,"doll progs/physics-rig-wrongspring.doll",0)==0,"suspension on wrong joint type rejects rig");
        check(physics_supported(0)==0,"backend shutdown");
        check(physics_supported(1)==1,"backend restart");
        check(skel_ragupdate(rig,"doll progs/physics-rig.doll",0)==1,"ragdoll can be reinstantiated after backend restart");
        check(skel_ragupdate(springrig,"doll progs/physics-rig-suspension.doll",0)==1,"recreate suspended rig after backend restart");
        phase=4;return;
    }
    if (phase==4 && simulated>3.7) {
        check(skel_ragupdate(springrig,"",0)==1,"derive restarted suspension rig");
        local vector restarted=skel_get_boneabs(springrig.skeletonindex,1);
        check(vlen(restarted-'0 0 62.8')<.15,"restarted suspension recovers original world rest anchor");
        dprint("PHYSICS_SUSPENSION_OK\n");phase=3;
    }
    if (phase==3) {dprint("PHYSICS_ENGINE_OK\n");phase=5;localcmd("physics_fixture_next\n");}
};
void() info_player_start={};
void() main={};
void() PlayerPreThink={}; void() PlayerPostThink={}; void() ClientKill={};
void() ClientConnect={}; void() PutClientInServer={
    if (!cvar("physics_fixture_demo")) return;
    self.classname="player";self.movetype=8;self.solid=0;self.flags=8;self.health=100;
    self.angles='15 112 0';self.fixangle=1;
    setsize(self,'-16 -16 -24','16 16 32');setorigin(self,'80 0 110');
    if (cvar("physics_fixture_capture")) {
        localcmd("echo JOLT_DEMO_CLIENT_READY\n");
        if (!cvar("physics_fixture_capture_started")) {
            localcmd("set physics_fixture_capture_started 1\n");
            localcmd("defer 0.5 screenshot jolt-rig-connected.png\n");
            localcmd("defer 1 gpu_info\n");
            localcmd("defer 2 screenshot jolt-rig-settled.png\n");
            localcmd("defer 2.1 set physics_fixture_joint 0\n");
            localcmd("defer 3 screenshot jolt-rig-released.png\n");
            localcmd("defer 3.2 set physics_fixture_joint 1\n");
            localcmd("defer 3.3 map physics\n");
            localcmd("defer 5 screenshot jolt-rig-reloaded.png\n");
            localcmd("defer 6 quit force\n");
        }
    }
}; void() ClientDisconnect={};
void() SetNewParms={}; void() SetChangeParms={};
