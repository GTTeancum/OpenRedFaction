"""Original owned-player animation selector with ordinary entity fixtures.

Executes 4a5cd0 and every callee without hooks. Registered logical motions use
identity IDs to observe requests; this does not sample loaded animation poses.
"""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536)
player,entity,info,mode,stack,stop=[base+n for n in (0,0x2000,0x4000,0x6000,0xe000,0xf000)]
pack=lambda v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
def put(a,*v):u.mem_write(a,pack(v))
f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
u.mem_write(0x7394cc,bytes(4096));put(0x7394cc,entity)
put(player+0x14,0x10000);put(entity+0x24,0);put(entity+0x2c,0x10000)
put(entity+0x294,info);put(entity+0x29c,info);put(entity+0x858,mode)
put(entity+0x200,-1);put(entity+0x1380,-1)
for i in range(23):put(entity+0x8e4+i*16,i)
directions=[(0,0,0),(.1,0,0),(-.1,0,0),(.10001,0,0),(-.10001,0,0),
            (0,1,0),(0,0,.1),(0,0,-.10001),(.2499,0,0),(.25,0,0),(.2,.2,0),(.15,.15,.15),(1,0,1)]
records=[];counts={}
for crouched,movement,attached,primary,hidden,direction in itertools.product((0,1),range(16),(-1,7),(-1,5),(0,1),directions):
    direction=tuple(map(f32,direction));u.mem_write(player+0xb1,bytes([crouched]))
    put(mode+4,movement);put(entity+0x75c,attached);put(entity+0x2a4,primary);put(entity+0x810,0x800 if hidden else 0)
    u.mem_write(entity+0x714,struct.pack('<3f',*direction));u.mem_write(entity+0x138c,struct.pack('<iiff',0,-1,0,0))
    before=bytes(u.mem_read(base,0x8000));put(stack,stop,player);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x4a5cd0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    after=bytes(u.mem_read(base,0x8000));a,b=entity-base+0x138c,entity-base+0x139c
    assert before[:a]==after[:a] and before[b:]==after[b:]
    current,next_state,duration,elapsed=struct.unpack('<iiff',after[a:b])
    if crouched:selected=10 if abs(direction[0])>f32(.1) or abs(direction[2])>f32(.1) else 9
    elif movement in (3,8):selected=14
    elif movement in (4,7):selected=19 if any(direction) else 18
    else:
        small,middle,large=sorted(map(abs,direction))
        moving=large+(small*.125+middle*.25)*1.5>=.25
        if attached!=-1:selected=17 if moving else 16
        elif primary!=-1 and not hidden:selected=5 if moving else 1
        else:selected=4 if moving else 0
    assert (current,next_state,duration,elapsed)==(0,selected if selected else -1,.25 if selected else 0,0)
    counts[selected]=counts.get(selected,0)+1
    records.append(dict(crouched=crouched,mode=movement,attachment=attached,primary=primary,hidden=hidden,
                        direction=direction,selected=selected,controller=[current,next_state,duration,elapsed]))
parent,parent_info,seats,first,second=[base+n for n in (0x8000,0xa000,0xc000,0xc100,0xc120)]
put(0x7394cc+4,parent);put(parent+0x24,0);put(parent+0x2c,0x10001);put(parent+0x294,parent_info)
put(parent_info+0x724,0x400000);put(parent+0x8cc,2,2,seats);put(seats,first,second)
for present,parent_kind,first_seat,second_seat,crouched,movement in itertools.product((0,1),(0,4),(0,1),(0,1),(0,1),(1,3)):
    put(player+0x14,0x10000 if present else -1);put(entity+0x200,0x10001)
    put(parent_info+0x1b4,parent_kind);put(first+4,0x10000 if first_seat else -1);put(second+4,0x10000 if second_seat else -1)
    u.mem_write(player+0xb1,bytes([crouched]));put(mode+4,movement);put(entity+0x75c,-1);put(entity+0x2a4,5);put(entity+0x810,0)
    u.mem_write(entity+0x714,bytes(12));u.mem_write(entity+0x138c,struct.pack('<iiff',0,-1,0,0))
    before=bytes(u.mem_read(base,0xd000));put(stack,stop,player);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x4a5cd0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    after=bytes(u.mem_read(base,0xd000));a,b=entity-base+0x138c,entity-base+0x139c
    assert before[:a]==after[:a] and before[b:]==after[b:]
    controller=struct.unpack('<iiff',after[a:b])
    selected=-1 if not present else 15 if parent_kind==4 else 20 if first_seat else 21 if second_seat else 9 if crouched else 14 if movement==3 else 1
    assert controller==(0,selected if selected>0 else -1,.25 if selected>0 else 0,0)
    counts[selected]=counts.get(selected,0)+1
    records.append(dict(present=present,parent_kind=parent_kind,first_seat=first_seat,second_seat=second_seat,
                        crouched=crouched,mode=movement,attachment=-1,primary=5,hidden=0,direction=[0,0,0],selected=selected,controller=controller))
report=dict(result='PASS',original_sha256=sha,cases=len(records),selected_counts=counts,
 scope='Complete original 4a5cd0 and all callees, no hooks; ordinary cases plus registered parent/seat and missing-entity fixtures, mapped logical motions 0..22, fresh controller. Tests all selected-state branches and parent/seat precedence. Actual assets, outer stance effects and C selector integration remain separate.',records=records)
(root/'artifacts/player-motion-reference.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='records'},indent=2))
