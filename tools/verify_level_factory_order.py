"""Audit actual post-geometry level-section dispatch order at loader boundaries."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;stack=b+0xe000;u.mem_map(b,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
routes={0x200:0x461b70,0x300:0x461ef0,0x400:0x4659b0,0x500:0x461ff0,0x600:0x462150,0x700:0x462b20,0xa00:0x45fcf0,0xb00:0x462ca0,0xd00:0x462e40,0xe00:0x460160,0xf00:0x4604a0,0x1000:0x463210,0x1100:0x462f60,0x3000:0x463820,0x4000:0x465d50,0x5000:0x465b90,0x6000:0x465c80,0x10000:0x468c50,0x20000:0x463d50,0x30000:0x464010,0x40000:0x464fe0,0x50000:0x465220,0x60000:0x465510,0x70000:0x463d20,0x4000000:0x461ef0}
geometry=[0x573b37,0x514c40,0x52cf60,0x514c50,0x5239c0,0x5153a0,0x463c60,0x573c71]
boundaries=set(routes.values())|set(geometry)|{0x4614e0,0x524400}
sections=[];cursor=0;trace=[];current=None;version=180

def hook(m,address,size,data):
 global cursor,current
 if address not in boundaries:return
 sp=m.reg_read(UC_X86_REG_ESP);ret=read(sp);result=0
 if address==0x4614e0:
  assert read(sp+4)==stack+0x38,(hex(address),cursor,hex(sp-stack),hex(read(sp+4)-stack))
  if cursor<len(sections):
   current=sections[cursor];cursor+=1;m.mem_write(read(sp+8),w(current['type']));m.mem_write(read(sp+12),w(current['size']));result=1
 elif address==0x5239c0:
  result=version
  if current['type']==0x2000:trace.append([current['type'],address])
 else:
  assert current is not None
  trace.append([current['type'],address])
  if address==0x524400:assert read(sp+4)==current['size'] and read(sp+8)==1
  if address in routes.values():
   assert read(sp+4)==stack+0x38,(hex(address),cursor,hex(sp-stack),hex(read(sp+4)-stack))
   if address==0x461ef0:assert read(sp+8)==(current['type']==0x4000000)
  if address==0x573b37:result=b+0x1000
 m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4+{0x524400:8,0x52cf60:16,0x514c50:12,0x5153a0:4}.get(address,0));m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
levels=json.loads((root/'artifacts/levels.json').read_text());inventory=json.loads((root/'artifacts/inventory.json').read_text());reports=[];cases=0
for level in levels:
 archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (root/'Installed_Game'/level['archive']).open('rb') as stream:
  stream.seek(entry['offset']);raw=stream.read(entry['size'])
 assert struct.unpack_from('<II',raw)==(0xd4bada55,180)
 ordered=sorted(level['sections'],key=lambda s:s['offset']);converted=[]
 for s in ordered:
  kind=int(s['type'],16);assert struct.unpack_from('<II',raw,s['offset'])==(kind,s['size'])
  converted.append(dict(type=kind,size=s['size'],offset=s['offset']))
 start=next(i for i,s in enumerate(converted) if s['type']==0x100)+1
 sections=[s for s in converted[start:] if s['type']]
 for multi,server,version in [(0,0,180),(1,0,180),(1,1,180),(2,0,180),(0,0,70)]:
  cursor=0;trace=[];current=None;u.mem_write(stack,bytes(1024));u.mem_write(0x64ecb9,bytes((multi,server)));u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x460d3b,0x461137,count=20000);assert u.reg_read(UC_X86_REG_EIP)==0x461137 and cursor==len(sections)
  expected=[]
  for s in sections:
   kind=s['type']
   if kind==0x2000:expected += [[kind,a] for a in geometry];continue
   target=routes.get(kind,0x524400)
   if kind in (0x30000,0x40000) and multi==1 and server==0:target=0x524400
   if kind==0x20000 and version<71:target=0x524400
   expected.append([kind,target])
  assert trace==expected,(level['file'],multi,server,version,trace,expected)
  if multi==0 and version==180:reports.append(dict(archive=level['archive'],level=level['file'],post_geometry=[dict(section=hex(t),handler=hex(a)) for t,a in trace]))
  cases+=1
report=dict(result='PASS',cases=cases,levels=len(levels),original_sha256=sha,scope='Original460d3b..461137 post-world-geometry dispatch loop. Actual installed section headers validated against archives. Header reads, seek, resource loading and section handlers are supplied boundaries; exact handler sequence/order and key arguments checked. SP, MP client/server, noncanonical multiplayer byte and old-version skip exercised. Does not execute section records, constructors, pre-geometry phase or post-load player creation.',dispatch=reports)
(root/'artifacts/level-factory-order.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='dispatch'})
print(next(x for x in reports if x['level'].lower()=='l1s1.rfl'))
