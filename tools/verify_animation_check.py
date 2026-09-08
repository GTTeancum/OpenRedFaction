"""Verify the exact 64-frame PC/Xbox diagnostic sequence against original x86."""
import hashlib,json,struct,subprocess,sys,random,os
from pathlib import Path
import pefile
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,desc,motion,data,order_address,stack,stop=[0x30000000+i*0x100000 for i in range(7)]
for a in (obj,desc,motion,data,order_address,stack,stop): u.mem_map(a,65536)
entries={e['name'].lower():(a['path'],e) for a in json.loads((root/'artifacts/inventory.json').read_text())['files'] for e in a.get('vpp',{}).get('entries',[])}
def read(name):
    archive,e=entries[name]
    with (root/'Installed_Game'/archive).open('rb') as f: f.seek(e['offset']); return f.read(e['size'])
raw=read('miner.v3c'); section=next(s for s in inspect(raw)['sections'] if s['type']=='0x424f4e45'); start=section['offset']+8
count,=struct.unpack_from('<I',raw,start)
lod=next(s for s in inspect(raw)['sections'] if s['type']=='0x5355424d')['lods'][0]
attachments=raw[lod['attachment_offset']:lod['attachment_offset']+lod['props']*100]
eye_index=next(i for i in range(lod['props']) if attachments[i*100:i*100+68].split(b'\0')[0]==b'eye')
a,b,c,tags=[order_address+i*4096 for i in (1,2,3,4)]
u.mem_write(desc+0x1a50,struct.pack('<I',a)); u.mem_write(a+0x8c,struct.pack('<I',b)); u.mem_write(b+4,struct.pack('<I',c))
u.mem_write(c+0x10,struct.pack('<2I',tags,lod['props'])); u.mem_write(tags,attachments)
parents=[struct.unpack_from('<i',raw,start+4+i*56+52)[0] for i in range(count)]
def depth(i): return 0 if parents[i]<0 else 1+depth(parents[i])
order=bytes(sorted(range(count),key=depth)); u.mem_write(order_address,order)
u.mem_write(desc+0x48,struct.pack('<I',count)); u.mem_write(desc+0xf58,struct.pack('<II',1,motion))
for i,parent in enumerate(parents): u.mem_write(desc+0x94+i*0x4c,struct.pack('<i',parent))
u.mem_write(motion+0x78,struct.pack('<I',data))
names=('ult2_stand.rfa','ult2_crouch.rfa','ult2_sidestep_left.rfa','ult2_sidestep_right.rfa')
for i,name in enumerate(names):
    assert len(read(name))<=16384
    u.mem_write(data+i*16384,read(name))
    u.mem_write(desc+0xf5c+i*4,struct.pack('<I',motion+i*256))
    u.mem_write(motion+i*256+0x78,struct.pack('<I',data+i*16384))
u.mem_write(desc+0xf58,struct.pack('<I',len(names)))
def rd(a,n): return bytes(u.mem_read(a,n))
def put(a,fmt,*v): u.mem_write(a,struct.pack(fmt,*v))
def fnv(value,raw):
    for byte in raw: value=((value^byte)*16777619)&0xffffffff
    return value
put(obj+0x1d50,'<I',desc);put(obj+0x12d0,'<I',0)
put(obj+0x1cfc,'<ii',-1,-1);put(obj+0x1d48,'<i',-1);put(obj+0x1d04,'<f',.25);put(obj+0x1cf8,'<H',1)
put(obj+0x12c0,'<3f',.125,-.25,.5)
for i in range(4):
    u.mem_write(desc+0x120c+i,bytes([int(i<2)]));put(motion+i*256+0x74,'<I',0)
    put(motion+i*256+0x50,'<i',3200);put(motion+i*256+0x64,'<i',6400)
