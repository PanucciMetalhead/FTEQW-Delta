#!/usr/bin/env python3
"""Exercise native menus with a temporary input plugin; never modify supplied game data."""
from pathlib import Path
import subprocess, json, re, shutil, sys
import argparse, os

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description='Check native display rollback, menu help, and crisp filtering using isolated Quake data. Requires cc, pkg-config, SDL3 development files and Pillow on macOS/Linux.')
parser.add_argument('game', type=Path, help='Game directory containing id1/pak0.pak')
parser.add_argument('--out', type=Path, default=ROOT/'build/menu-qol')
parser.add_argument('--engine', type=Path, default=ROOT/'build/fteqw')
args=parser.parse_args()
OUT=args.out.resolve()
game=args.game.resolve()
engine=args.engine.resolve()
if not (game/'id1/pak0.pak').is_file(): raise FileNotFoundError(game/'id1/pak0.pak')
if OUT.exists(): raise FileExistsError(f'Choose a new output directory: {OUT}')
if OUT==game or game in OUT.parents: raise ValueError('Output must be separate from the original game data')
(OUT/'id1').mkdir(parents=True)
for name in ['pak0.pak','pak1.pak']:
 source=game/'id1'/name
 if not source.is_file(): continue
 try: (OUT/'id1'/name).symlink_to(source)
 except OSError: shutil.copyfile(source,OUT/'id1'/name)
(OUT/'probe.c').write_text(r'''
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
static pluginputfuncs_t *input;
static plug2dfuncs_t *draw;
static void key(void) {
 char name[64],count[32];int i,code;
 cmdfuncs->Argv(1,name,sizeof(name));cmdfuncs->Argv(2,count,sizeof(count));
 code=input->GetKeyCode(name,NULL);
 for(i=0;i<(*count?atoi(count):1);i++) {
  input->KeyEvent(0,1,code,0);input->KeyEvent(0,0,code,0);
 }
}
static void report(void) {
 char tag[64],world[128],ui[128],msg[1024];float size[2];unsigned int pixels[2];
 cmdfuncs->Argv(1,tag,sizeof(tag));
 cvarfuncs->GetString("gl_texturemode",world,sizeof(world));
 cvarfuncs->GetString("gl_texturemode2d",ui,sizeof(ui));
 draw->GetVideoSize(size,pixels);
 snprintf(msg,sizeof(msg),"QOL_REPORT %s fullscreen=%g width=%g height=%g world=%s ui=%s pixels=%ux%u\n",tag,
  cvarfuncs->GetFloat("vid_fullscreen"),cvarfuncs->GetFloat("vid_width"),cvarfuncs->GetFloat("vid_height"),world,ui,pixels[0],pixels[1]);
 plugfuncs->Print(msg);
}
static void idle(void) {
 int count;char action[32];SDL_Window **windows=SDL_GetWindows(&count);
 cmdfuncs->Argv(1,action,sizeof(action));
 if(count>0) {
  if(action[0]=='1') SDL_MinimizeWindow(windows[0]);
  else SDL_RestoreWindow(windows[0]);
 }
 SDL_free(windows);
}
qboolean Plug_Init(void) {
 input=plugfuncs->GetEngineInterface(pluginputfuncs_name,sizeof(*input));
 draw=plugfuncs->GetEngineInterface(plug2dfuncs_name,sizeof(*draw));
 if (!input || !draw || !cmdfuncs->AddCommand("qol_idle",idle,"Isolated minimize/restore test") || !cmdfuncs->AddCommand("qol_key",key,"Isolated menu test input") ||
  !cmdfuncs->AddCommand("qol_report",report,"Isolated menu state report")) return false;
 cmdfuncs->AddText("defer 1 exec qol-tests.cfg\n",false);
 return true;
}
''')
subprocess.run([os.environ.get('CC','cc'),'-std=c17','-shared','-fPIC','-DFTEPLUGIN','-DFTE_LITTLE_ENDIAN' if sys.byteorder=='little' else '-DFTE_BIG_ENDIAN','-I'+str(ROOT/'plugins'),'-I'+str(ROOT/'engine/common'),'-I'+str(ROOT/'engine/client'),'-I'+str(ROOT/'engine/qclib'),str(OUT/'probe.c'),str(ROOT/'plugins/plugin.c'),*subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],text=True).split(),'-o',str(OUT/'fteplug_qolprobe.so')],check=True)
commands=['cfg_save_auto 0','m_preset_chosen 1','con_notifytime 0','sv_public 0','vid_conwidth 800','vid_conheight 600',
'defer 3 qol_report defaults',
'defer 4 menu_video','defer 4.2 vid_fullscreen 2','defer 5 qol_key enter',
'defer 7 screenshot qol-confirm.png','defer 7.2 qol_report waiting','defer 8 qol_idle 1','defer 21 qol_idle 0',
'defer 22 qol_report timeout','defer 23 menu_video','defer 23.2 vid_fullscreen 2','defer 24 qol_key enter',
'defer 26 qol_key y','defer 27 qol_report kept',
'defer 28 menu_video','defer 28.2 vid_fullscreen 0','defer 29 qol_key enter',
'defer 31 qol_key escape','defer 32 qol_report escape',
'defer 33 menu_video','defer 33.2 vid_fullscreen 0','defer 34 qol_key enter',
'defer 36 qol_key enter','defer 37 qol_report enter-revert',
'defer 38 menu_video','defer 38.2 vid_fullscreen 0','defer 39 qol_key enter',
'defer 41 qol_key y','defer 42 qol_report windowed',
'defer 43 menu_video','defer 43.2 vid_fullscreen 2','defer 44 qol_key enter','defer 46 vid_revert','defer 47 qol_report console-revert',
'defer 48 menu_options','defer 48.2 qol_key downarrow','defer 48.4 qol_key f1','defer 49 screenshot qol-help.png','defer 49.2 qol_key f1','defer 49.4 screenshot qol-help-off.png']
t=50
for preset in ['286','fast','spasm','vanilla','normal','nice','realtime']:
 commands += [f'defer {t} fps_preset builtin_{preset}',f'defer {t+.5} qol_report preset-{preset}'];t+=1
