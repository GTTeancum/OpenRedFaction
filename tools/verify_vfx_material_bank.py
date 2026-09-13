"""Retained VFX material banks and global mesh material resolution."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
B=0x30000000;ARCH=B+0x1000;OUT=B+0x2000;NAME=B+0x3000;HEAP=B+0x4000;BUF=B+0x5000;STACK=B+0xe000;STOP=B+0xf000
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(B,65536)
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
HEAP=0x31000000;x.mem_map(HEAP,1048576);BANK=B+0x6000;VALUE=B+0x6100;MESH=B+0x7000;ID=B+0x7200
word=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
state=dict(data=b'',live=False,fail_malloc=False,reads=0,fail_read=0)
def hook(u,a,size,data):
 sp=u.reg_read(UC_X86_REG_ESP);args=[word(sp+4+i*4) for i in range(5)];result=0
 if a==sym('rf_vpp_read'):
  state['reads']+=1
  if state['reads']==state['fail_read']:result=-1
  else:
   chunk=state['data'][args[2]:args[2]+args[4]];assert len(chunk)==args[4];u.mem_write(args[3],chunk)
 elif a==sym('malloc'):
  assert not state['live'] and 0<args[0]<=1048576
  if not state['fail_malloc']:state['live']=True;u.mem_write(HEAP,b'\xa5'*args[0]);result=HEAP
 else:
  if args[0]:assert args[0]==HEAP and state['live'];state['live']=False
 u.reg_write(UC_X86_REG_EAX,result&0xffffffff);u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
for name in ('rf_vpp_read','malloc','free'):x.hook_add(UC_HOOK_CODE,hook,begin=sym(name),end=sym(name))
def run(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(sym(name),STOP,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='meshes.vpp');reports=[];guards=0;checks=0
for asset in json.loads((root/'artifacts/vfx-header.json').read_text())['assets']:
 e=next(e for e in archive['vpp']['entries'] if e['name']==asset['name'])
 with (root/'Installed_Game/meshes.vpp').open('rb') as file:file.seek(e['offset']);data=file.read(e['size'])
 chunks=[];at=asset['header_bytes'];version=int(asset['version'],16)
 while at<len(data):
  tag,length=struct.unpack_from('<II',data,at);chunks.append((tag,at+8,length-4));at+=4+length
 mats=[c for c in chunks if c[0]==0x4c54414d];size=sum(c[2] for c in mats);budget=20+len(mats)*208+size
 x.mem_write(OUT,bytes(216));x.mem_write(OUT,w(ARCH));x.mem_write(OUT+76,w(version));x.mem_write(OUT+204,w(BUF,len(chunks),0));x.mem_write(BUF,b''.join(w(*c) for c in chunks));x.mem_write(BANK,w(0));state.update(data=data,reads=0)
 assert run('rf_vfx_material_bank_open',[OUT,budget-1,BANK])!=0 and word(BANK)==0 and not state['live'];guards+=1
 state['fail_malloc']=True;assert run('rf_vfx_material_bank_open',[OUT,budget,BANK])!=0 and word(BANK)==0;state['fail_malloc']=False;guards+=1
 for n in range(1,len(mats)+1):
  state.update(reads=0,fail_read=n);assert run('rf_vfx_material_bank_open',[OUT,budget,BANK])!=0 and word(BANK)==0 and not state['live'];guards+=1
 if mats:
  start=mats[0][1];state.update(data=data[:start]+w(0xffffffff)+data[start+4:],reads=0,fail_read=0)
  assert run('rf_vfx_material_bank_open',[OUT,budget,BANK])!=0 and word(BANK)==0 and not state['live'];guards+=1
 state.update(data=data,reads=0,fail_read=0);assert run('rf_vfx_material_bank_open',[OUT,budget,BANK])==0;assert word(BANK)==HEAP
 assert bytes(x.mem_read(HEAP,12))==w(len(mats),size,budget);views=word(HEAP+12);raw=word(HEAP+16);result=bytes(x.mem_read(HEAP,12+0))+bytes(x.mem_read(views,len(mats)*208))+bytes(x.mem_read(raw,size))
 expected_views=b'';offset=0
 for tag,start,length in mats:
  x.mem_write(B,data[start:start+length]);assert run('rf_vfx_material_read',[B,length,version,NAME])==0;v=list(struct.unpack('<52I',x.mem_read(NAME,208)))
  for c,p in [(31,32),(46,47),(48,49)]:
   if v[c]:v[p]+=offset
  expected_views+=w(*v);offset+=length
 assert result==w(len(mats),size,budget)+expected_views+b''.join(data[s:s+n] for t,s,n in mats)
 state['data']=b'' # No archive dependency after open.
 for i in range(len(mats)):
  id=len(mats)-1-i;x.mem_write(ID,w(id));x.mem_write(MESH,bytes(308));x.mem_write(MESH+280,w(0x40006,1,0,4));x.mem_write(MESH+304,w(ID))
  for track in range(3):
   for time in (0,.5,2,10000):
    bits=struct.unpack('<I',struct.pack('<f',time))[0];x.mem_write(VALUE,b'\xa5'*4);status=run('rf_vfx_mesh_material_sample',[MESH,HEAP,0,track,bits,VALUE]);value=bytes(x.mem_read(VALUE,4));result+=w(status)+value
    x.mem_write(VALUE,b'\xa5'*4);reference=run('rf_vfx_material_evaluate',[raw,size,views+id*208,track,bits,VALUE]);assert reference==status and bytes(x.mem_read(VALUE,4))==value;checks+=1
 run('rf_vfx_material_bank_close',[BANK]);run('rf_vfx_material_bank_close',[BANK]);assert not state['live'] and word(BANK)==0
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--vfx-material-bank',str(root/'Installed_Game/meshes.vpp'),asset['name']]);assert pc==result
 reports.append(dict(name=asset['name'],materials=len(mats),allocated_bytes=budget))
report=dict(result='PASS',assets=reports,scalar_bindings=checks,budget_allocation_io_guards=guards,scope='PC archive load/close and compiled NXDK with supplied archive/heap services. All retained views/raw arrays and reversed global ID lookups match; no renderer/native XEMU.')
(root/'artifacts/vfx-material-bank.json').write_text(json.dumps(report,indent=2));print(report)
