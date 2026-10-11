// Render model skins over a uniform background to test replacement lookup.
entity self;
float end_sys_globals;
.vector origin;
.vector angles;
.vector mins;
.vector maxs;
.string model;
.float modelindex;
.float frame;
.float alpha;
.float scale;
.float renderflags;
float end_sys_fields;
entity rocket, quad;
entity() spawn = #14;
void(entity e, string name) setmodel = #3;
void(string name) precache_model = #20;
void() clearscene = #300;
void(entity e) addentity = #302;
float(float property, ...) setproperty = #303;
void() renderscene = #304;
void(string shader, float flags) polygonbegin = #306;
void(vector pos, vector uv, vector rgb, float alpha) polygonvertex = #307;
void() polygonend = #308;
float(string name) cvar = #45;
void() CSQC_Init = {
    precache_model("progs/g_rock2.mdl"); precache_model("progs/quaddama.mdl");
    rocket = spawn(); quad = spawn();
    setmodel(rocket, "progs/g_rock2.mdl");
    setmodel(quad, "progs/quaddama.mdl");
    rocket.origin = '128 52 -16'; rocket.angles = '0 90 0'; rocket.scale = 1.5;
    quad.origin = '128 -52 -24'; quad.angles = '0 90 0'; quad.scale = 1.5;
};
void(float width, float height, float do2d) CSQC_UpdateView = {
    clearscene();
    setproperty(1, '0 0 0'); setproperty(4, [width, height, 0]);
    setproperty(11, '0 0 0'); setproperty(15, '0 0 0');
    setproperty(19, 0); setproperty(20, 0); setproperty(21, 0);
    polygonbegin("gpu_sprite_background", 0);
    polygonvertex('300 600 600', '0 0 0', '0.2 0.4 0.7', 1);
    polygonvertex('300 -600 600', '1 0 0', '0.2 0.4 0.7', 1);
    polygonvertex('300 -600 -600', '1 1 0', '0.2 0.4 0.7', 1);
    polygonvertex('300 600 -600', '0 1 0', '0.2 0.4 0.7', 1);
    polygonend();
    if (cvar("gpu_test_models")) { addentity(rocket); addentity(quad); }
    renderscene();
};
