// Projection regression: full view, independent subviews, and CSQC RTT.
entity self;
float end_sys_globals;
.vector origin;
.vector angles;
.string model;
.float modelindex;
.float frame;
.float alpha;
.float scale;
.float renderflags;
float end_sys_fields;
entity preview;
entity() spawn = #14;
void(entity e, string name) setmodel = #3;
void(string name) precache_model = #20;
void(entity e) addentity = #302;
void() clearscene = #300;
void(float mask) addentities = #301;
float(float property, ...) setproperty = #303;
void() renderscene = #304;
float(vector position, vector size, vector rgb, float alpha, float flags) drawfill = #323;
float(vector position, string pic, vector size, vector rgb, float alpha, float flags) drawpic = #322;
float(string name) cvar = #45;
void() CSQC_Init = {
    precache_model("progs/g_rock2.mdl");
    preview = spawn(); setmodel(preview, "progs/g_rock2.mdl");
    preview.origin = '128 52 -16'; preview.angles = '0 90 0'; preview.scale = 1.5;
};
void(vector position, vector size, vector angles) view = {
    clearscene();
    addentities(3);
    setproperty(1, position);
    setproperty(4, size);
    setproperty(11, '394 730 84');
    setproperty(15, angles);
    if (cvar("gpu_panini_layout") == 3) {
        setproperty(11, '1415 1722 144'); setproperty(15, '80 0 0');
    }
    if (cvar("gpu_panini_layout") == 4) {
        setproperty(11, '0 0 0'); setproperty(15, '0 0 0');
        setproperty(19, 0); addentity(preview);
    }
    setproperty(8, [cvar("gpu_panini_fov"), 85, 0]);
    renderscene();
};
void(float width, float height, float do2d) CSQC_UpdateView = {
    if (cvar("gpu_panini_layout") == 1) {
        view('0 0 0', [width/2, height, 0], '10 160 0');
        view([width/2, 0, 0], [width/2, height, 0], '0 60 0');
    } else if (cvar("gpu_panini_layout") == 2) {
        setproperty(212, "gpu_rtt_fixture", 1, '400 300 0');
        // Viewport dimensions use logical screen coordinates, even in an RTT.
        view('0 0 0', [width,height,0], '10 160 0');
        setproperty(212, "");
        drawfill('0 0 0', [width,height,0], '0.05 0.08 0.12', 1, 0);
        drawpic('200 150 0', "gpu_rtt_pic", '400 300 0', '1 1 1', 1, 0);
    } else {
        view('0 0 0', [width, height, 0], '10 160 0');
    }
    if (cvar("gpu_panini_overlay"))
        drawfill('16 16 0', '128 64 0', '0.3 0.5 0.7', 1, 0);
};
