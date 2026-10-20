#include "declarations.qh"
#include "physics_pointanchors.qh"
float elapsed,phase;
void(string text) pa_report = {dprint(text);};
void() worldspawn = {
    self.dimension_solid=1;self.dimension_hit=1;
    pa_start(cvar("physics_jolt_pointanchor2"));
};
void() StartFrame = {
    elapsed+=frametime;
    if(!phase && elapsed>.8) {
        pa_initial();
        if(pa_mode) pa_j2.velocity='20000000 0 0'; // Must keep the live joint.
        phase=1;
    } else if(phase==1 && elapsed>1.15) {
        pa_check(vlen(pa_second.origin-[pa_mode?240:200,600,256])<.1,"rejected edit preserves joint");
        setorigin(pa_j2,pa_second.origin);pa_j2.velocity='260 600 256';phase=2;
    } else if(phase==2 && elapsed>1.5) {
        pa_check(vlen(pa_second.origin-[pa_mode?260:200,600,256])<.1,"valid edit recovers after rejection");
        pa_drop();phase=3;
    } else if(phase==3 && elapsed>1.9) {
        pa_dropped(pa_mode?260:200);pa_j2.jointtype=0;phase=4;
    } else if(phase==4 && elapsed>2.25) {
        pa_check(pa_second.origin_z<225,"jointtype zero releases the remaining grouped joint");
        dprint("POINT_ENGINE_OK\n");phase=5;localcmd("point_next\n");
    }
};
void() main={};void() PlayerPreThink={};void() PlayerPostThink={};void() ClientKill={};
void() ClientConnect={};void() PutClientInServer={};void() ClientDisconnect={};
void() SetNewParms={};void() SetChangeParms={};
