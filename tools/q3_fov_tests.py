#!/usr/bin/env python3
from pathlib import Path
import subprocess, argparse
root=Path(__file__).resolve().parents[1]
s=(root/'plugins/quake3/clq3_ui.c').read_text()
a=s.index('static void VQ3_ExtendFov('); b=s.index('\nvoid VQ3_RenderView(',a)
function=s[a:b]
parser=argparse.ArgumentParser(description='Check the plugin FOV adjustment, zoom, aspect and server restrictions')
parser.add_argument('--out',type=Path,default=root/'build/q3-fov-tests')
args=parser.parse_args()
args.out.mkdir(parents=True,exist_ok=True)
p=args.out/'fov-generated.c'
p.write_text('''#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define RDF_NOWORLDMODEL 1
#define bound(a,b,c) fminf(fmaxf((a),(b)),(c))
typedef struct { float fov[2], fov_viewmodel[2]; int flags; } plugrefdef_t;
static struct { struct { int valid; struct { int pm_type; } playerstate; } snap; } ccs;
static float requested, zoomvalue;
static const char *dmflags, *videoflags;
static float getfloat(const char *n) { return !strcmp(n,"fov") ? requested : zoomvalue; }
static const char *getkey(const char *s,const char *n) { (void)s; return !strcmp(n,"dmflags") ? dmflags : videoflags; }
static const char *CG_GetConfigString(int n) { (void)n; return "server"; }
static struct { float (*GetFloat)(const char*); } cv={getfloat}, *cvarfuncs=&cv;
static struct { const char *(*GetInfoKey)(const char*,const char*); } wf={getkey}, *worldfuncs=&wf;
''' + function + '''
static plugrefdef_t scene(float x) {
    plugrefdef_t s={{x,atan(tan(x*M_PI/360)*0.75)*360/M_PI},{0,0},0};
    return s;
}
static void check(float input,float output) {
    plugrefdef_t s=scene(input);
    double ratio=tan(s.fov[1]*M_PI/360)/tan(s.fov[0]*M_PI/360);
    VQ3_ExtendFov(&s);
    assert(fabs(s.fov[0]-output)<0.001);
    assert(fabs(tan(s.fov[1]*M_PI/360)/tan(s.fov[0]*M_PI/360)-ratio)<0.0001);
}
int main(void) {
    ccs.snap.valid=1; dmflags=videoflags="0"; zoomvalue=22.5;
    requested=175; check(160,175); check(22.5,22.5); check(91.25,98.75);
    requested=500; check(160,179); check(161,179);
    requested=120; check(120,120);
    requested=175; dmflags="16"; check(90,90); dmflags="0";
    videoflags="1"; check(140,140); videoflags="0";
    ccs.snap.playerstate.pm_type=5; check(90,90); ccs.snap.playerstate.pm_type=0;
    ccs.snap.valid=0; check(160,160); ccs.snap.valid=1;
    plugrefdef_t ui=scene(90); ui.flags=RDF_NOWORLDMODEL; VQ3_ExtendFov(&ui); assert(ui.fov[0]==90);
    puts("Q3 extended FOV: range, zoom, aspect, server limits, intermission, and UI passed");
}
''')
subprocess.run(['clang','-std=c17','-fsanitize=address,undefined','-O1',str(p),'-o',str(args.out/'fov-tests')],check=True)
subprocess.run([str(args.out/'fov-tests')],check=True)
