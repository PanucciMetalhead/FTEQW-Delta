// Fixed-camera remaster models; this client fixture leaves server gamecode intact.
entity self;
float end_sys_globals;
.vector origin;
.vector angles;
.vector mins;
.vector maxs;
.string model;
.float modelindex;
.float frame;
.float skin;
.float scale;
float end_sys_fields;
entity player_md5, soldier_md5, dog_md5, player_mdl, soldier_mdl, dog_mdl;
entity() spawn = #14;
void(entity e, string name) setmodel = #3;
void(string name) precache_model = #20;
void(string s, ...) print = #339;
void(string s) localcmd = #46;
float(float modelindex, string name) frameforname = #276;
string(float f) ftos = #26;
float(float modelindex) skel_create = #263;
float(float skel) skel_get_numbones = #265;
void(float skel) skel_delete = #275;
void() clearscene = #300;
void(entity e) addentity = #302;
float(float property, ...) setproperty = #303;
void() renderscene = #304;
void(string shader, float flags) polygonbegin = #306;
void(vector pos, vector uv, vector rgb, float alpha) polygonvertex = #307;
void() polygonend = #308;
float(string name) cvar = #45;
void(string name, string lastpose, string beyondpose) inspect_model = {
    local entity e;
    local float skel;
    precache_model(name);
    e = spawn(); setmodel(e, name);
    skel = skel_create(e.modelindex);
    print("REMASTER_MODEL ", name, " bones=", ftos(skel_get_numbones(skel)), "\n");
    skel_delete(skel);
    if (lastpose != "")
        print("REMASTER_ANIM ", name, " last=", ftos(frameforname(e.modelindex,lastpose)), " beyond=", ftos(frameforname(e.modelindex,beyondpose)), "\n");
};
#include "remaster-model-list.qh"
entity(string name, vector position) actor = {
    local entity e;
    precache_model(name);
    e = spawn(); setmodel(e, name);
    e.origin = position; e.angles = '0 270 0'; e.scale = 1.25;
    return e;
};
void() CSQC_Init = {
    inspect_all_models();
    player_md5 = actor("progs/player.md5mesh", '480 -192 88');
    soldier_md5 = actor("progs/soldier.md5mesh", '536 -192 88');
    dog_md5 = actor("progs/dog.md5mesh", '424 -192 88');
    player_mdl = actor("progs/player.mdl", '480 -192 88');
    soldier_mdl = actor("progs/soldier.mdl", '536 -192 88');
    dog_mdl = actor("progs/dog.mdl", '424 -192 88');
    local float skel;
    skel = skel_create(player_mdl.modelindex);
    print("REMASTER_REPLACEMENT player bones=", ftos(skel_get_numbones(skel)), "\n");
    skel_delete(skel);
    localcmd("exec remaster-captures.cfg\n");
};
void(float width, float height, float do2d) CSQC_UpdateView = {
    clearscene();
    setproperty(1, '0 0 0'); setproperty(4, [width,height,0]);
    setproperty(11, '480 -352 120'); setproperty(15, '0 90 0');
    setproperty(19, 0); setproperty(20, 0); setproperty(21, 0);
    polygonbegin("gpu_sprite_background", 0);
    polygonvertex('1080 -52 720', '0 0 0', '0.2 0.4 0.7', 1);
    polygonvertex('-120 -52 720', '1 0 0', '0.2 0.4 0.7', 1);
    polygonvertex('-120 -52 -480', '1 1 0', '0.2 0.4 0.7', 1);
    polygonvertex('1080 -52 -480', '0 1 0', '0.2 0.4 0.7', 1);
    polygonend();
    player_md5.frame = soldier_md5.frame = dog_md5.frame = cvar("gpu_remaster_frame");
    player_mdl.frame = soldier_mdl.frame = dog_mdl.frame = cvar("gpu_remaster_frame");
    if (cvar("gpu_remaster_models") == 1) {addentity(player_md5);addentity(soldier_md5);addentity(dog_md5);}
    if (cvar("gpu_remaster_models") == 2) {addentity(player_mdl);addentity(soldier_mdl);addentity(dog_mdl);}
    renderscene();
};
