#!/usr/bin/env python3
"""Exercise real Panini allocation code under ASan/UBSan, including bad cvars."""
import argparse
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCE=r"""
#include <assert.h>
#include <limits.h>
#include "panini.h"
int main(void) {
    unsigned w,h;
    assert(GPU_PaniniLevels(1,1)==1 && GPU_PaniniLevels(1920,1080)==11 && GPU_PaniniLevels(8192,2)==14);
    GPU_PaniniDimensions(1920,1080,1.25f,&w,&h); assert(w==2400 && h==1350);
    GPU_PaniniDimensions(1920,1080,2,&w,&h); assert(w==3840 && h==2160);
    GPU_PaniniDimensions(3840,2160,2,&w,&h); assert((double)w*h<=16000000 && w>=3840 && h>=2160);
    GPU_PaniniDimensions(6000,100,2,&w,&h); assert(w==8192 && h>=100);
    GPU_PaniniDimensions(100,6000,2,&w,&h); assert(h==8192 && w>=100);
    GPU_PaniniDimensions(10000,10000,2,&w,&h); assert(w==10000 && h==10000);
    GPU_PaniniDimensions(1,1,2,&w,&h); assert(w==2 && h==2);
    GPU_PaniniDimensions(0,600,2,&w,&h); assert(w==0 && h==600);
    const float bad[]={NAN,INFINITY,-INFINITY,-2,0};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        GPU_PaniniDimensions(800,600,bad[i],&w,&h); assert(w==800 && h==600);
    }
    GPU_PaniniDimensions(800,600,100,&w,&h); assert(w==1600 && h==1200);
    GPU_PaniniDimensions(INT_MAX,INT_MAX,2,&w,&h); assert(w==INT_MAX && h==INT_MAX);
    // Pixel and axis caps must hold across aspect ratios and rounding.
    for(unsigned x=1;x<10000;x+=137) for(unsigned y=1;y<5000;y+=113) {
        GPU_PaniniDimensions(x,y,2,&w,&h);
        assert(w>=x && h>=y);
        assert((double)w*h<=fmax(16000000.0,(double)x*y));
        assert(w<=fmax(8192.0,x) && h<=fmax(8192.0,y));
    }
    return 0;
}
"""
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,default=Path('/tmp/fte-panini-unit'))
    args=p.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    source=args.out/'limits.c'; source.write_text(SOURCE)
    binary=args.out/'limits'
    subprocess.run(['clang','-std=c17','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-I',str(ROOT/'engine/sdlgpu'),str(source),
        '-o',str(binary),'-lm'],check=True)
    subprocess.run([str(binary)],check=True)
    print('Panini source limits passed (ASan/UBSan).')
if __name__=='__main__': main()
