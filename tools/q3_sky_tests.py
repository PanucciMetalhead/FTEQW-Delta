#!/usr/bin/env python3
"""Check the renderer's actual cloud projection, mesh bounds and texture transforms.

Compiles source helpers with ASan/UBSan. No game data or GPU is required.
"""
from pathlib import Path
import argparse
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out', type=Path, default=ROOT/'build/q3-sky-tests')
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
warp = (ROOT/'engine/gl/gl_warp.c').read_text()
a = warp.index('static void R_Q3CloudTexCoord(')
b = warp.index('\n#endif', a)
clouds = warp[a:b]
backend = (ROOT/'engine/sdlgpu/gpu_backend.c').read_text()
a = backend.index('static float GPU_Wave(')
b = backend.index('\nstatic texid_t GPU_PassTexture(', a)
transforms = backend[a:b]
# The fixture only needs enum symbols, not renderer/SDL structure layouts.
symbols = sorted(set(__import__('re').findall(r'\bSHADER_(?:FUNC|TCMOD)_\w+', transforms)))
source = '''#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define max(a,b) ((a)>(b)?(a):(b))
#define bound(a,b,c) ((b)<(a)?(a):(b)>(c)?(c):(b))
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VectorCopy(a,b) memcpy((b),(a),sizeof(vec3_t))
typedef float vec3_t[3], vec2_t[2], vecV_t[3];
typedef unsigned short index_t;
typedef struct { float cloudheight; } skydome_t;
typedef struct { skydome_t *skydome; } shader_t;
typedef struct { int unused; } batch_t;
typedef struct { vecV_t *xyz_array; vec2_t *st_array; index_t *indexes; int numvertexes,numindexes; } mesh_t;
static int st_to_vec[6][3]={{3,-1,2},{-3,1,2},{1,3,2},{-1,-3,2},{-2,-1,3},{2,-1,-3}};
static float skymins[2][6],skymaxs[2][6];
static int calls,vertices,indices;
static void R_CalcSkyChainBounds(batch_t *b) { (void)b; }
static void R_DrawSkyMesh(batch_t *b,mesh_t *m,shader_t *s) {
    (void)b;(void)s; ++calls;vertices=m->numvertexes;indices=m->numindexes;
    assert(vertices<=405 && indices<=1920 && indices%3==0);
    for(int i=0;i<indices;++i)assert(m->indexes[i]<vertices);
    for(int i=0;i<vertices;++i) {
        for(int j=0;j<3;++j)assert(isfinite(m->xyz_array[i][j]) && fabs(m->xyz_array[i][j])<=1);
        for(int j=0;j<2;++j)assert(isfinite(m->st_array[i][j]) && m->st_array[i][j]>=0 && m->st_array[i][j]<=M_PI);
    }
}
''' + 'enum {'+','.join(symbols)+'};\n' + '''
typedef struct { int type; float args[4]; } shaderfunc_t;
typedef struct { int type; float args[6]; } tcmod_t;
typedef struct { int numtcmods; tcmod_t tcmods[8]; } shaderpass_t;
''' + clouds + transforms + '''
static void near(float a,double b) {assert(fabs(a-b)<0.00002);}
int main(void) {
    vec2_t uv,opposite,other;
    vec3_t zenith={0,0,1},horizon={1,0,0},west={-1,0,0},zero={0,0,0};
    R_Q3CloudTexCoord(zenith,512,uv);near(uv[0],M_PI/2);near(uv[1],M_PI/2);
    /* Independent horizon geometry: the cloud normal's vertical component
     * is radius/(radius+height); acos of its horizontal component is the UV. */
    R_Q3CloudTexCoord(horizon,512,uv);near(uv[0],acos(sqrt(1-pow(4096.0/4608,2))));near(uv[1],M_PI/2);
    R_Q3CloudTexCoord(west,512,opposite);near(uv[0]+opposite[0],M_PI);
    R_Q3CloudTexCoord(horizon,1024,other);assert(other[0]<uv[0]-0.1);
    for(int i=0;i<3;++i)horizon[i]*=1000;
    R_Q3CloudTexCoord(horizon,512,other);near(uv[0],other[0]);near(uv[1],other[1]);
    R_Q3CloudTexCoord(horizon,0,other);near(uv[0],other[0]);
    R_Q3CloudTexCoord(horizon,NAN,other);near(uv[0],other[0]);
    R_Q3CloudTexCoord(zero,512,other);near(other[0],M_PI/2);
    skydome_t dome={512};shader_t shader={&dome};batch_t batch={0};
    for(int i=0;i<6;++i)for(int j=0;j<2;++j){skymins[j][i]=-1;skymaxs[j][i]=1;}
    R_DrawQ3Clouds(&batch,&shader);assert(calls==1 && vertices==405 && indices==1920);
    dome.cloudheight=1024;R_DrawQ3Clouds(&batch,&shader);assert(calls==2);
    dome.cloudheight=512;R_DrawQ3Clouds(&batch,&shader);assert(calls==3);
    for(int i=0;i<6;++i)for(int j=0;j<2;++j){skymins[j][i]=0;skymaxs[j][i]=0.01f;}
    R_DrawQ3Clouds(&batch,&shader);assert(vertices==20 && indices==30);
    for(int i=0;i<6;++i)for(int j=0;j<2;++j)skymaxs[j][i]=skymins[j][i];
    int previous=calls;R_DrawQ3Clouds(&batch,&shader);assert(calls==previous);
    shaderpass_t p={0};vec3_t pos={0,0,0};
    p.numtcmods=1;p.tcmods[0].type=SHADER_TCMOD_ROTATE;p.tcmods[0].args[0]=-90.0f/360;
    uv[0]=0.75f;uv[1]=0.5f;GPU_TexCoord(uv,&p,1,pos);near(uv[0],0.5);near(uv[1],0.25);
    uv[0]=0.75f;uv[1]=0.5f;GPU_TexCoord(uv,&p,4,pos);near(uv[0],0.75);near(uv[1],0.5);
    p.numtcmods=2;p.tcmods[0].type=SHADER_TCMOD_SCALE;p.tcmods[0].args[0]=2;p.tcmods[0].args[1]=3;
    p.tcmods[1].type=SHADER_TCMOD_SCROLL;p.tcmods[1].args[0]=-0.01;p.tcmods[1].args[1]=0.02;
    uv[0]=uv[1]=M_PI/2;GPU_TexCoord(uv,&p,4,pos);near(uv[0],M_PI-0.04);near(uv[1],1.5*M_PI+0.08);
    puts("Q3 sky projection, height/cache, clipped mesh bounds, scrolling and rotation passed");
}
'''
generated = args.out/'sky-generated.c'
generated.write_text(source)
binary = args.out/'sky-tests'
subprocess.run(['clang','-std=c17','-fsanitize=address,undefined','-O1',str(generated),'-lm','-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
