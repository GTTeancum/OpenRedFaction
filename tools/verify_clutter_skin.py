"""Original410d30 skin selection plus actual4153e0/48ac00 effects.

Only503650 model-material lookup and50f6a0 texture loading are supplied.
"""
import hashlib,json,random,re,struct,subprocess,sys
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
shared='--shared' in sys.argv;commands=[];answers=[];failure=0;failure_texture_number=0
if shared:
    xp=pefile.PE(str(root/'build/xbox/main.exe'));xi=xp.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
    x.mem_map(xp.OPTIONAL_HEADER.ImageBase,(len(xi)+4095)//4096*4096);x.mem_write(xp.OPTIONAL_HEADER.ImageBase,xi);x.mem_map(b,0x20000)
    entry=int(re.search(r'\s_rf_clutter_skin_apply\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def string(address,value):
    global pool
    u.mem_write(pool,value+b'\0');u.mem_write(address,w(len(value),pool));names[pool]=value;pool+=len(value)+1
def hook(cpu,address,size,context):
    if address not in (0x503650,0x50f6a0,b+0xf100,b+0xf200):return
    sp=cpu.reg_read(UC_X86_REG_ESP);result=0
    shared_call=address>=b
    if address in (0x503650,b+0xf100):
        if shared_call:
            context,model,out,count=struct.unpack('<4I',cpu.mem_read(sp+4,16));assert context==0
        else:model,count,out=struct.unpack('<3I',cpu.mem_read(sp+4,12))
        assert model==0x12345678
        trace.append(('materials',));cpu.mem_write(count,w(material_count));cpu.mem_write(out,w(materials))
    else:
        if shared_call:
            context,name,first,second,out=struct.unpack('<5I',cpu.mem_read(sp+4,20));assert context==0
        else:name,first,second=struct.unpack('<3I',cpu.mem_read(sp+4,12))
        assert (first,second)==(0xffffffff,1)
        value=names[name];trace.append(('texture',value))
        result=0xffffffff if value==b'missing' else 0x900000+sum(value)
        if shared_call:cpu.mem_write(out,w(result));result=0
    if shared_call and failure==address:
        if not failure_texture_number or sum(row[0]=='texture' for row in trace)==failure_texture_number:result=0xffffffff
    cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,struct.unpack('<I',cpu.mem_read(sp,4))[0]);cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook)
if shared:x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x410d30);selected_count=misses=texture_calls=glare_updates=0
for case in range(2048):
    count=case%7;material_count=rng.choice((-2,-1,0,1,2,3,4,5,6));pool=b+0x10000;names={};trace=[]
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
    expected_materials=bytearray(rng.randbytes(6*200));material_before=bytes(expected_materials);u.mem_write(materials,material_before)
    expected_children=[]
    u.mem_write(0x5cab98,w(5));u.mem_write(0x5c9e60,w(children[0]))
    for i,child in enumerate(children):
        raw=bytearray(rng.randbytes(0x2bc));raw[0x30:0x34]=w(77 if i%2==0 else 88)
        raw[0x2b8:0x2bc]=w(children[i+1] if i+1<len(children) else 0x5c9ba8)
        u.mem_write(child,bytes(raw));expected_children.append(raw)
    glare_before=b''.join(raw[0x30:0x34]+raw[0x2b0:0x2b4]+raw[0x2ac:0x2b0] for raw in expected_children)
    selected=next((i for i,v in enumerate(variants) if v[0].lower()==requested.lower()),-1)
    expected=[]
    if selected>=0:
        selected_count+=1;name,textures,glare=variants[selected]
        if 0<=glare<5:
            for i in range(0,6,2):
                expected_children[i][0x2ac:0x2b4]=w(0x5c9e98+glare*0x34,glare);glare_updates+=1
        expected.append(('materials',))
        for i,texture in enumerate(textures[:max(0,material_count)]):
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
    if shared:
        pad=lambda s:s+b'\0'*(16-len(s))
        wire=w(count,material_count)+pad(requested)
        for name,textures,glare in variants:
            wire+=pad(name)+b''.join(pad(t) for t in textures)+bytes((6-len(textures))*16)+w(len(textures),glare)
        wire+=bytes((6-count)*120)+material_before+glare_before
        assert len(wire)==2016;commands.append(wire)
        glare_after=b''.join(raw[0x30:0x34]+raw[0x2b0:0x2b4]+raw[0x2ac:0x2b0] for raw in expected_children)
        output=w(0,selected,len(expected))
        for row in expected:output+=w(1)+bytes(16) if row[0]=='materials' else w(2)+pad(row[1])
        output+=bytes((7-len(expected))*20)+bytes(expected_materials)+glare_after
        assert len(output)==1424;answers.append(output)
        x.mem_write(b+0x10000,bytes(u.mem_read(b+0x10000,pool-b-0x10000+len(requested)+1)))
        for i,(name,textures,glare) in enumerate(variants):
            pointers=[read(skins+i*108+16+j*8) for j in range(len(textures))]
            x.mem_write(b+0x6500+i*32,w(*pointers) or b'\0')
            x.mem_write(b+0x6000+i*16,w(read(skins+i*108+4),b+0x6500+i*32,len(textures),glare))
        x.mem_write(materials,material_before);x.mem_write(b+0x7000,glare_before)
        x.mem_write(b+0x7800,w(*[0x5c9e98+i*52 for i in range(5)]))
        x.mem_write(b+0x7900,w(b+0xf100,b+0xf200,0));x.mem_write(b+0x7980,w(0x12345678))
        args=[b+0x6000,count,requested_ptr,77,0x12345678,b+0x7000,6,b+0x7800,5,b+0x7900,b+0x7980]
        x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);trace=[]
        x.emu_start(entry,stop,count=100000)
        assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
        assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(b+0x7980,4))==w(selected),(case,'NXDK result')
        assert trace==expected and bytes(x.mem_read(materials,1200))==expected_materials,(case,'NXDK materials')
        assert bytes(x.mem_read(b+0x7000,72))==glare_after,(case,'NXDK glare')
