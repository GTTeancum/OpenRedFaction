"""Verify the standing/crouching pose-query sequence through character wrappers."""
import hashlib,json,struct,subprocess,sys,os
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
names=('ult2_stand.rfa','ult2_crouch.rfa')
for i,name in enumerate(names):
    u.mem_write(data+i*16384,read(name))
    u.mem_write(desc+0xf5c+i*4,struct.pack('<I',motion+i*256))
    u.mem_write(motion+i*256+0x78,struct.pack('<I',data+i*16384))
u.mem_write(desc+0xf58,struct.pack('<I',len(names)))
def rd(a,n): return bytes(u.mem_read(a,n))
def put(a,fmt,*v): u.mem_write(a,struct.pack(fmt,*v))
cases=[]
for phase in [0,.1,.5,.9]:
    for elapsed in [0,1/30]:
        state=struct.pack('<I',0)+bytes(192)+struct.pack('<3i',-1,-1,-1)+bytes(40)+struct.pack('<fII',phase,1,0)
        assert len(state)==260
        cases.append(state+struct.pack('<If3fi',3,elapsed,0,0,0,-1))
env=dict(os.environ);env.pop('RF_PROBE_CACHE',None);env['RF_PROBE_EYE_SETUP']='1'
run=subprocess.run([str(root/'build/pc/Release/rf_skeleton_probe.exe'),str(root/'Installed_Game/meshes.vpp'),str(root/'Installed_Game/motions.vpp'),'miner.v3c',*names],input=b''.join(cases),capture_output=True,check=True,env=env)
stride=272+(count+1)*48;assert len(run.stdout)==len(cases)*3*stride
wrapper=obj+0x4000;results=[];failures=[]
def call(address,fmt='',*args):
    put(stack+64000,'<I'+fmt,stop,*args);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(address,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
for k,raw in enumerate(cases):
    u.mem_write(obj,bytes(65536));put(wrapper,'<II',2,obj);put(obj+0x1d50,'<I',desc)
    put(obj+0x1cfc,'<ii',-1,-1);put(obj+0x1d48,'<i',-1);u.mem_write(obj+0x1d04,raw[248:252]);put(obj+0x1cf8,'<H',1)
    for i in range(2): u.mem_write(desc+0x120c+i,b'\x01');put(motion+i*256+0x74,'<I',0)
    elapsed=struct.unpack_from('<f',raw,264)[0]
    call(0x503390,'Iif',wrapper,0,1)
    if elapsed: call(0x503360,'If4I',wrapper,elapsed,0,0,0,1)
    call(0x503390,'Iif',wrapper,0,1)
    for query in range(3):
        if query:
            call(0x5033f0,'I',wrapper);call(0x503390,'Iif',wrapper,1 if query==1 else 0,1);call(0x503360,'If4I',wrapper,.2,0,0,0,1)
        call(0x51b500,'3I',count,order_address,obj)
        state=rd(obj+0x12d0,196)+rd(obj+0x1cfc,8)+rd(obj+0x1d48,4)+struct.pack('<II',rd(obj+0x1d4c,1)[0],rd(obj+0x1d14,1)[0])+rd(obj+0x1d18,32)+rd(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',rd(obj+0x1cf8,2))[0],rd(obj+0x1d44,1)[0]|rd(obj+0x1d45,1)[0]<<1)
        expected=state+rd(obj+0x12c0,12)+rd(obj,count*48)
        u.reg_write(UC_X86_REG_ECX,obj);call(0x51b2e0,'2I',obj+0x3000,count+eye_index);expected+=rd(obj+0x3000,48)
        actual=run.stdout[(k*3+query)*stride:(k*3+query+1)*stride]
        if actual!=expected: failures.append(dict(case=k,query=query,fields=[i for i in range(0,stride,4) if actual[i:i+4]!=expected[i:i+4]]))
        results.append(dict(case=k,query=['standing','crouching','restored-standing-query'][query],initial_phase=struct.unpack_from('<f',raw,248)[0],initial_elapsed=elapsed,phase=struct.unpack_from('<f',state,248)[0],tick=struct.unpack_from('<i',state,8)[0],eye=list(struct.unpack('<3f',expected[-12:]))))
report=dict(result='PASS' if not failures else 'FAIL',sequences=len(cases),bone_matrices=len(cases)*3*count,eye_transforms=len(results),failures=failures,results=results,scope='Loaded miner stand/crouch sequence through character control wrappers; initial controller selection and entity flags not emulated')
(root/'artifacts/eye-setup-verification.json').write_text(json.dumps(report,indent=2));print(report);assert not failures