entity=obj+0x6000;wrapper=obj+0x4000
put(wrapper,'<II',2,obj);put(entity+0x80,'<I',wrapper)
put(entity+0x138c,'<iiff',0,-1,0,0);put(entity+0x1384,'<i',0)
for i in range(23):put(entity+0x8e4+i*16,'<i',0 if i==0 else 1 if i==8 else -1)
put(0x5a4014,'<f',1/30)
info=obj+0x9000; mode=obj+0xa000; candidates=stack+65200
put(entity+0x294,'<I',info);put(info+0x724,'<I',0x800)
put(info+0x50,'<4f',6,.3,1.5,20);put(entity+0x75c,'<i',-1);put(entity+0x98,'<f',1)
put(0x594590,'<f',7);put(0x59458c,'<f',9);put(0x64ecb9,'<B',0)
put(entity+0x7d0,'<I',10);put(entity+0x48,'<9f',1,0,0,0,1,0,0,0,1)
put(entity+0x858,'<I',mode);put(entity+0x2a0,'<Iii',entity,0,-1);put(0x872114,'<i',0);put(0x6fc4d8,'<B',0)
put(entity+0x6cc,'<i',-1);put(0x872448,'<I',1)
put(entity+0x2c,'<I',0x10000);put(0x7394cc,'<I',entity);put(info+0x94,'<I',2)
put(entity+0x81c,'<ii',-1,-1);put(entity+0x13d4,'<i',0)
put(0x85cd08+0x264,'<II',6,0x40);put(0x85cd08+0x204,'<i',-1);put(0x64ecbb,'<B',0)
effect_objects=[order_address+24000,order_address+25000]
put(0x75ec48,'<II',*effect_objects)
for a,t in zip(effect_objects,[77,99]):put(a+0x140,'<I',1);put(a+0x154,'<i',t)
player=obj+0xd000;put(player+0x14,'<I',0x10000);put(player+0xf41,'<B',1)
put(player+0x1154,'<32i',1,0,*([-1]*30));put(entity+0x42c,'<2B',1,1)
put(0x85cd08+0x24,'<i',0);put(0x85cd08+0x260,'<i',10)
put(0x85cd08+1360+0x24,'<i',-1);put(0x85cd08+1360+0x260,'<i',0);put(0x85cd08+1360+0x268,'<I',0x100)
put(entity+0x200,'<i',-1)
for i in range(45):put(entity+0xa54+i*16,'<iii',2 if i==17 else 3 if i==18 else -1,0,-1)
starts=[];reset_calls=[]
preparing=[False]
# Candidate entry is also the preparation boundary. An observation hook is
# needed when that entry already has a cached Unicorn translation block.
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop() if preparing[0] else None,begin=0x41f61d,end=0x41f61d)
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:starts.append(struct.unpack('<i',rd(uc.reg_read(UC_X86_REG_ESP)+8,4))[0]),begin=0x428c90,end=0x428c90)
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:reset_calls.append(a),begin=0x41ae70,end=0x41ae70)
def evaluate():
    put(stack+64000,'<4I',stop,count,order_address,obj);u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x51b500,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
