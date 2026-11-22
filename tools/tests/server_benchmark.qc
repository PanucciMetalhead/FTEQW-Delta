// Synthetic server benchmark. Public NQ ABI declarations, no game code.
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

void(vector start, vector end, float nomonsters, entity ignore) traceline = #16;
void(entity e, vector p) setorigin = #2;
void(entity e, vector lo, vector hi) setsize = #4;
entity() spawn = #14;
float(string name) cvar = #45;
void(string s) error = #10;
void(string s) dprint = #25;
string(float x) ftos = #26;

float(float x) floor = #37;
float traces, ran;

void() worldspawn = {
    traces = cvar("bench_traces");
    for (float i = 0; i < 64; ++i) {
        vector pos = '0 0 48';
        pos.x = (i % 8) * 48 - 168;
        pos.y = floor(i / 8) * 48 - 168;
        entity blocker = spawn();
        blocker.solid = 2; // SOLID_BBOX
        setsize(blocker, '-12 -12 -12', '12 12 12');
        setorigin(blocker, pos);
    }
    traceline('-168 -168 128', '-168 -168 -64', 0, world);
    if (trace_ent == world || trace_fraction >= 1)
        error("Server benchmark: expected a trace to hit a linked blocker");
};

void() StartFrame = {
    for (float i = 0; i < traces; ++i) {
        vector top = '0 0 128', bottom = '0 0 -64';
        top.x = (i % 64) * 6 - 192;
        top.y = ((i * 19) % 64) * 6 - 192;
        bottom.x = top.x;
        bottom.y = top.y;
        traceline(top, bottom, 0, world);
    }
    if (!ran) {
        dprint("SERVER_BENCH_TRACES ");
        dprint(ftos(traces));
        dprint("\n");
        ran = 1;
    }
};
