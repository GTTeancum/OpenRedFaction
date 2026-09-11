"""Original505a40 handle decoding and505680 reset vs shared PC/NXDK."""
import hashlib,itertools,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(i&0xffffffff for i in v))
b=0x30000000;stack=b+0xe000;stop=b+0xf000
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');trace=bytes(8);mutation=0;selected=0
entry=int(re.search(r'_rf_audio_control_stop\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def hook(m,address,size,data):
    global trace
    sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<3I',m.mem_read(sp,12))
    trace=w(1,args[1] if m is u else args[2])
    pointer=(0x1753c38 if m is u else b)+selected*44
    if mutation:m.mem_write(pointer,bytes(v^0x5a for v in m.mem_read(pointer,44)))
    m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,hook,begin=0x5442b0,end=0x5442b0);x.hook_add(UC_HOOK_CODE,hook,begin=b+0x3000,end=b+0x3000)
rng=random.Random(0x505a40);commands=[];expected=[];stops=0
for slot,generation,stale,device,enabled,mutation in itertools.product((0,1,29,30,255),(0,1,8388607,-8388608,-1),(0,1),(-2,-1,0,1,2147483647),(0,1,256,257,2),(0,1)):
    selected=slot if slot<30 else 0;handle=((generation<<8)|slot)&0xffffffff
    raw=bytearray(w(*[rng.getrandbits(32) for _ in range(11)]));raw[:4]=w(device);raw[12:16]=w(generation+stale)
    commands.append(bytes(raw)+w(enabled,handle,mutation))
    u.mem_write(0x1753c38,bytes([0xa5])*1320);u.mem_write(0x1753c38+selected*44,bytes(raw));u.mem_write(0x17543d8,w(enabled&255))
    trace=bytes(8);u.mem_write(stack,w(stop,handle));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x505a40,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    should_stop=slot<30 and not stale and device>=0 and bool(enabled&255)
    assert struct.unpack('<I',trace[:4])[0]==int(should_stop);stops+=should_stop
    want=bytes(u.mem_read(0x1753c38+selected*44,44))+trace;expected.append(want)
    x.mem_write(b,bytes([0xa5])*1320);x.mem_write(b+selected*44,bytes(raw));trace=bytes(8)
    x.mem_write(stack,w(stop,b,enabled,handle,b+0x3000,0));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert bytes(x.mem_read(b+selected*44,44))+trace==want,('NXDK',slot,generation,stale,device,enabled,mutation)
    assert bytes(x.mem_read(b,1320))==bytes(u.mem_read(0x1753c38,1320)),('other slot mutation',slot)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--control-stop'],input=b''.join(commands))
assert actual==b''.join(expected),'PC control stop mismatch'
report=dict(result='PASS',cases=len(commands),device_stops=stops,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Full original505a40 and505680 with5442b0 device stop supplied. PC/NXDK exact record bytes and stop count/device argument; NXDK all30 records compared. Low-byte enabled, slot bounds, signed generation extremes, stale/device-negative guards, valid zero handle and callback mutations. No native device stop or game-handle allocation/refresh integration.')
(root/'artifacts/audio-control-stop.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