hashes=[2166136261]*4
effect_candidates=bytes(8)
for frame in range(64):
    put(entity+0x2ac,'<i',int(frame<32))
    put(stack+64000,'<3I',stop,entity,0);u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x42add0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    reserve=u.reg_read(UC_X86_REG_EAX)
    put(stack+64000,'<2I',stop,player);u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x4a6e50,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    replacement=u.reg_read(UC_X86_REG_EAX)
    assert replacement==(0 if frame<32 else 1)
    u.reg_write(UC_X86_REG_FPCW,0x37f)
    if frame in (4,7,20,40):
        put(stack+64000,'<IIif',stop,entity,8 if frame in (4,20) else 0,.25)
        u.reg_write(UC_X86_REG_ESP,stack+64000)
        u.emu_start(0x42a580,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    override=int(22<=frame<26);put(entity+0x810,'<I',override*32)
    put(stack+64000,'<6I',entity,0,0,0,stop,entity)
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ESI,entity)
    u.emu_start(0x41f2b6,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    put(entity+0x588,'<i',int(8<=frame<56));put(entity+0x7a0,'<3f',-1 if frame<32 else 1,0,0)
    put(0x5a3ed8,'<i',frame*33)
    put(entity+0x520,'<i',12 if frame>=62 else 7 if frame>=60 else 17 if frame>=58 else 0)
    put(entity+0x554,'<i',int(frame>=48));put(entity+0x144,'<3f',int(frame>=32),0,0);put(entity+0x810,'<I',0)
    if frame==48:put(entity+0x46c,'<B',1)
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ESI,entity)
    preparing[0]=True
    u.emu_start(0x41f5ae,0x41f61d,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41f61d
    preparing[0]=False
    predicate_results=[]
    for address in (0x41f950,0x427020,0x428e60):
        put(stack+64000,'<II',stop,entity);u.reg_write(UC_X86_REG_ESP,stack+64000)
        u.emu_start(address,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
        predicate_results.append(u.reg_read(UC_X86_REG_EAX)&255)
    ready,dead,excluded=predicate_results;eligible=int(ready==1 and not dead and not excluded)
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ESI,entity);u.reg_write(UC_X86_REG_EBX,1)
    u.emu_start(0x41f61d,0x41f729,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x41f729
    selected=struct.pack('<I',u.reg_read(UC_X86_REG_EBX))+rd(stack+64012,4)+rd(stack+64032,4)+struct.pack('<I',u.reg_read(UC_X86_REG_EDI))
    if frame<58:effect_candidates=rd(stack+64012,4)+rd(stack+64032,4)
    put(stack+64000,'<If',stop,1/30);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ECX,obj);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x51ba80,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    evaluate()
    state=rd(obj+0x12d0,196)+rd(obj+0x1cfc,8)+rd(obj+0x1d48,4)+struct.pack('<II',rd(obj+0x1d4c,1)[0],rd(obj+0x1d14,1)[0])+rd(obj+0x1d18,32)+rd(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',rd(obj+0x1cf8,2))[0],rd(obj+0x1d44,1)[0]|rd(obj+0x1d45,1)[0]<<1)
    hashes[0]=fnv(hashes[0],rd(obj,count*48));hashes[1]=fnv(hashes[1],state)
    hashes[1]=fnv(hashes[1],rd(entity+0x138c,16)+rd(entity+0x1384,4)+struct.pack('<I',override))
    for i in range(4):hashes[1]=fnv(hashes[1],rd(motion+i*256+0x74,4))
    effects=rd(entity+0x8c,4)+rd(entity+0x8c0,8)+b''.join(rd(entity+off,4) for off in (0x79c,0x4d0,0x4d4,0x744,0x798))+rd(entity+0x7bc,4)+effect_candidates
    hashes[1]=fnv(hashes[1],effects+struct.pack('<i',-1)+selected+struct.pack('<ii',ready,eligible))
    hashes[1]=fnv(hashes[1],rd(entity+0x46c,64)+rd(entity+0x7d0,4)+rd(entity+0x810,4)+rd(entity+0x81c,8)+rd(entity+0x13d4,4)+struct.pack('<II',1,0))
    for a in effect_objects:hashes[1]=fnv(hashes[1],rd(a+0x140,4)+rd(a+0x154,4))
    hashes[1]=fnv(hashes[1],struct.pack('<II',reserve,replacement))
    put(stack+64000,'<3I',stop,obj+0x3000,count+eye_index);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ECX,obj)
    u.emu_start(0x51b2e0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    hashes[3]=fnv(hashes[3],rd(obj+0x3000,48))
    put(obj+0x12c0,'<f',1);evaluate();assert rd(obj+0x12c0,4)==struct.pack('<f',1)
    hashes[2]=fnv(hashes[2],rd(obj,count*48)+rd(obj+0x12c0,12)+b''.join(rd(obj+0x1394+i*48,2) for i in range(count)))
    put(obj+0x12c0,'<f',0)
expected=[2,count,64,*hashes,4+count*56]
actual=list(struct.unpack('<8I',subprocess.check_output([str(root/'build/pc/Release/rf_animation_check.exe'),str(root/'Installed_Game/meshes.vpp'),str(root/'Installed_Game/motions.vpp')])))
assert len(reset_calls)==16 and 17 in starts and 18 in starts and len(starts)<48,(starts,reset_calls)
report=dict(result='PASS' if actual==expected else 'FAIL',expected=expected,actual=actual,action_starts=starts,reset_calls=len(reset_calls),scope='64-frame ammo/replacement decisions, entity predicates, scripted controller, preparation/candidate blocks, valid active-weapon reset with nonloop/effect stops, playback, skeleton/cache and eye against unmodified original instructions; actual switching, sound and complete empty-weapon handling excluded')
(root/'artifacts/animation-check-original.json').write_text(json.dumps(report,indent=2));print(report);assert actual==expected
