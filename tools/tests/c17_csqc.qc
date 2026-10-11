entity self, other, world;
float time;
float end_sys_globals;
.vector origin, mins, maxs;
.float solid, marker;
.string classname;
.entity chain;
float end_sys_fields;
entity() spawn = #14;
void(entity e) remove = #15;
void(entity e, vector p) setorigin = #2;
void(entity e, vector mins, vector maxs) setsize = #4;
void(string text) print = #339;
void(string text) error = #10;
float(...) fork = #210;
entity(.string field, string match) findchain = #402;
entity(.float field, float match) findchainfloat = #403;
entity(.float field, float match) findchainflags = #450;
entity(vector org, float radius) findradius = #22;
float(vector pos, vector size, vector rgb, float alpha, float flags) drawfill = #323;

void(entity head, entity a, entity b) checkchain = {
    local float count;
    while (head != world) {
        if (head != a && head != b) error("C17 chain: unexpected entity");
        count = count + 1;
        if (count > 2) error("C17 chain: loop");
        head = head.chain;
    }
    if (count != 2) error("C17 chain: wrong entity count");
};
void() testdelayed = {
    local float start;
    start = time;
    if (fork(0.25)) {
        if (time < start + 0.2) error("C17 fork: resumed too early");
        print("C17_COROUTINE_DELAY_PASS\n");
    }
};
void() testimmediate = {
    if (fork()) print("C17_COROUTINE_ZERO_PASS\n");
};
void() CSQC_Init = {
    local entity a, b, c;
    a = spawn(); b = spawn(); c = spawn();
    a.classname = "c17"; b.classname = "c17"; c.classname = "other";
    a.marker = 7; b.marker = 7; c.marker = 8;
    a.solid = 2; b.solid = 2; c.solid = 2;
    setsize(a, '-1 -1 -1', '1 1 1');
    setsize(b, '-1 -1 -1', '1 1 1');
    setsize(c, '-1 -1 -1', '1 1 1');
    setorigin(a, '0 0 0'); setorigin(b, '1 0 0'); setorigin(c, '2048 0 0');
    checkchain(findchain(classname, "c17"), a, b);
    checkchain(findchainfloat(marker, 7), a, b);
    checkchain(findchainflags(marker, 4), a, b);
    checkchain(findradius('0 0 0', 32), a, b);
    remove(a); remove(b); remove(c);
    print("C17_FIELDS_PASS\n");
    testdelayed(); testimmediate();
};
void(float width, float height, float do2d) CSQC_UpdateView = {
    drawfill('0 0 0', [width,height,0], '0.1 0.1 0.1', 1, 0);
};
