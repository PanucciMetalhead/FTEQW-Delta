#pragma copyright "QCC regression fixture"
void(string message) puts = #1;
string(float value) ftos = #2;
entity() spawn = #3;
.float health;
float end_sys_fields;
float calls;
float(float x) sideeffect = { calls = calls + 1; return x; };
float(float n) factorial = {
    local float saved;
    saved = n;
    if (n <= 1) return 1;
    return saved * factorial(n - 1);
};
float(float n) branch = {
    local float result;
    if (n < 0) result = 3;
    else if (n == 0) result = 5;
    else result = 7;
    return result;
};
void() main = {
    local float a, b, result, i;
    local vector v;
    local entity e;
    a = 2; b = 3;
    result = (a + b) * (a - b);
    if (result != -5) { puts("FAIL arithmetic\n"); return; }
    result = 5 * 6 + 12 / 4;
    if (result != 33) { puts("FAIL constants\n"); return; }
    result = 16777216; result = result + 1;
    if (result != 16777216) { puts("FAIL float precision\n"); return; }
    v = '2 3 4'; v = v * 2;
    if (v != '4 6 8') { puts("FAIL vector\n"); return; }
    calls = 0;
    result = sideeffect(2) + sideeffect(3);
    if (result != 5 || calls != 2) { puts("FAIL side effects\n"); return; }
    if (factorial(6) != 720) { puts("FAIL recursion\n"); return; }
    if (branch(-1) != 3 || branch(0) != 5 || branch(1) != 7) {
        puts("FAIL branches\n"); return;
    }
    result = 0;
    for (i = 0; i < 5; i = i + 1) result = result + i;
    if (result != 10) { puts("FAIL loop\n"); return; }
    e = spawn(); e.health = 37;
    if (e.health != 37) { puts("FAIL entity field\n"); return; }
    puts("QCC_RUNTIME_OK\n");
};