if shared:
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--clutter-skin'],input=b''.join(commands))
    assert len(actual)==len(answers)*1424
    for i,want in enumerate(answers):assert actual[i*1424:(i+1)*1424]==want,(i,'PC')
    # Explicit port error behavior: completed glare/texture writes survive.
    material_count=3;names={b+0x10100:b'a.tga',b+0x10120:b'missing'}
    x.mem_write(b+0x10000,b'skin\0');x.mem_write(b+0x10100,b'a.tga\0');x.mem_write(b+0x10120,b'missing\0')
    x.mem_write(b+0x6500,w(b+0x10100,b+0x10120));x.mem_write(b+0x6000,w(b+0x10000,b+0x6500,2,2))
    args=[b+0x6000,1,b+0x10000,77,0x12345678,b+0x7000,6,b+0x7800,5,b+0x7900,b+0x7980]
    changed=bytearray(glare_before)
    for i in range(6):
        if struct.unpack_from('<I',changed,i*12)[0]==77:changed[i*12+4:i*12+12]=w(2,0x5c9e98+104)
    def guarded(arguments,status,wanted_trace,wanted_materials,wanted_glare):
        global trace
        trace=[];x.mem_write(materials,material_before);x.mem_write(b+0x7000,glare_before)
        x.mem_write(b+0x7980,w(0x12345678));x.mem_write(stack,w(stop,*arguments));x.reg_write(UC_X86_REG_ESP,stack)
        x.emu_start(entry,stop,count=100000)
        assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
        assert x.reg_read(UC_X86_REG_EAX)==status&0xffffffff and bytes(x.mem_read(b+0x7980,4))==w(0x12345678)
        assert trace==wanted_trace and bytes(x.mem_read(materials,1200))==wanted_materials
        assert bytes(x.mem_read(b+0x7000,72))==wanted_glare
    for position in (0,2,7,9):
        bad=args[:];bad[position]=0;guarded(bad,-4,[],material_before,glare_before)
    x.mem_write(b+0x6004,w(0));guarded(args,-4,[],material_before,glare_before);x.mem_write(b+0x6004,w(b+0x6500))
    failure=b+0xf100;guarded(args,-1,[('materials',)],material_before,changed)
    failure=b+0xf200;failure_texture_number=2
    partial=bytearray(material_before);partial[16:20]=w(0x900000+sum(b'a.tga'))
    guarded(args,-1,[('materials',),('texture',b'a.tga'),('texture',b'missing')],partial,changed)
    failure=failure_texture_number=0
report=dict(result='PASS',cases=2048,selected=selected_count,missing=misses,texture_calls=texture_calls,
            glare_updates=glare_updates,original_sha256=digest,
            scope='Full410d30 with actual ASCII case-insensitive string equality,4153e0 glare retarget and48ac00 ordered material replacement. '
            'Supplied model-material lookup and texture handles. Exact actor/glare/material bytes and call sequence. '
            'No shared C skin binding, full class parser, actual texture residency or native rendering proof.')
if shared:
    report['nxdk_error_cases']=7
    report['scope']='Full original410d30/4153e0/48ac00 versus shared PC/compiled NXDK: exact selected index, texture calls, material records and glare views. Supplied material lookup/texture loader. No live scene/native Xbox proof.'
(root/('artifacts/clutter-skin-shared.json' if shared else 'artifacts/clutter-skin.json')).write_text(json.dumps(report,indent=2));print(report)
