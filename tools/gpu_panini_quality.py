#!/usr/bin/env python3
"""Measure Panini center detail against a higher-resolution perspective reference.

Uses owned Quake data in an isolated directory. Includes uncapped-throughput
measurements; these are scene-specific, not a guarantee of gameplay FPS.
"""
import argparse
import json
import math
from pathlib import Path
import re
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from gpu_panini import prepare

def prepare_quality(game,out,qcc):
    prepare(game,out,qcc)
    commands=['ruleset_allow_in 1','cfg_save_auto 0','m_preset_chosen 1',
        'con_notifytime 0','sv_public 0','set r_bloom 0','r_fxaa 0','r_hdr_framebuffer 0',
        'r_shadow_realtime_world 0','r_shadow_realtime_dlight 0','r_dynamic 0',
        'gamma 1','contrast 1','brightness 0','v_contrastboost 1','vid_sceneonlygamma 0',
        'set gpu_panini_overlay 0','set gpu_panini_layout 0',
        'cl_maxfps 1000','cl_yieldcpu 0','cl_idlefps 0','vid_vsync 0',
        'sv_csqc_progname gpu-panini.dat','map start','defer 5 pause']
    t=7
    def step(*cmds,shot=None,delay=3):
        nonlocal t
        name=f'quality_step_{t}'
        commands.append('alias '+name+' "'+'; '.join(cmds)+'"')
        commands.append(f'defer {t} {name}')
        if shot: commands.append(f'defer {t+2} screenshot quality-{shot}.png')
        t+=delay
    for fov in (150,170):
        step(f'set gpu_panini_fov {fov}','r_projection 0','vid_width 1200','vid_height 800',
             'vid_restart',shot=f'{fov}-reference',delay=4)
        step('vid_width 600','vid_height 400','vid_restart',delay=3)
        for label,scale,filtering,sharp in [('legacy',1,0,0),('cubic',1,1,0),('source',1.25,0,0),
                                          ('balanced',1.25,1,0.2),('maximum',2,1,0.2),('restored',1,0,0)]:
            step('r_projection 6',f'r_panini_scale {scale}',f'r_panini_filter {filtering}',
                 f'r_panini_sharpen {sharp}',shot=f'{fov}-{label}')
    step('vid_fullscreen 2','vid_restart','vid_vsync 0',delay=4)
    for label,projection,scale,filtering,sharp in [('regular',0,1,0,0),('legacy',6,1,0,0),
          ('cubic',6,1,1,0),('source',6,1.25,0,0),('balanced',6,1.25,1,0.2),('maximum',6,2,1,0.2)]:
        step(f'r_projection {projection}',f'r_panini_scale {scale}',f'r_panini_filter {filtering}',
             f'r_panini_sharpen {sharp}',delay=2)
        step(f'gpu_benchmark 4 {label}',delay=5)
    step('gpu_info','quit force',delay=1)
    (out/'id1/gpu-test.cfg').write_text('\n'.join(commands)+'\n')
    return t

def check(out):
    from PIL import Image,ImageChops,ImageStat
    log=(out/'quality.log').read_text(errors='replace')
    assert 'SDL3 GPU renderer: metal' in log or 'SDL3 GPU renderer: vulkan' in log
    assert not re.search(r'Host_Error|Sys_Error|Validation Error|SDL3 GPU:.*error',log,re.I)
    def image(name):
        matches=list(out.rglob(f'quality-{name}.png'));assert len(matches)==1,(name,matches)
        return Image.open(matches[0]).convert('RGB')
    def bilinear(im,x,y):
        x=max(0,min(im.width-1,x));y=max(0,min(im.height-1,y))
        ix,iy=math.floor(x),math.floor(y);tx,ty=x-ix,y-iy
        return tuple(sum(im.getpixel((min(im.width-1,ix+dx),min(im.height-1,iy+dy)))[c]*
            (tx if dx else 1-tx)*(ty if dy else 1-ty) for dx in (0,1) for dy in (0,1)) for c in range(3))
    results={}
    for fov in (150,170):
        reference=image(f'{fov}-reference');legacy=image(f'{fov}-legacy')
        assert reference.size==(legacy.width*2,legacy.height*2),(reference.size,legacy.size)
        # Independent ray equations; focus on the central third where source
        # magnification loses the most detail. Compare identical paused views.
        tangent=math.tan(math.radians(fov/2));extent=2*math.tan(math.radians(fov/4))
        samples=[]
        for y in range(legacy.height//3,legacy.height*2//3,3):
            for x in range(legacy.width//3,legacy.width*2//3,3):
                nx=2*(x+0.5)/legacy.width-1;ny=2*(y+0.5)/legacy.height-1
                longitude=2*math.atan(nx*extent/2)
                px=math.tan(longitude)/tangent
                py=ny*extent/tangent*(1+math.cos(longitude))/(2*math.cos(longitude))
                samples.append((x,y,bilinear(reference,(px+1)*reference.width/2-0.5,
                                            (py+1)*reference.height/2-0.5)))
        errors={}
        for label in ('legacy','cubic','source','balanced','maximum','restored'):
            rendered=image(f'{fov}-{label}'); assert rendered.size==legacy.size
            errors[label]=sum(sum(abs(a-b) for a,b in zip(expected,rendered.getpixel((x,y))))/3
                              for x,y,expected in samples)/len(samples)
        stability=sum(ImageStat.Stat(ImageChops.difference(legacy,image(f'{fov}-restored'))).mean)/3
        assert stability<0.5,('restored source changed',stability)
        assert errors['source']<errors['legacy'],('higher resolution did not improve detail',fov,errors)
        assert errors['balanced']<errors['legacy'],('balanced quality did not improve detail',fov,errors)
        assert errors['maximum']<errors['legacy'],('maximum quality did not improve detail',fov,errors)
        results[str(fov)]={'center_rgb_error':errors,'restored_difference':stability,
                          'output_size':legacy.size,'reference_size':reference.size}
    pattern=r'SDL GPU benchmark (\S+) (\d+)x(\d+): ([\d.]+) fps, ([\d.]+) ms/frame, p50 ([\d.]+) ms, p95 ([\d.]+) ms'
    benchmarks={m[0]:dict(zip(('width','height','fps','frame_ms','p50_ms','p95_ms'),map(float,m[1:])))
                for m in re.findall(pattern,log)}
    assert len(benchmarks)==6,benchmarks
    result={'detail':results,'benchmarks':benchmarks, 'presentation_limited': all(58 < b['fps'] < 62 for b in benchmarks.values())}
    (out/'quality-results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=('prepare','run','check'));p.add_argument('game',nargs='?',type=Path)
    p.add_argument('--out',type=Path,default=ROOT/'build/paniniqualitydata')
    p.add_argument('--engine',type=Path,default=ROOT/'build/fteqw')
    p.add_argument('--qcc',type=Path,default=ROOT/'build/all/fteqcc')
    args=p.parse_args();out=args.out.resolve()
    if args.action=='prepare':
        if not args.game:p.error('prepare requires the Quake folder')
        print('Scheduled seconds:',prepare_quality(args.game,out,args.qcc))
    elif args.action=='run':
        with (out/'quality.log').open('w') as log:
            subprocess.run([str(args.engine.resolve()),'-nohome','-basedir',str(out),'-quake',
                '-window','-width','600','-height','400','+vid_conautoscale','0',
                '+vid_conwidth','800','+vid_conheight','600','+exec','gpu-test.cfg'],
                cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=150)
        check(out)
    else:check(out)
if __name__=='__main__':main()
