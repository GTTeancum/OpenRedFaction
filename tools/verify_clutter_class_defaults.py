"""Original40f4f0 through40f9b7: factory-facing class defaults and writes.

Parser tokens/numbers, string storage and material/flag resolution are supplied.
This is explicitly a class-parser prefix, not full40f4f0 parsing equivalence.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);u.mem_map(0,4096)
B=0x30000000;u.mem_map(B,0x40000);P=B+0x1000;STACK=B+0xe000;STUB=B+0xf000;FLOAT=B+0xf100
w=lambda *a:struct.pack('<%dI'%len(a),*(v&0xffffffff for v in a))
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def text(a):
    out=bytearray()
    while u.mem_read(a,1)!=b'\0':out+=u.mem_read(a,1);a+=1
    return bytes(out)
def string(a,value):
    global pool
    u.mem_write(pool,value+b'\0');u.mem_write(a,w(len(value),pool));pool+=len(value)+1
u.mem_write(STUB,b'\xd9\x05'+w(FLOAT)+b'\xc3')
boundaries=(0x4ff3b0,0x4ff470,0x4ffa80,0x511fc0,0x5125c0,0x5126a0,
            0x512bb0,0x512920,0x512750,0x513020,0x4686c0)
def hook(cpu,address,length,context):
    global current
    if address not in boundaries:return
    sp=cpu.reg_read(UC_X86_REG_ESP);owner=cpu.reg_read(UC_X86_REG_ECX);arg=lambda i:r(sp+4+4*i)
    result=pop=0
    if address==0x4ff3b0:string(owner,b'');result=owner
    elif address==0x4ff470:pass
    elif address==0x4ffa80:string(owner,text(arg(0)));result=owner;pop=4
    elif address==0x511fc0:
        assert owner==P;result=1;pop=8
    elif address in (0x5125c0,0x5126a0):
        assert owner==P;current=text(arg(0)).decode();requests.append(current)
        if address==0x5126a0:assert current in fields,current
        result=int(current in fields);pop=4
    elif address==0x512bb0:
        assert owner==P and arg(1)==arg(2)==34
        string(arg(0),fields[current]);pop=12
    elif address==0x512920:
        assert owner==P;u.mem_write(FLOAT,struct.pack('<f',fields[current]));cpu.reg_write(UC_X86_REG_EIP,STUB);return
    elif address==0x512750:assert owner==P;result=fields[current]
    elif address==0x513020:assert owner==P and arg(0)==0x593f20 and arg(1)==9;result=fields[current];pop=8
    elif address==0x4686c0:assert text(r(arg(0)+4))==b'metal';result=2
    cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,hook)
u.mem_write(0x1754474,b'\x07')
rng=random.Random(0x40f4f0);rows=[]
optional=['$Emitter Life:','$Radius:','$Explode Anim Radius:','$Explode Damage:','$Screen Width:','$Screen Height:']
for case in range(256):
    pool=B+0x20000;current='';requests=[]
    model=rng.choice((b'fixture.v3d',b'fixture.vcm',b'fixture.vfx',b'fixture.VFX',b'fixture'))
    fields={'$Class Name:':b'fixture','$V3D Filename:':model,'$Material:':b'metal','$Life:':rng.choice((-1.,0.,1.,100.)),'$Flags:':rng.randrange(512)}
    for i,key in enumerate(optional):
        if case&(1<<i):fields[key]=rng.choice((0,1,64,512)) if i>=4 else rng.choice((-1.,0.,.125,2.,100.))
    cls=0x5afb88;u.mem_write(cls,b'\xa5'*232)
    # Owner preconditions from the table reset: string/array members are empty.
    for offset in (0,8,16,24,32):string(cls+offset,b'')
    u.mem_write(cls+0x28,w(0,0,0));u.mem_write(0x5c97dc,w(0))
    u.mem_write(STACK,w(0xdeadbeef,P));u.reg_write(UC_X86_REG_ESP,STACK)
    try:u.emu_start(0x40f4f0,0x40f9b7,count=1000000)
    except Exception:print('stopped',case,hex(u.reg_read(UC_X86_REG_EIP)),current,requests);raise
    assert u.reg_read(UC_X86_REG_EIP)==0x40f9b7
    defaults={0x34:fields.get('$Emitter Life:',-1.),0x3c:fields['$Life:'],0x40:fields.get('$Radius:',-1.),
              0x5c:fields.get('$Explode Anim Radius:',1.),0x60:fields.get('$Explode Damage:',1.)}
    for offset,value in defaults.items():assert bytes(u.mem_read(cls+offset,4))==struct.pack('<f',value),(case,hex(offset),value)
    assert r(cls+0x38)==(3 if model.lower().endswith(b'.vfx') else 1)
    assert bytes(u.mem_read(cls+0x4c,4))==b'\x02\xa5\xa5\xa5'
    for offset in (0x50,0x54,0x58,0x7c,0x80):assert r(cls+offset)==0xffffffff,(case,hex(offset))
    assert bytes(u.mem_read(cls+0x64,12))==bytes(12)
    assert bytes(u.mem_read(cls+0x9c,44))==struct.pack('<11f',*([1.]*11))
    assert r(cls+0x74)==fields['$Flags:']
    assert r(cls+0xe0)==fields.get('$Screen Width:',64) and r(cls+0xe4)==fields.get('$Screen Height:',64)
    assert text(r(cls+4))==b'fixture' and text(r(cls+12))==model
    rows.append(dict(case=case,model=model.decode(),values={k:v for k,v in fields.items() if not isinstance(v,bytes)}))
result=dict(result='PASS',cases=len(rows),entry='0040f4f0',stop='0040f9b7',original_sha256=digest,
 scope='Original class-parser prefix with supplied tag presence, decoded numbers/strings, flags and material lookup. Actual default/write branches, extension extraction and case comparison execute. No full parser, suffix/light/skin parsing, resource resolution, shared C class loader or native XEMU claim.',rows=rows)
(ROOT/'artifacts/clutter-class-defaults.json').write_text(json.dumps(result,indent=2));print({k:v for k,v in result.items() if k!='rows'})
