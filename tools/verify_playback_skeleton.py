"""Verify playback update followed by archive-based blended skeleton/eye evaluation."""
import hashlib,json,struct,subprocess,sys,random
from pathlib import Path
import pefile
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_FPCW
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
names=('ult2_stand.rfa','ult2_crouch.rfa','ult2_stand.rfa')
for i,name in enumerate(names):
    u.mem_write(data+i*16384,read(name))
    u.mem_write(desc+0xf5c+i*4,struct.pack('<I',motion+i*256))
    u.mem_write(motion+i*256+0x78,struct.pack('<I',data+i*16384))
u.mem_write(desc+0xf58,struct.pack('<I',len(names)))
rng=random.Random(0x51b500); cases=[]
for k in range(160):
    n=k%4; ids=rng.sample(range(3),n)
    slots=b''.join(struct.pack('<iif',ids[i] if i<n else 0,rng.randrange(161,9602),rng.choice([0,.25,.5,1])) for i in range(16))
    primary=rng.choice([-1]+list(range(n)))
    state=struct.pack('<I',n)+slots+struct.pack('<3i',rng.choice([-1]+list(range(n))),primary,-1)+struct.pack('<4I6f',0,1,123,456,1,2,3,4,5,6)+struct.pack('<fII',rng.uniform(.1,.9),1,0)
    cases.append(state+struct.pack('<If',rng.randrange(8),rng.choice([0,1/60,.2,1,2])))
run=subprocess.run([str(root/'build/pc/Release/rf_skeleton_probe.exe'),str(root/'Installed_Game'/entries['miner.v3c'][0]),str(root/'Installed_Game'/entries[names[0]][0]),'miner.v3c',*names],input=b''.join(cases),capture_output=True,check=True)
stride=260+(count+1)*48;assert len(run.stdout)==len(cases)*stride
failures=[]
for k,raw in enumerate(cases):
    u.mem_write(obj,bytes(0x4000));u.mem_write(obj+0x1d50,struct.pack('<I',desc))
    u.mem_write(obj+0x12d0,raw[:196]);u.mem_write(obj+0x1cfc,raw[196:204]);u.mem_write(obj+0x1d48,raw[204:208])
    u.mem_write(obj+0x1d4c,raw[208:209]);u.mem_write(obj+0x1d14,raw[212:213]);u.mem_write(obj+0x1d18,raw[216:248])
    u.mem_write(obj+0x1d04,raw[248:252]);u.mem_write(obj+0x1cf8,raw[252:254])
    flags=struct.unpack_from('<I',raw,260)[0]
    for i in range(3):
        u.mem_write(desc+0x120c+i,bytes([(flags>>i)&1]));u.mem_write(motion+i*256+0x74,struct.pack('<I',2))
    u.mem_write(stack+64000,struct.pack('<I',stop)+raw[264:268])
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ECX,obj);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x51ba80,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    got=run.stdout[k*stride:(k+1)*stride]
    rd=lambda a,n:bytes(u.mem_read(a,n))
    expected=rd(obj+0x12d0,196)+rd(obj+0x1cfc,8)+rd(obj+0x1d48,4)+struct.pack('<II',rd(obj+0x1d4c,1)[0],rd(obj+0x1d14,1)[0])+rd(obj+0x1d18,32)+rd(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',rd(obj+0x1cf8,2))[0],rd(obj+0x1d44,1)[0]|rd(obj+0x1d45,1)[0]<<1)
    assert got[:260]==expected,('playback',k)
    u.mem_write(stack+64000,struct.pack('<4I',stop,count,order_address,obj));u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x51b500,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
    want=rd(obj,count*48)
    for i in range(count):
        actual=got[260+i*48:260+(i+1)*48];expected=want[i*48:(i+1)*48]
        if actual!=expected: failures.append(dict(case=k,bone=i,actual=actual.hex(),expected=expected.hex()))
    u.mem_write(stack+64000,struct.pack('<3I',stop,obj+0x3000,count+eye_index));u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ECX,obj)
    u.emu_start(0x51b2e0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    if got[-48:]!=rd(obj+0x3000,48): failures.append(dict(case=k,attachment='eye',actual=got[-48:].hex(),expected=rd(obj+0x3000,48).hex()))
report=dict(result='PASS' if not failures else 'FAIL',cases=len(cases),bone_matrices=len(cases)*count,eye_transforms=len(cases),failures=len(failures),examples=failures[:8],scope='Update then evaluate miner with 0..3 active slots, stand/crouch motions, mixed loops and primary attenuation; no root displacement/overrides')
(root/'artifacts/playback-skeleton-verification.json').write_text(json.dumps(report,indent=2)); print(report)
assert not failures
