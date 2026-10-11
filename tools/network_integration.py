#!/usr/bin/env python3
"""Exercise local QuakeWorld/DTLS connections through a lossy UDP relay.

Original game data is linked into an isolated directory. Tests drop the initial
request burst, inject malformed headers, and introduce loss, duplicates, and
reordering. Optional --baseline measures the former connection retry delay.
"""
from pathlib import Path
import argparse
import socket, subprocess, time, select, random, json, ssl, hashlib, base64
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('game',type=Path)
parser.add_argument('--out',type=Path,default=root/'build/networkdata')
parser.add_argument('--client',type=Path,default=root/'build/fteqw')
parser.add_argument('--server',type=Path,default=root/'build/all/fteqw-sv')
parser.add_argument('--baseline',type=Path)
args=parser.parse_args()
out=args.out.resolve()
source=args.game.resolve()/'id1'
if out==args.game.resolve() or args.game.resolve() in out.parents:
 raise ValueError('Output must be separate from original game data')
if not (source/'pak0.pak').is_file():raise FileNotFoundError(source/'pak0.pak')
(out/'id1').mkdir(parents=True,exist_ok=True)
for name in ('pak0.pak','pak1.pak'):
 p=out/'id1'/name
 if not p.exists():p.symlink_to(source/name)
def freeport():
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
  s.bind(('127.0.0.1',0));return s.getsockname()[1]
results={}
cases=[('after',args.client.resolve(),False),('dtls',args.client.resolve(),True),('dtls_badpin',args.client.resolve(),True)]
if args.baseline:cases.insert(0,('before',args.baseline.resolve(),False))
for case,binary,secure in cases:
 port=freeport();proxy=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);proxy.bind(('127.0.0.1',0));proxy.setblocking(False)
 serveraddr=('127.0.0.1',port);proxyport=proxy.getsockname()[1]
 common='cfg_save_auto 0\nm_preset_chosen 1\ncon_notifytime 0\n'
 security='tls_provider GNUTLS\nnet_enable_dtls 2\nnet_mtu 1200\n' if secure else 'net_enable_dtls 0\n'
 (out/'id1/server.cfg').write_text('cfg_save_auto 0\n'+security+f'sv_port 127.0.0.1:{port}\nsv_port_ipv6 ""\nsv_port_tcp ""\nsv_public 0\nsv_listen_nq 0\nsv_bigcoords 0\ndeathmatch 1\nmap start\n')
 (out/'id1/client.cfg').write_text(common+security+f'name nettest_{case}\nconnect '+('dtls://' if secure else 'qw://')+f'127.0.0.1:{proxyport}\ndefer 13 say TRANSPORT_CHECK_{case}\ndefer 15 status\ndefer 18 quit force\n')
 serverlog=(out/f'{case}-server.log').open('w');clientlog=(out/f'{case}-client.log').open('w')
 server=subprocess.Popen([str(args.server.resolve()),'-nohome','-basedir',str(out),'-quake','-ip','127.0.0.1','-port',str(port),'+exec','server.cfg'],cwd=root,stdin=subprocess.PIPE,stdout=serverlog,stderr=subprocess.STDOUT)
 client=None
 try:
  time.sleep(1)
  if secure:
   deadline=time.monotonic()+20
   while True:
    try:der=ssl.PEM_cert_to_DER_cert((out/'fullchain.pem').read_text());break
    except (FileNotFoundError,ValueError):
     if time.monotonic()>deadline:raise TimeoutError('Local test certificate generation')
     time.sleep(0.05)
   digest=bytearray(hashlib.sha256(der).digest())
   if case=='dtls_badpin':digest[0]^=1
   pin=base64.urlsafe_b64encode(digest).decode().rstrip('=')
   cfg=(out/'id1/client.cfg').read_text().replace(f'127.0.0.1:{proxyport}',f'127.0.0.1:{proxyport}?fp={pin}')
   (out/'id1/client.cfg').write_text(cfg)
  client=subprocess.Popen([str(binary),'-nohome','-basedir',str(out),'-quake','-window','-width','640','-height','480','+vid_renderer','sdlgpu','+exec','client.cfg'],cwd=root,stdout=clientlog,stderr=subprocess.STDOUT)
  start=time.monotonic();first=None;retry=None;clientaddr=None;forwarded=0;dropped=0;duplicated=0;injected=0;max_datagram=0;pending=[];rng=random.Random(11)
  while client.poll() is None:
   now=time.monotonic()
   if now-start>45:raise TimeoutError(case)
   ready,_,_=select.select([proxy],[],[],0.01)
   for data,dest,when in pending[:]:
    if now>=when:proxy.sendto(data,dest);pending.remove((data,dest,when))
   if not ready:continue
   data,addr=proxy.recvfrom(65535)
   max_datagram=max(max_datagram,len(data))
   if addr==serveraddr:
    if clientaddr is None:continue
    dest=clientaddr
   else:
    clientaddr=addr;dest=serveraddr
    if first is None:first=now
    # Lose the entire first request burst, including NQ compatibility probes.
    if not secure and now-first<0.1:
     dropped+=1;continue
    if retry is None:retry=now-first
   if first and now-first>8 and rng.random()<0.05:
    dropped+=1;continue
   if first and now-first>8 and rng.random()<0.03:
    pending.append((data,dest,now+0.06))
   else:proxy.sendto(data,dest)
   forwarded+=1
   if first and now-first>8 and rng.random()<0.02:
    proxy.sendto(data,dest);duplicated+=1
   if case == 'after' and addr==serveraddr and not data.startswith(b'\xff'*4) and len(data)>8 and injected<10:
    proxy.sendto(data[:6],dest);injected+=1
  assert client.returncode==0,(case,client.returncode)
  clientlog.flush();serverlog.flush()
  clienttext=(out/f'{case}-client.log').read_text(errors='replace');servertext=(out/f'{case}-server.log').read_text(errors='replace')
  assert 'Host_Error' not in clienttext and 'Host_EndGame' not in clienttext and 'SDL GPU error' not in clienttext,case
  if case=='dtls_badpin':
   assert 'does not match fingerprint' in clienttext,(case,'wrong pin accepted')
   assert 'Connected to' not in clienttext and 'plain-text' not in clienttext,(case,'security downgrade')
   assert 'entered the game' not in servertext,(case,'unauthenticated join')
  else:
   assert f'TRANSPORT_CHECK_{case}' in servertext,(case,'chat did not reach server')
   assert 'entered the game' in servertext,(case,'did not join')
   if secure:assert 'plain-text' not in clienttext and 'Connected to' in clienttext,(case,'encryption')
  if secure:assert max_datagram<=1200,(case,max_datagram)
  results[case]={'max_datagram':max_datagram,'retry_seconds':retry,'forwarded':forwarded,'dropped':dropped,'duplicates':duplicated,'malformed_headers':injected,'result':'wrong_certificate_rejected' if case=='dtls_badpin' else 'joined_and_reliable_chat_passed'}
  print(case,results[case],flush=True)
 finally:
  if client is not None and client.poll() is None:client.terminate();client.wait(timeout=10)
  server.stdin.write(b'quit\n');server.stdin.flush()
  try:server.wait(timeout=5)
  except subprocess.TimeoutExpired:server.terminate();server.wait(timeout=10)
  proxy.close();clientlog.close();serverlog.close()
if args.baseline:assert results['before']['retry_seconds']>4.5,results
assert results['after']['retry_seconds']<1.5,results
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
