"""Original 48a190 control flow; lookup/notification boundaries are fixtures."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_EIP
exe=ROOT/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(raw)+4095)//4096*4096);u.mem_write(0x400000,raw)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;player=base+0x1000
u.mem_map(base,0x10000)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
roomptr=lambda token:base+0x2000+token*0x400 if token else 0
counts=[0,0,0];query_room=0

def boundary(m,address,size,data):
    if address not in (0x4cd970,0x5231e0):return
    sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    if address==0x4cd970:
        args=struct.unpack('<4I',m.mem_read(sp+4,16));assert args==(0,base+0x3c,base+0x3c,0)
        counts[0]+=1;m.reg_write(UC_X86_REG_EAX,roomptr(query_room));sp+=16
    else:
        ptr=struct.unpack('<I',m.mem_read(sp+4,4))[0]
        counts[1]+=1;counts[2]=2 if ptr==0x59f9e8 else 1
        if counts[2]==1:assert ptr==roomptr(query_room)+0x4a
    m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,boundary)
rng=random.Random(0x48a190);inputs=[];expected=[]
for case in range(384):
    old=case%3;query_room=(case//3)%3;local=(case//9)%2;liquid=(case//18)%2
    pos=[rng.uniform(-100,100) for _ in range(3)];pos=list(struct.unpack('<3f',f(*pos)))
    previous=pos[:] if case%4==0 else [v+1 for v in pos]
    if case==0:pos=[0.,0.,0.];previous=[1.401298464324817e-45,0.,0.];old=1
    minimum=pos[1]-2;depth=[1.,2.,3.][case%3];flags=rng.getrandbits(32)
    payload=w(old,flags)+f(*previous)+f(*pos)+w(local,query_room,liquid)+f(minimum,depth)
    inputs.append(payload);counts[:]=[0,0,0]
    u.mem_write(base,bytes(0x4000));u.mem_write(base,w(roomptr(old)))
    u.mem_write(base+4,payload[8:20]);u.mem_write(base+0x3c,payload[20:32]);u.mem_write(base+0x7c,w(flags));u.mem_write(base+0x2c,w(123))
    u.mem_write(0x7c75d4,w(player if local else 0));u.mem_write(player+0x14,w(123))
    if query_room:
        q=roomptr(query_room);u.mem_write(q+0x184,bytes([liquid]));u.mem_write(q+0xc,f(minimum));u.mem_write(q+0x188,f(depth))
    u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
    u.emu_start(0x48a190,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
    outroom=struct.unpack('<I',u.mem_read(base,4))[0];outroom=(outroom-base-0x2000)//0x400 if outroom else 0
    expected.append(w(outroom)+bytes(u.mem_read(base+0x7c,4))+bytes(u.mem_read(base+4,12))+w(*counts))
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_probe.exe'),'--room-refresh'],input=b''.join(inputs))
for i,e in enumerate(expected):assert actual[i*32:(i+1)*32]==e,('Room refresh mismatch',i)
# Execute the compiled NXDK C routine with process-local callback fixtures.
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));raw=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_entity_room_refresh\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
def compiled_callback(m,address,size,data):
    if address not in (base+0x8000,base+0x8100):return
    sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    if address==base+0x8000:
        out=struct.unpack('<I',m.mem_read(sp+12,4))[0]
        m.mem_write(out,callback_result);counts[0]+=1;m.reg_write(UC_X86_REG_EAX,0)
    else:
        ptr=struct.unpack('<I',m.mem_read(sp+8,4))[0]
        counts[1]+=1;counts[2]=2 if bytes(m.mem_read(ptr,10))==b'underwater' else 1
    m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,compiled_callback)
for i,payload in enumerate(inputs):
    counts[:]=[0,0,0];local,room,liquid=struct.unpack('<3I',payload[32:44])
    callback_result=w(room,liquid)+payload[44:52]+w(base+0x5000)
    x.mem_write(base,payload[:20]);x.mem_write(base+0x100,payload[20:32]);x.mem_write(base+0x5000,b'test-room\0')
    x.mem_write(stack,w(stop,base,base+0x100,local,base+0x8000,base+0x8100,0));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base,20))+w(*counts)==expected[i],('NXDK mismatch',i)
report=dict(result='PASS',pc_cases=len(inputs),nxdk_cases=len(inputs),scope='Complete original 48a190 with original squared-distance, vector copy and underwater predicate. Containing-room lookup and notification dispatch are intercepted fixtures; does not validate 4e1630 or runtime integration.')
(ROOT/'artifacts/entity-room-refresh-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
