"""Original actor/clutter/corpse factory append regions and normalized list links.

No factories or constructors are replaced and claimed executed: only the
explicit final append instruction regions run. Allocation/lifetime is separate.
"""
import hashlib,json,random,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EBP

base=0x30000000;stack=base+0x70000;stop=base+0x71000;portable=base+0x60000
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
def machine(path):
    p=pefile.PE(str(path));image=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(image)+4095)//4096*4096)
    m.mem_write(p.OPTIONAL_HEADER.ImageBase,image);m.mem_map(base,0x80000);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
symbols=(root/'build/xbox/main.map').read_text()
symbol=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
read=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def call(name,*args):
    x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(symbol(name),stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4

families=[('actors',0x5cb060,0x62f2d4,0x4236ad,0x4236fc),
          ('clutter',0x5c9360,0x5c9358,0x4109d4,0x410a0b),
          ('corpses',0x5cabb8,0x5caed0,0x416d75,0x416da3)]
rng=random.Random(0x4991c0);appends=0;report_rows=[]
for name,sentinel,counter,entry,end in families:
    total=0
    for epoch in range(64):
        initial=rng.choice((0,0x7ffffffe,0xfffffffe,0xffffffff))
        u.mem_write(sentinel+0x28c,w(sentinel,sentinel));u.mem_write(counter,w(initial))
        call('rf_object_list_init',portable);x.mem_write(portable+8,w(initial))
        order=list(range(16));rng.shuffle(order);live=[]
        for index in order:
            node=base+index*0x2000;link=portable+0x100+index*8
            before=bytearray(rng.randbytes(0x1494));u.mem_write(node,bytes(before))
            u.reg_write(UC_X86_REG_ESI,node);u.reg_write(UC_X86_REG_EBP,0)
            u.reg_write(UC_X86_REG_ESP,stack);u.mem_write(stack,b'\0'*0x100)
            previous=sentinel if not live else base+live[-1]*0x2000
            u.emu_start(entry,end,count=1000)
            assert u.reg_read(UC_X86_REG_EIP)==end and u.reg_read(UC_X86_REG_ESP)==stack
            before[0x28c:0x294]=w(sentinel,previous)
            if name=='actors':
                before[0x13ec:0x13f4]=w(0,0);before[0x1384:0x138c]=w(0xffffffff,0xffffffff)
            assert bytes(u.mem_read(node,len(before)))==before,(name,epoch,index,'write footprint')
            call('rf_object_list_append',portable,link);live.append(index)
            assert read(u,counter)==read(x,portable+8)==(initial+len(live))&0xffffffff
            # Both directions and each endpoint must retain creation order.
            for reverse in (False,True):
                offset=0x290 if reverse else 0x28c;other=4 if reverse else 0
                a=read(u,sentinel+offset);b=read(x,portable+other);seen=[]
                while a!=sentinel:
                    assert len(seen)<16
                    identity=(a-base)//0x2000;assert a==base+identity*0x2000
                    assert b==portable+0x100+identity*8
                    seen.append(identity);a=read(u,a+offset);b=read(x,b+other)
                assert b==portable and seen==(list(reversed(live)) if reverse else live)
            total+=1;appends+=1
    report_rows.append(dict(family=name,sentinel=hex(sentinel),region=[hex(entry),hex(end)],appends=total))
report=dict(result='PASS',appends=appends,families=report_rows,original_sha256=digest,
            scope='Unhooked original final factory append regions versus existing compiled NXDK intrusive append: '
            'full new-node write footprint, forward/reverse order and wrapping family counters. '
            'No full factory execution, removals, registration ordering or live visibility binding proof.')
(root/'artifacts/visibility-lists.json').write_text(json.dumps(report,indent=2));print(report)