commands += [f'defer {t} gl_texturemode lll',f'defer {t} gl_texturemode2d GL_LINEAR',f'defer {t+.5} qol_report manual',f'defer {t+1} menu_options',f'defer {t+1.1} qol_key downarrow',f'defer {t+1.2} qol_key rightarrow',f'defer {t+1.5} cfg_save_auto',f'defer {t+2} qol_key del',f'defer {t+2.2} cfg_save_auto',f'defer {t+3} menu_video',f'defer {t+3.2} qol_key downarrow 2',f'defer {t+3.4} qol_key rightarrow',f'defer {t+3.6} qol_key f1',f'defer {t+4} screenshot qol-pending.png',f'defer {t+5} quit force']
# Cold shader initialization can outlast startup timers. Start the fixture from
# the probe's successful initialization, after all its input commands exist.
(OUT/'id1/qol-tests.cfg').write_text('\n'.join(commands)+'\n')
(OUT/'id1/qol.cfg').write_text('cfg_save_auto 0\nm_preset_chosen 1\ndefer 2 plug_load qolprobe\n')
with (OUT/'console.log').open('w') as log:
 result=subprocess.run([str(engine),'-basedir',str(OUT),'-nohome','-quake','-window','-width','800','-height','600','+exec','qol.cfg'],cwd=OUT,stdout=log,stderr=subprocess.STDOUT,timeout=100)
print('Engine exit:',result.returncode)
text=(OUT/'console.log').read_text(errors='replace')
for line in text.splitlines():
 if 'QOL_REPORT' in line or 'restored.' in line or 'kept.' in line or 'Error' in line or "Couldn't" in line: print(line)
assert result.returncode==0
assert not re.search('Host_Error|Sys_Error|SDL GPU error',text,re.I)
rows={m[1]:m.groups()[1:] for m in re.finditer(r'QOL_REPORT (\S+) fullscreen=(\S+) width=(\S+) height=(\S+) world=(\S+) ui=(\S+) pixels=(\d+x\d+)',text)}
for name,val in [('timeout','0'),('kept','2'),('escape','2'),('enter-revert','2'),('windowed','0'),('console-revert','0')]: assert rows[name][0]==val,(name,rows.get(name))
for name in ['defaults']+['preset-'+p for p in ['286','fast','spasm','vanilla','normal','nice','realtime']]: assert rows[name][3:5]==('nll','GL_NEAREST'),(name,rows.get(name))
for name in ['timeout','kept','escape','enter-revert','windowed','console-revert']:
 assert rows[name][1:3]==('800','600'),(name,rows[name])
assert rows['manual'][3:5]==('lll','GL_LINEAR')
assert text.count('Previous display settings restored.')==4
assert text.count('Display settings kept.')==2
print('Minimized timeout, Keep, Escape, default Revert, console rollback, Retina dimensions, seven presets and manual overrides passed.')

from PIL import Image, ImageChops, ImageStat
on=Image.open(OUT/'fte/qol-help.png').convert('RGB')
off=Image.open(OUT/'fte/qol-help-off.png').convert('RGB')
assert on.size==off.size
box=(0,on.height*2//3,on.width//2,on.height)
difference=sum(ImageStat.Stat(ImageChops.difference(on.crop(box),off.crop(box))).mean)/3
assert difference>0.3, ('F1 help did not appear',difference)
assert '"cfg_save_auto" is "1"' in text and '"cfg_save_auto" is "0"' in text
print(f'F1 help image difference: {difference:.3f}; Delete reset passed.')
print(f'Inspect qol-confirm.png and qol-pending.png in {OUT/"fte"} for countdown and pending-value layout.')

results={'display_and_filter_states':rows,'f1_help_difference':difference,'delete_reset':'passed'}
(OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
