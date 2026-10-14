#!/usr/bin/env python3
"""Compile actual engine network functions with injected faults and sanitizers.

The core fixture covers socket errors, stale DNS results, random challenges,
fragment validation, retries, and DTLS peer cleanup. The DTLS fixture uses
real GnuTLS records to test MTU limits, blocked sends, corruption and bounds.
Requires Clang, pkg-config, and GnuTLS development files.
"""
from pathlib import Path
import argparse
import re, subprocess, shlex
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out',type=Path,default=root/'build/network-tests')
parser.add_argument('--cc',default='clang')
args=parser.parse_args()
out=args.out.resolve()
out.mkdir(parents=True,exist_ok=True)
def function(file,name):
 s=(root/file).read_text()
 m=re.search(r'^(?:static )?[^\n;]+\b'+name+r'\s*\([^;]*?\)\s*\{',s,re.M)
 assert m,name
 start=m.start();brace=s.index('{',m.start());level=1;i=brace+1
 while level:
  if s[i]=='{':level+=1
  if s[i]=='}':level-=1
  i+=1
 return s[start:i]
for name, funcs in {'core':[('engine/common/net_chan.c','Netchan_GenerateChallenge'),('engine/client/cl_main.c','CL_ResolvedServer'),('engine/client/cl_main.c','CL_ConnectRetryInterval'),('engine/common/net_wins.c','FTENET_Datagram_SendPacket'),('engine/common/net_wins.c','FTENET_Datagram_GetPacket'),('engine/common/net_wins.c','NET_DTLS_DisconnectPeer'),('engine/common/net_wins.c','NET_DTLS_Timeouts'),('engine/common/net_chan.c','Netchan_Process')], 'bits':[('engine/common/common.c','MSG_WriteBits')], 'dtls':[('engine/common/net_ssl_gnutls.c','DTLS_Push'),('engine/common/net_ssl_gnutls.c','DTLS_Pull'),('engine/common/net_ssl_gnutls.c','GNUDTLS_Transmit'),('engine/common/net_ssl_gnutls.c','GNUDTLS_Received')]}.items():
 s=(root/f'tools/tests/network_{name}.c').read_text().replace('/* ENGINE_FUNCTIONS */','\n'.join(function(f,n) for f,n in funcs))
 (out/f'{name}-generated.c').write_text(s)
 cmd=[args.cc,'-std=c17','-g','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-Wno-pointer-sign','-Wno-macro-redefined',str(out/f'{name}-generated.c'),'-o',str(out/name)]
 if name=='dtls':cmd+=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gnutls'],text=True))
 subprocess.run(cmd,check=True)
 subprocess.run([str(out/name)],check=True)
