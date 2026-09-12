"""Owned glare table lifetime on PC and compiled NXDK; bitmap resources excluded."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x800000);O=B+0x1000;A=B+0x100000;S=B+0x700000;STOP=S+0x1000
sym=(root/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',sym)[1],16)
entry,close,find,read,malloc,free=map(symbol,('rf_glare_classes_open','rf_glare_classes_close','rf_vpp_find','rf_vpp_read','malloc','free'))
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
allocations=0;fail_alloc=0;fail_read=False;live={};cases=0

def hook(cpu,address,length,context):
 global allocations
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);result=0
 if address==find:
  assert arg(0)==B and bytes(cpu.mem_read(arg(1),12))==b'effects.tbl\0'
  cpu.mem_write(arg(2),b'effects.tbl'.ljust(64,b'\0')+w(0,len(raw)))
 elif address==read:
  assert arg(0)==B and arg(2)==0 and arg(4)==len(raw)
  if fail_read:result=0xffffffff
  else:cpu.mem_write(arg(3),raw)
 elif address==malloc:
  allocations+=1;n=arg(0);assert 0<n<0x100000
  if allocations!=fail_alloc:
   result=A+(allocations-1)*0x100000;live[result]=n;cpu.mem_write(result,b'\xa5'*n)
 elif arg(0):
  n=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*n)
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for address in (find,read,malloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
def call(function,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(function,STOP,count=100000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def check(expected,budget=1000000,pc=False):
 global allocations,cases
 assert not live;allocations=0;x.mem_write(O,bytes(20));status=call(entry,B,budget,O)
 if expected is None:
  assert status and not live and bytes(x.mem_read(O,20))==bytes(20);out=w(status,0,0,0)
 else:
  assert status==0,hex(status)
  count=len(expected);retained=20+312*count;peak=retained+len(raw)
  assert bytes(x.mem_read(O+8,12))==w(count,retained,peak)
  assert len(live)==bool(count)
  out=w(0,count,retained,peak)
  for i,value in enumerate(expected):
   d=r(O)+300*i;view=r(O+4)+12*i
   assert bytes(x.mem_read(d,300))==value
   assert bytes(x.mem_read(view,12))==value[288:296]+w(d)
   out+=value
  # A second open must reject the occupied owner without mutation.
  before=bytes(x.mem_read(O,20));assert call(entry,B,budget,O)!=0 and bytes(x.mem_read(O,20))==before
 if pc:
  actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--glare-classes',str(root/'Installed_Game/tables.vpp'),str(budget)])
  assert actual==out,'PC/NXDK owner bytes'
 call(close,O);call(close,O);assert not live and bytes(x.mem_read(O,20))==bytes(20);cases+=1
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];e=next(e for a in inv if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='effects.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
original=raw;section=raw.split(b'#Glares',1)[1].split(b'#End',1)[0]
names=re.findall(rb'\$Name:\s*"([^"]*)"',section);expected=[]
path=root/'artifacts/glare-classes-input.tbl';path.write_bytes(raw)
for name in names:
 value=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--glare-definition',str(path),name.decode('cp1252')]);assert value[:4]==w(0);expected.append(value[4:])
peak=20+312*len(expected)+len(raw)
check(expected,pc=True);check(expected,peak,pc=True);check(None,peak-1,pc=True)
for fail_alloc in (1,2):check(None)
fail_alloc=0;fail_read=True;check(None);fail_read=False
base=b'$Name: "duplicate"\n$Light Color: {1,2,3}\n'
value=b'duplicate'.ljust(256,b'\0')+w(1,2,3)+bytes(32)
raw=b'#Glares\n'+base+base.replace(b'{1,2,3}',b'{4,5,6}')+b'#End\n'
check([value,value[:256]+w(4,5,6)+value[268:]])
raw=b'#Glares\n#End\n';check([])
raw=b'#Glares\n'+base*64+b'#End\n';check([value]*64)
raw=b'#Glares\n'+base*65+b'#End\n';check(None)
for raw in (b'#Glares\n'+base, b'#Glares\n'+base+b'$Volumetric Bitmap: "x"\n#End\n',b'#Glares\n'+base+base.replace(b'{1,2,3}',b'{256,2,3}')+b'#End\n'):
 check(None)
report=dict(result='PASS',cases=cases,authored_classes=len(names),retained_bytes=20+312*len(names),peak_bytes=peak,scope='PC archive-close survival and compiled NXDK authored definitions, exact/short budgets, duplicate ordinal mapping, 64-row cap, malformed rows, read/allocation failures, occupied-owner rejection and repeated retirement. Definition contents separately checked by verify_glare_definition.py. No bitmap residency or native XEMU claim.')
(root/'artifacts/glare-classes.json').write_text(json.dumps(report,indent=2));print(report)
