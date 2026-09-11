"""Original5058c0 with actual spatial math vs shared control refresh."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(i&0xffffffff for i in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
b=0x30000000;stack=b+0xe000;stop=b+0xf000
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');trace=[]
entry=int(re.search(r'_rf_audio_control_refresh\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def hook(m,address,size,data):
    sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<4I',m.mem_read(sp,16));original=m is u
    kind=int(address in (0x544450,b+0x3010));values=args[1:3] if original else args[2:4];trace.append(w(kind,*values))
    if not kind and mutation&1:m.mem_write((0x1753c38 if original else b)+selected*44,w(0x2222))
    m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
for address in (0x544390,0x544450):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
for address in (b+0x3000,b+0x3010):x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
rng=random.Random(0x5058c0);commands=[];expected=[];refreshes=0
for case in range(2048):
    slot=(0,29,30,255)[case%4];selected=slot if slot<30 else 0;generation=(0,1,-1,-8388608,8388607)[(case//4)%5]
    stale=(case//20)%2;positional=(0,1,2,256,257)[(case//40)%5];mutation=(case//7)%4
    handle=((generation<<8)|slot)&0xffffffff;device=(-1,0,123)[case%3]
    raw=bytearray(w(*[rng.getrandbits(32) for _ in range(11)]));raw[:16]=w(device,2,0,generation+stale);raw[24:36]=f(1,4,-2);raw[40:44]=w(positional)
    position=f((-40,-10,0,5,10,40)[case%6],case%7-3,case%11-5);volume=(case%17)/8;gain=(case%13)/8
    parameters=f(5,32,.9,1.25);listener=f(1,2,3);right=f(1,0,0)
    commands.append(bytes(raw)+w(handle)+position+f(volume)+parameters+listener+right+f(gain)+w(mutation));assert len(commands[-1])==112
    u.mem_write(0x1753c38,bytes([0xa5])*1320);u.mem_write(0x1753c38+selected*44,bytes(raw));u.mem_write(b+0x1000,position)
    u.mem_write(0x17543d8,w(case&1));u.mem_write(0x1754160,listener);u.mem_write(0x1753c28,right);u.mem_write(0x1753c18,f(gain));u.mem_write(0x1cd3ba8+2*64+0x20,f(.9,5,32,1.25))
    source=0x1753c38+selected*44+24 if mutation&2 else b+0x1000
    trace=[];u.mem_write(stack,w(stop,handle,source,0x173c378)+f(volume));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x5058c0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    refreshes+=bool(trace);want=bytes(u.mem_read(0x1753c38+selected*44,44))+w(len(trace))+b''.join(trace)+bytes((2-len(trace))*12);expected.append(want)
    x.mem_write(b,bytes([0xa5])*1320);x.mem_write(b+selected*44,bytes(raw));x.mem_write(b+0x1000,position);x.mem_write(b+0x2000,parameters+listener+right+f(gain));x.mem_write(b+0x2100,w(b+0x2000,b+0x2010,b+0x201c,b+0x2028));x.mem_write(b+0x2200,w(b+0x3000,b+0x3010))
    source=b+selected*44+24 if mutation&2 else b+0x1000
    trace=[];x.mem_write(stack,w(stop,b,handle,source)+f(volume)+w(b+0x2100,b+0x2200,0));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=bytes(x.mem_read(b+selected*44,44))+w(len(trace))+b''.join(trace)+bytes((2-len(trace))*12)
    assert got==want,('NXDK',case,[(i,a,c) for i,(a,c) in enumerate(zip(got,want)) if a!=c][:12])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--control-refresh'],input=b''.join(commands));assert actual==b''.join(expected),'PC refresh mismatch'
report=dict(result='PASS',cases=len(commands),refreshes=refreshes,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Full5058c0/505740 vs PC/NXDK control refresh;544390/544450 supplied. Exact state and setter order/float args, stale and signed handles, positional low byte, negative devices, ignored global gate, source alias and device mutation between setters. No final device gain conversion or live scene integration.')
(root/'artifacts/audio-control-refresh.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
