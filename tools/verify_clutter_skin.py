"""Original410d30 skin selection plus actual4153e0/48ac00 effects.

Only503650 model-material lookup and50f6a0 texture loading are supplied.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,0x20000);stack=b+0xe000;stop=b+0xf000
actor,cls,skins,ids,materials=b+0x1000,b+0x2000,b+0x3000,b+0x4000,b+0x5000
children=[b+0x8000+i*0x400 for i in range(6)]
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
pool=b+0x10000;names={};trace=[];material_count=0
def string(address,value):
    global pool
    u.mem_write(pool,value+b'\0');u.mem_write(address,w(len(value),pool));names[pool]=value;pool+=len(value)+1
def hook(cpu,address,size,context):
    if address not in (0x503650,0x50f6a0):return
    sp=cpu.reg_read(UC_X86_REG_ESP);result=0
    if address==0x503650:
        model,count,out=struct.unpack('<3I',cpu.mem_read(sp+4,12));assert model==0x12345678
        trace.append(('materials',));cpu.mem_write(count,w(material_count));cpu.mem_write(out,w(materials))
    else:
        name,first,second=struct.unpack('<3I',cpu.mem_read(sp+4,12));assert (first,second)==(0xffffffff,1)
        value=names[name];trace.append(('texture',value))
        result=0xffffffff if value==b'missing' else 0x900000+sum(value)
    cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x410d30);selected_count=misses=texture_calls=glare_updates=0
for case in range(2048):
    count=case%7;material_count=rng.randrange(7);pool=b+0x10000;names={};trace=[]
    choices=[b'Yellowish',b'yellowish',b'BLUE',b'',b'duplicate'];requested=rng.choice(choices+[b'absent',b'blue'])
    variants=[]
    u.mem_write(cls,b'\xa5'*0xe8);u.mem_write(cls+0xc8,w(count,count,skins));u.mem_write(cls+0xd4,w(count,count,ids))
    for i in range(count):
        name=rng.choice(choices);textures=[rng.choice((b'a.tga',b'B.vbm',b'missing',b'')) for _ in range(rng.randrange(7))]
        glare=rng.choice((-2,-1,0,2,4,5))
        variants.append((name,textures,glare));u.mem_write(skins+i*108,b'\xa5'*108)
        string(skins+i*108,name);u.mem_write(skins+i*108+8,w(len(textures)));u.mem_write(ids+i*4,w(glare))
        for j,texture in enumerate(textures):string(skins+i*108+12+j*8,texture)
    requested_ptr=pool;u.mem_write(requested_ptr,requested+b'\0')
    actor_before=bytearray(rng.randbytes(0x2d8));actor_before[0x2c:0x30]=w(77)
    actor_before[0x80:0x84]=w(0x12345678);actor_before[0x294:0x298]=w(cls);u.mem_write(actor,bytes(actor_before))
    expected_materials=bytearray(rng.randbytes(6*200));u.mem_write(materials,bytes(expected_materials))
    expected_children=[]
    u.mem_write(0x5cab98,w(5));u.mem_write(0x5c9e60,w(children[0]))
    for i,child in enumerate(children):
        raw=bytearray(rng.randbytes(0x2bc));raw[0x30:0x34]=w(77 if i%2==0 else 88)
        raw[0x2b8:0x2bc]=w(children[i+1] if i+1<len(children) else 0x5c9ba8)
        u.mem_write(child,bytes(raw));expected_children.append(raw)
    selected=next((i for i,v in enumerate(variants) if v[0].lower()==requested.lower()),-1)
    expected=[]
    if selected>=0:
        selected_count+=1;name,textures,glare=variants[selected]
        if 0<=glare<5:
            for i in range(0,6,2):
                expected_children[i][0x2ac:0x2b4]=w(0x5c9e98+glare*0x34,glare);glare_updates+=1
        expected.append(('materials',))
        for i,texture in enumerate(textures[:material_count]):
            expected.append(('texture',texture));texture_calls+=1
            token=0xffffffff if texture==b'missing' else 0x900000+sum(texture)
            expected_materials[i*200+16:i*200+20]=w(token)
    else:misses+=1
    u.mem_write(stack,w(stop,actor,requested_ptr));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x410d30,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
    assert u.reg_read(UC_X86_REG_EAX)==selected&0xffffffff,(case,'selection',requested,variants)
    assert trace==expected,(case,'calls',trace,expected)
    assert bytes(u.mem_read(materials,1200))==expected_materials,(case,'materials')
    assert bytes(u.mem_read(actor,0x2d8))==actor_before,(case,'actor')
    for child,raw in zip(children,expected_children):assert bytes(u.mem_read(child,len(raw)))==raw,(case,'glare')
report=dict(result='PASS',cases=2048,selected=selected_count,missing=misses,texture_calls=texture_calls,
            glare_updates=glare_updates,original_sha256=digest,
            scope='Full410d30 with actual ASCII case-insensitive string equality,4153e0 glare retarget and48ac00 ordered material replacement. '
            'Supplied model-material lookup and texture handles. Exact actor/glare/material bytes and call sequence. '
            'No shared C skin binding, full class parser, actual texture residency or native rendering proof.')
(root/'artifacts/clutter-skin.json').write_text(json.dumps(report,indent=2));print(report)
