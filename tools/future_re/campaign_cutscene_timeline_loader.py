"""Original cutscene completion broadcast and lifecycle boundary evidence; no port build."""
import sys,struct,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'local/python'))
(ROOT/'artifacts/future-campaign-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
EXE=ROOT/'Installed_Game/RF.exe';SHA=hashlib.sha256(EXE.read_bytes()).hexdigest()
assert SHA=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
IMAGE=pefile.PE(str(EXE)).get_memory_mapped_image()
BASE=0x30000000;STACK=BASE+0x7e000;STOP=BASE+0x7f000
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
def machine():
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(IMAGE)+4095)//4096*4096);u.mem_write(0x400000,IMAGE);u.mem_map(BASE,0x80000);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
def word(u,a):return struct.unpack('<I',u.mem_read(a,4))[0]
def call(u,entry,args=(),ecx=0):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,ecx);u.emu_start(entry,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP

def return_boundary(u,value=0,pop=0):
 sp=u.reg_read(UC_X86_REG_ESP);ret=word(u,sp);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)

# Parse the version180 original RFL section identified at460f63..460f8c.
inv=json.loads((ROOT/'artifacts/inventory.json').read_text()); levels=json.loads((ROOT/'artifacts/levels.json').read_text());events=json.loads((ROOT/'artifacts/events.json').read_text());results=[]
for level in levels:
 section=next((x for x in level['sections'] if x['type']=='0x4000'),None)
 if not section:continue
 archive=next(a for a in inv['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (ROOT/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
 pos=[0]
 def take(n):
  p=pos[0];assert 0<=n<=len(data)-p;pos[0]+=n;return data[p:p+n]
 def integer():return struct.unpack('<I',take(4))[0]
 def floating():return struct.unpack('<f',take(4))[0]
 def string():return take(struct.unpack('<H',take(2))[0]).decode('cp1252')
 records=[]
 for _ in range(integer()):
  selector=integer();hide=take(1)[0];fov=floating();points=[]
  for j in range(integer()):points.append(dict(camera=integer(),durations=[floating(),floating(),floating()],words=[integer(),integer()],text=string()))
  records.append(dict(selector=selector,hide=hide,fov=fov,points=points))
 assert pos[0]==len(data)
 authored=[e['uid'] for l in events['results'] if l['file']==level['file'] for e in l['records'] if e['type_index']==55]
 assert [r['selector'] for r in records]==authored
 # Full465d50, actual versioned primitive readers52c780/52c910/52c9b0.
 # File bytes, strings, allocation/construction and registry append are boundaries.
 u=machine();u.mem_map(0,4096);cursor=[0];heap=[BASE+0x10000];objects=[];strings={}
 def hook(cpu,a,n,unused):
  sp=cpu.reg_read(UC_X86_REG_ESP)
  if a==0x523990:return_boundary(cpu,1,4) # version180 includes thresholds143 and146
  elif a==0x524530:return_boundary(cpu,0)
  elif a==0x52cf60:
   dst,size=word(cpu,sp+4),word(cpu,sp+8);p=cursor[0];assert p+size<=len(data);cpu.mem_write(dst,data[p:p+size]);cursor[0]+=size;return_boundary(cpu,size,16)
  elif a==0x573619:
   size=word(cpu,sp+4);out=heap[0];heap[0]+=(size+15)&~15;return_boundary(cpu,out)
  elif a==0x466180:return_boundary(cpu,cpu.reg_read(UC_X86_REG_ECX))
  elif a==0x52c720:
   dst=word(cpu,sp+8);p=cursor[0];size=struct.unpack_from('<H',data,p)[0];p+=2;strings[dst]=data[p:p+size].decode('cp1252');cursor[0]=p+size;return_boundary(cpu)
  elif a==0x45ec40:objects.append(word(cpu,sp+4));return_boundary(cpu,0,4)
 u.hook_add(UC_HOOK_CODE,hook);call(u,0x465d50,(BASE+0x50000,));assert cursor[0]==len(data) and len(objects)==len(records)
 for obj,r in zip(objects,records):
  assert word(u,obj)==r['selector'] and word(u,obj+4)==len(r['points']);assert u.mem_read(obj+0x859,1)[0]==r['hide'];assert struct.unpack('<f',u.mem_read(obj+0x85c,4))[0]==r['fov']
  for j,p in enumerate(r['points']):
   pt=obj+8+j*32;assert word(u,pt)==p['camera'];assert list(struct.unpack('<3f',u.mem_read(pt+4,12)))==p['durations'];assert [word(u,pt+16),word(u,pt+20)]==p['words'];assert strings[pt+24]==p['text']
   # Original sum routine returns ST0; trampoline stores float then returns.
   stub=BASE+0x60000+j*64;out=BASE+0x65000
   cpu_bytes=b'\x68'+w(j)+b'\x68'+w(obj)+b'\xb8'+w(0x45b200)+b'\xff\xd0\x83\xc4\x08\xd9\x1d'+w(out)+b'\xc3'
   u.mem_write(stub,cpu_bytes);call(u,stub);total=struct.unpack('<f',u.mem_read(out,4))[0];expected=struct.unpack('<f',struct.pack('<f',sum(p['durations'])))[0];assert total==expected,(total,expected,p);p['duration_sum']=total
 results.append(dict(file=level['file'],bytes=len(data),records=records))
report=dict(result='PASS',original_sha256=SHA,levels=len(results),timelines=sum(len(x['records']) for x in results),points=sum(len(r['points']) for x in results for r in x['records']),results=results,limitations=['File transport, allocation, constructor initialization, string internals and registry append intercepted.','Full original465d50 loader and primitive value readers executed;52c720 string bytes decoded by boundary.','Version180 authored files only; camera interpolation and natural completion not exercised.'])
(ROOT/'artifacts/future-campaign-re/cutscene-timeline-loader.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',report['levels'],'timelines',report['points'],'point records')
