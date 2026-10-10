// Client fixture exercising engine scene, transient lights, and offscreen UI.
entity self;
float end_sys_globals;
.vector origin;
float end_sys_fields;
void() clearscene = #300;
void(float mask) addentities = #301;
float(float property, ...) setproperty = #303;
void() renderscene = #304;
float(vector position, float radius, vector color, float style, string cube, float flags) dynamiclight_add = #305;
float(vector position, vector size, vector rgb, float alpha, float flags) drawfill = #323;
float(vector position, string pic, vector size, vector rgb, float alpha, float flags) drawpic = #322;
float(string name) cvar = #45;
float(float timer) gettimef = #519;
void(float width, float height, float do2d) CSQC_UpdateView = {
    clearscene();
    addentities(3);
    setproperty(1, '0 0 0');
    setproperty(4, [width, height, 0]);
    if (cvar("gpu_test_slime")) {
        setproperty(11, '0 2816 -70');
        if (cvar("gpu_test_slime") == 2) setproperty(11, '128 2816 -170');
        setproperty(15, '20 0 0');
    }
    if (cvar("gpu_test_water")) {
        setproperty(11, '1415 1722 144');
        setproperty(15, '80 0 0');
    }
    if (cvar("gpu_test_ad")) {
        setproperty(11, '-48 -470 110');
        setproperty(15, '-8 100 0');
        if (cvar("gpu_test_ad") == 2) {
            setproperty(11, '0 0 344');
            setproperty(15, '-75 90 0');
        }
    }
    if (cvar("gpu_test_tears")) {
        setproperty(11, '672 -256 -490');
        setproperty(15, '-10 180 0');
        if (cvar("gpu_test_tears") == 3)
            setproperty(15, [-10, gettimef(0)*24, 0]);
        if (cvar("gpu_test_tears") == 2) {
            setproperty(11, '800 -400 -680');
            setproperty(15, '-10 0 0');
            // Illuminate this dark alcove and exercise shadows through foliage.
            dynamiclight_add('940 -400 -630', 400, '1 0.8 0.6', 0, "", 0);
        }
    }
    dynamiclight_add('394 730 84', 600, '1 0.6 0.3', 0, "", 0);
    renderscene();
    if (cvar("gpu_test_rtt")) {
        setproperty(212, "gpu_rtt_fixture", 1, '64 64 0');
        drawfill('0 0 0', [width,height,0], '0 0 1', 1, 0);
        drawfill('0 0 0', [width*0.5,height,0], '1 0 0', 1, 0);
        setproperty(212, "");
        drawpic('16 16 0', "gpu_rtt_pic", '128 128 0', '1 1 1', 1, 0);
    }
};
