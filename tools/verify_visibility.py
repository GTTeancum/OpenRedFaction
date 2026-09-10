"""Shared room bookkeeping versus original reset/view/visit across many views."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_room_visibility_lifecycle.py')))
u,base,stack,stop,root,put,call=(c[k] for k in ('u','base','stack','stop','root','put','call'))
x=c['c']['x']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
symbols=(root/'build/xbox/main.map').read_text()
entries=[int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16) for name in
    ('rf_visibility_begin_render','rf_visibility_begin_view','rf_visibility_visit')]
world=base+0x2000;array=base+0x3000;rect=base+0x4000
for offset in (0x90,0x9c):put(world+offset,8);put(world+offset+8,array)
for i in range(8):
    put(array+4*i,base+0x10000+i*384);u.mem_write(base+0x10000+i*384,bytes(384))
u.mem_write(0x9a8548,bytes(32));put(0x9bb57c,0)
state=base+0x5000;rooms=base+0x6000;order=base+0x7000
x.mem_write(state,struct.pack('<4I',rooms,order,8,0));x.mem_write(rooms,bytes(224));x.mem_write(order,bytes(32))
commands=bytearray();expected=bytearray();rng=random.Random(0x4d4860)
for step in range(512):
    op=0 if step%32==0 else 1 if step%16==1 else 2
    index=rng.randrange(8);depth=rng.randrange(16)
    rectangle=tuple(rng.randint(-100,100)/4 for _ in range(4))
    if op==0:call(0x4d2f80,this=world)
    elif op==1:call(0x4d4be0,(world,));put(0x9bb57c,0)
    else:
        u.mem_write(rect,struct.pack('<4f',*rectangle));call(0x4d4860,(world,base+0x10000+index*384,rect,depth,1,0))
    out=bytearray(4)+u.mem_read(0x9bb57c,4)
    for i in range(8):
        raw=bytes(u.mem_read(base+0x10000+i*384,384))
        out.extend(struct.pack('<2I',raw[0x160],raw[0x161])+raw[0x164:0x168]+raw[0x16c:0x17c])
    for pointer in struct.unpack('<8I',u.mem_read(0x9a8548,32)):
        out.extend(struct.pack('<I',(pointer-base-0x10000)//384 if pointer else 0))
    expected.extend(out);commands.extend(struct.pack('<3I4f',op,index,depth,*rectangle))
    x.mem_write(rect,struct.pack('<4f',*rectangle))
    args=(state,) if op<2 else (state,index,rect,depth)
    x.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entries[op],stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(state+12,4))+bytes(x.mem_read(rooms,224))+bytes(x.mem_read(order,32))
    assert actual==out,(step,op,[(i,a,b) for i,(a,b) in enumerate(zip(actual,out)) if a!=b][:8])
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--visibility'],input=commands)==expected
report=dict(result='PASS',commands=512,rooms=8,scope='Original room reset, per-view reset and nonrecursive visit bookkeeping versus exact PC/NXDK room fields and full order array. Multiple views preserve render visibility, repeated visits union rectangles and move to tail. Shared caller supplies same room set to resets; original world vectors +90/+9c may differ. Portal traversal and scene integration excluded.')
(root/'artifacts/visibility-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
