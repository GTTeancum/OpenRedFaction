"""Bounded mutable routing workspace for every installed navigation section."""
import json,re,struct,subprocess,sys
from pathlib import Path
from inspect_navigation_records import ROOT,inspect,sections
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im)
B=0x30000000;u.mem_map(B,0x800000);SOURCE=B;OWNER=B+0x100;STACK=B+0xe000;STOP=B+0xf000;REFS=B+0x10000;NEIGHBORS=B+0x30000;NODES=B+0x80000;ALLOC=B+0x100000
maps=(ROOT/'build/xbox/main.map').read_text();symbol=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',maps)[1],16)
entry=symbol('rf_level_navigation_workspace_open');close=symbol('rf_level_navigation_workspace_close');calloc=symbol('calloc');free=symbol('free');live=False;fail=False;size=0
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def hook(m,a,n,c):
 global live,size
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==calloc:
  assert not live;size=r(sp+4)*r(sp+8);assert size<0x100000
  value=0 if fail else ALLOC
  if value:live=True;m.mem_write(ALLOC,bytes(size))
 elif a==free:
  if r(sp+4):assert live and r(sp+4)==ALLOC;live=False
  value=0
 else:return
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_EIP,r(sp));m.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook)
def call(a,*args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(a,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
rows=[]
for level,payload in sections():
 nodes=inspect(payload);n=len(nodes);edges=sum(len(v['neighbors']) for v in nodes);cursor=NEIGHBORS
 u.mem_write(SOURCE,w(0,NODES,REFS,n,0));u.mem_write(OWNER,bytes(24))
 for i,node in enumerate(nodes):
  adj=node['neighbors'];u.mem_write(REFS+i*16,w(NODES+i*68,i,cursor,len(adj)));u.mem_write(cursor,w(*adj));cursor+=4*len(adj)
 expected=24+(n+2)*32+(edges+2*n+2)*4
 assert call(entry,SOURCE,expected-1,OWNER)!=0 and not live and bytes(u.mem_read(OWNER,24))==bytes(24)
 fail=True;assert call(entry,SOURCE,expected,OWNER)!=0 and not live;fail=False
 assert call(entry,SOURCE,expected,OWNER)==0 and live
 storage,refs,lists,scratch,count,budget=struct.unpack('<6I',u.mem_read(OWNER,24));assert storage==ALLOC and count==n and budget==expected and size==expected-24
 before=bytes(u.mem_read(OWNER,24));assert call(entry,SOURCE,expected,OWNER)!=0 and bytes(u.mem_read(OWNER,24))==before
 for i in range(n+2):
  candidate,key,adj,count=struct.unpack('<4I',u.mem_read(refs+i*16,16));items,lc,capacity=struct.unpack('<3I',u.mem_read(lists+i*12,12));want=nodes[i]['neighbors'] if i<n else []
  assert key==i+1 and candidate==(NODES+i*68 if i<n else 0) and adj==items and count==lc==len(want)
  assert capacity==(len(want)+2 if i<n else (2 if i==n+1 else 0)) and bytes(u.mem_read(items,len(want)*4))==w(*want)
  if capacity>count:u.mem_write(items+count*4,w(*([n]*(capacity-count))))
 assert bytes(u.mem_read(scratch,(n+2)*4))==bytes((n+2)*4)
 call(close,OWNER);call(close,OWNER);assert not live and bytes(u.mem_read(OWNER,24))==bytes(24)
 output=subprocess.check_output([str(ROOT/'build/pc/Release/rf_level_entity_probe.exe'),str(ROOT/'Installed_Game'/level['archive']),level['file'],'--navigation-workspace'])
 assert list(map(int,output.split()))==[n,edges,expected],(level['file'],output,expected)
 rows.append(dict(file=level['file'],nodes=n,edges=edges,workspace_bytes=expected))
report=dict(result='PASS',sections=len(rows),nodes=sum(v['nodes'] for v in rows),maximum_workspace_bytes=max(v['workspace_bytes'] for v in rows),scope='PC authored loader plus routing workspace and compiled NXDK workspace from audited adjacency. Exact/insufficient budget, allocation failure, nonempty destination, borrowed candidates, nonzero ordered keys, spare links, scratch isolation and repeat close. NXDK allocator supplied; no scene binding or route execution.',levels=rows)
(ROOT/'artifacts/navigation-workspace.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='levels'})
