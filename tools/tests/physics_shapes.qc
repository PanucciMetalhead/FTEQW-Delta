// Original model and recipe fixture, using the public SSQC interface.
#include "declarations.qh"
.string physics_shape;
.float scale;
entity stack0,stack1,stack2,closed,opened,shot_closed,shot_open,changing,recovery;
float elapsed,phase;
void(float ok,string why) verify={
    if(!ok) {dprint("SHAPES_FAIL: ");dprint(why);dprint("\n");localcmd("quit force\n");error(why);}
};
entity(vector at,float shape,string recipe,string mdl) prop={
    local entity e=spawn();e.movetype=32;e.solid=32;e.mass=2;e.geomtype=shape;e.physics_shape=recipe;
    if(mdl!="")setmodel(e,mdl);
    setsize(e,'-16 -16 -16','16 16 16');setorigin(e,at);return e;
};
entity(vector at) shot={
    local entity e=prop(at,2,"","");setsize(e,'-4 -4 -4','4 4 4');e.velocity='0 1000 0';e.gravity=.000001;return e;
};
void() worldspawn={
    self.dimension_solid=1;self.dimension_hit=1;
    verify(physics_supported(),"Jolt enabled");elapsed=0;phase=0;
    precache_model("progs/cube.md5mesh");precache_model("progs/frame.md5mesh");
    stack0=prop('0 400 18',12,"","progs/cube.md5mesh");
    stack1=prop('0 400 52',12,"","progs/cube.md5mesh");
    stack2=prop('0 400 86',12,"","progs/cube.md5mesh");
    closed=prop('-300 0 200',12,"","progs/frame.md5mesh");closed.gravity=.000001;closed.mass=1000;
    opened=prop('300 0 200',13,"progs/frame.jolt","progs/frame.md5mesh");opened.gravity=.000001;opened.mass=1000;
    shot_closed=shot('-300 -100 200');shot_open=shot('300 -100 200');
    changing=prop('500 400 128',13,"progs/cube.jolt","progs/cube.md5mesh");
    recovery=prop('-500 400 128',13,"../outside.jolt","progs/cube.md5mesh");
    prop('-500 -400 128',13,"progs/missing.jolt","");
    prop('-400 -400 128',13,"progs/bad.jolt","");
    prop('-300 -400 128',13,"progs/flat.jolt","");
    prop('-200 -400 128',13,"progs/large.jolt","");
    prop('-100 -400 128',4,"","progs/cube.md5mesh");
    prop('0 -400 128',999,"","");
};
void() StartFrame={
    elapsed=elapsed+frametime;
    if(!phase && elapsed>.35) {
        verify(shot_open.origin_y>50,"authored opening remains passable through compound");
        verify(shot_closed.origin_y<0,"single model hull fills concave opening");phase=1;
    }
    if(phase==1 && elapsed>1.5) {
        verify(vlen(changing.origin-'500 400 16')<1,"compound cube lands with aligned origin");
        changing.physics_shape="progs/tall.jolt";setorigin(changing,'500 400 128');
        recovery.physics_shape="progs/cube.jolt";recovery.scale=2;phase=2;
    }
    if(phase==2 && elapsed>3) {
        verify(stack0.origin_z>14 && stack0.origin_z<18,"model hull stack bottom");
        verify(stack1.origin_z>46 && stack1.origin_z<50,"model hull stack middle");
        verify(stack2.origin_z>78 && stack2.origin_z<82,"model hull stack top");
        verify(vlen(changing.origin-'500 400 32')<1,"recipe name change rebuilds shape");
        verify(vlen(recovery.origin-'-500 400 32')<1,"rejected geometry recovers with valid scaled recipe");
        remove(opened);remove(closed);remove(changing);remove(recovery);
        phase=3;dprint("SHAPES_ENGINE_OK\n");localcmd("shapes_next\n");
    }
};
void() main={};void() info_player_start={};void() PlayerPreThink={};void() PlayerPostThink={};
void() ClientKill={};void() ClientConnect={};void() PutClientInServer={};void() ClientDisconnect={};
void() SetNewParms={};void() SetChangeParms={};
