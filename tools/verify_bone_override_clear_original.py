"""Audit original bone-override clearing and generation-cache behavior; no hooks."""
import hashlib,json,struct,subprocess,sys
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

def evaluate(generation):
    u.mem_write(obj+0x1cf8,struct.pack('<H',generation))
    u.mem_write(stack+64000,struct.pack('<4I',stop,count,order_address,obj))
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x51b500,stop,count=1000000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    return bytes(u.mem_read(obj,count*48))

def prepare(tick):
    u.mem_write(obj,bytes(0x2000));u.mem_write(obj+0x1d50,struct.pack('<I',desc))
    u.mem_write(obj+0x12d0,struct.pack('<3If',1,0,tick,1));u.mem_write(obj+0x1d00,struct.pack('<i',-1))

cases=changed=0
for name in ('ult2_stand.rfa','ult2_crouch.rfa'):
    raw=read(name);u.mem_write(data,raw);first,last=struct.unpack_from('<2i',raw,16)
    for tick in (first+1,(first+last)//2):
        prepare(tick);baseline=evaluate(1)
        for bone in (0,1,8,count-1):
            for weight in (0,.25,.5,1):
                reference=None
                for enabled in (1,2,127,128,255):
                    prepare(tick)
                    # Original per-bone override: 3x3 basis, enabled byte, weight.
                    address=obj+0x1398+48*bone
                    u.mem_write(address,struct.pack('<9f',0,1,0,-1,0,0,0,0,1))
                    u.mem_write(address+36,bytes([enabled]))
                    u.mem_write(address+40,struct.pack('<f',weight))
                    payload=bytes(u.mem_read(address,44))
                    overridden=evaluate(1)
                    if reference is None:reference=overridden
                    assert overridden==reference,(name,tick,bone,weight,'nonzero flag semantics')
                    # Clearing the exact death-stage byte does not invalidate cached matrices.
                    u.mem_write(address+36,b'\0')
                    assert evaluate(1)==overridden,(name,tick,bone,weight,'same generation')
                    assert evaluate(2)==baseline,(name,tick,bone,weight,'next generation')
                    remaining=bytes(u.mem_read(address,44))
                    assert remaining==payload[:36]+b'\0'+payload[37:], 'override payload changed'
                    changed+=overridden!=baseline;cases+=1
assert changed>0
report=dict(result='PASS',cases=cases,changed_poses=changed,bones=count,
 scope='Unhooked original 0x51b500: miner stand/crouch, four bones, four weights, all tested nonzero flag bytes; clear preserves current-generation cache and next generation restores baseline. No shared override implementation or Xbox execution.')
(root/'artifacts/bone-override-clear-original.json').write_text(json.dumps(report,indent=2));print(report)
