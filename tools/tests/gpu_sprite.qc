// Render real Quake explosion sprites over a uniform background.
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
.float forceshader;
float end_sys_fields;
entity explosion0, explosion3, explosion5, smoke;
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
float(string name) shaderforname = #238;
void() CSQC_Init = {
    precache_model("progs/s_explod.spr");
    explosion0 = spawn(); explosion3 = spawn(); explosion5 = spawn();
    setmodel(explosion0, "progs/s_explod.spr");
    setmodel(explosion3, "progs/s_explod.spr");
    setmodel(explosion5, "progs/s_explod.spr");
    explosion0.origin = '128 80 0'; explosion0.frame = 0;
    explosion3.origin = '128 0 0'; explosion3.frame = 3;
    explosion5.origin = '128 -80 0'; explosion5.frame = 5;
    if (cvar("gpu_test_smoke_assets")) {
        smoke = spawn();
        setmodel(smoke, "progs/s_explod.spr");
        smoke.origin = '128 0 0'; smoke.scale = 36;
        smoke.forceshader = shaderforname("shotgunSmokePuff");
    }
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
    if (cvar("gpu_test_sprite")) {
        explosion0.alpha = explosion3.alpha = explosion5.alpha = 1;
        if (cvar("gpu_test_sprite") == 2)
            explosion0.alpha = explosion3.alpha = explosion5.alpha = 0.25;
        addentity(explosion0); addentity(explosion3); addentity(explosion5);
    }
    if (smoke && cvar("gpu_test_smoke_alpha") > 0) {
        smoke.alpha = cvar("gpu_test_smoke_alpha");
        addentity(smoke);
    }
    renderscene();
};
