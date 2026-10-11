// Scene/UI compositing fixture. Uses an owned map only as the 3D background.
entity self;
float end_sys_globals;
.vector origin;
float end_sys_fields;
void() clearscene = #300;
void(float mask) addentities = #301;
float(float property, ...) setproperty = #303;
void() renderscene = #304;
float(vector position, vector size, vector rgb, float alpha, float flags) drawfill = #323;
float(vector position, string pic, vector size, vector rgb, float alpha, float flags) drawpic = #322;
float(string name) cvar = #45;
void(float width, float height, float do2d) CSQC_UpdateView = {
    clearscene();
    addentities(3);
    setproperty(1, '0 0 0');
    setproperty(4, [width, height, 0]);
    setproperty(11, '394 730 84');
    setproperty(15, '10 160 0');
    renderscene();
    if (cvar("gpu_gamma_overlay")) {
        drawfill('16 16 0', '128 64 0', '0.3 0.5 0.7', 1, 0);
        drawfill('160 16 0', '128 64 0', '0.3 0.5 0.7', 0.5, 0);
        setproperty(212, "gpu_rtt_fixture", 1, '64 64 0');
        drawfill('0 0 0', [width,height,0], '0.6 0.3 0.5', 1, 0);
        setproperty(212, "");
        drawpic('304 16 0', "gpu_rtt_pic", '128 128 0', '1 1 1', 1, 0);
        // Repaint the same RTT after its first on-screen use. Both uses must
        // retain their own contents when screen draws bypass scene gamma.
        setproperty(212, "gpu_rtt_fixture", 1, '64 64 0');
        drawfill('0 0 0', [width,height,0], '0.2 0.7 0.4', 1, 0);
        setproperty(212, "");
        drawpic('448 16 0', "gpu_rtt_pic", '128 128 0', '1 1 1', 1, 0);
    }
};
