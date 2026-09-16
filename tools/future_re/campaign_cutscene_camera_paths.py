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

inv=json.loads((ROOT/'artifacts/inventory.json').read_text());levels=json.loads((ROOT/'artifacts/levels.json').read_text());timelines=json.loads((ROOT/'artifacts/future-campaign-re/cutscene-timeline-loader.json').read_text());results=[];samples=0
for timeline in timelines['results']:
 level=next(l for l in levels if l['file']==timeline['file']);archive=next(a for a in inv['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file']);sections={}
 for kind in ['0x400','0x5000','0x6000']:
  section=next((x for x in level['sections'] if x['type']==kind),None)
  if not section:sections[kind]=[];continue
  with (ROOT/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
  pos=[0]
  def take(n):
   p=pos[0];assert 0<=n<=len(data)-p;pos[0]+=n;return data[p:p+n]
  def integer():return struct.unpack('<I',take(4))[0]
  def string():return take(struct.unpack('<H',take(2))[0]).decode('cp1252')
  def floats(n):return list(struct.unpack('<'+'f'*n,take(n*4)))
  records=[]
  for _ in range(integer()):
   if kind=='0x6000':
    name=string();records.append(dict(name=name,controls=[integer() for j in range(integer())]))
   else:records.append(dict(uid=integer(),name=string(),position=floats(3),orientation_disk=floats(9),text=string(),flag=take(1)[0]))
  assert pos[0]==len(data),(level['file'],kind,pos[0],len(data));sections[kind]=records
 cameras={r['uid']:r for r in sections['0x400']};controls={r['uid']:r for r in sections['0x5000']};paths={r['name'].lower():r for r in sections['0x6000']}
 for r in timeline['records']:
  for p in r['points']:assert p['camera'] in cameras
 for path in paths.values():
  assert len(path['controls'])==4;points=[controls[c]['position'] for c in path['controls']];u=machine();obj=BASE;ptrs=[]
  for i,p in enumerate(points):ptr=BASE+0x1000+i*16;u.mem_write(ptr,struct.pack('<3f',*p));ptrs.append(ptr)
  call(u,0x530160,ptrs,ecx=obj);path_samples=[]
  for t in [-.25,0,.25,.5,.75,1,1.25]:
   out=BASE+0x2000;bits=struct.unpack('<I',struct.pack('<f',t))[0];call(u,0x530060,(out,bits),ecx=obj);actual=list(struct.unpack('<3f',u.mem_read(out,12)));b=[(1-t)**3,3*t*(1-t)**2,3*t*t*(1-t),t**3];expected=[sum(b[i]*points[i][axis] for i in range(4)) for axis in range(3)];assert max(abs(a-b) for a,b in zip(actual,expected))<.0005,(actual,expected);samples+=1;path_samples.append(dict(t=t,position=actual))
  path['samples']=path_samples
 missing_paths=sorted(set(p['text'] for r in timeline['records'] for p in r['points'] if p['text'].lower() not in paths))
 results.append(dict(file=level['file'],sections=sections,unresolved_timeline_path_names=missing_paths))
report=dict(result='PASS',original_sha256=SHA,levels=len(results),samples=samples,results=results,limitations=['Authored camera/control/path resource parsing follows statically recovered readers; parser independently requires exact byte consumption.','Full original530160 constructor and530060 sampler plus basis52ff10 execute without hooks.','Camera/path resource loaders themselves not emulated in this probe.'])
(ROOT/'artifacts/future-campaign-re/cutscene-camera-paths.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'levels',samples,'actual cubic samples')
