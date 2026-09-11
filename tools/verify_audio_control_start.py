"""Original505560 full control-table allocation vs shared PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(i&0xffffffff for i in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
b=0x30000000;stack=b+0xe000;stop=b+0xf000
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_audio_control_start\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
addresses={0x5054d0:0,0x544360:1,0x5442b0:2,0x5439d0:3,0x543a80:3,**{b+0x3000+i*16:i for i in range(4)}}
trace=[];queried=0
def hook(m,address,size,data):
    global queried
    if address not in addresses:return
    kind=addresses[address];sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<7I',m.mem_read(sp,28));original=m is u
    args=a[1:] if original else a[2:];table=0x1753c38 if original else b
    gp=0x1753c18+category*4 if original else b+0x2000;lp=0x1cd3be6+sample*64 if original else b+0x2004
    result=0
    if kind==0:trace.append(w(0,args[0],0,0,0,0));result=ready
    elif kind==1:
        trace.append(w(1,args[0],0,0,0,0));result=playing[queried]
        if not queried and mutation&1:
            m.mem_write(table,w(0x22222));m.mem_write(gp,f(.5));m.mem_write(lp,bytes([m.mem_read(lp,1)[0]^1]))
        queried+=1
    elif kind==2:trace.append(w(2,args[0],0,0,0,0))
    else:
        loop=int(address==0x543a80) if original else args[3]
        if original:assert args[3:5]==(0,0)
        trace.append(w(3,*args[:3],loop,0));result=device
        if mutation&2:
            for i in range(30):
                if struct.unpack('<i',m.mem_read(table+i*44+4,4))[0]<0:m.mem_write(table+i*44+12,w(-1));break
    m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook,begin=b+0x3000,end=b+0x3040)
rng=random.Random(0x505560);commands=[];expected=[];starts=0;exhausted=0
for case in range(1024):
    enabled=(0,1,256,257,2)[case%5];ready=(-1,-2,0,123)[(case//5)%4];device=(-1,-2,0,17,2147483647)[(case//7)%5]
    sample=5;category=case%8;pan=(case%9-4)/4;volume=(case%17)/8;gain=(case%13)/8;loop=(0,1,2,255)[case%4];mutation=(case//11)%4
    voices=bytearray();playing=[]
    for i in range(30):
        raw=bytearray(w(*[rng.getrandbits(32) for _ in range(11)]));raw[:8]=w(i,-1 if (case%4==1 and i==29) or (case%4==2 and i==0) else 4)
        raw[12:16]=w((0,0x7fffff,0x800000,0xffffff,0x7fffffff,0xffffffff,0x80000000)[(case+i)%7]);voices+=raw
        playing.append(1 if case%4!=3 else rng.choice((0,1,256,257,2)))
    commands.append(bytes(voices)+w(enabled,sample,category)+f(pan,volume,gain)+w(loop,ready,device,*playing,mutation));assert len(commands[-1])==1480
    u.mem_write(0x1753c38,bytes(voices));u.mem_write(0x17543d8,w(enabled&255));u.mem_write(0x1753c18+category*4,f(gain));u.mem_write(0x1cd3be6+sample*64,bytes([loop]))
    trace=[];queried=0;u.mem_write(stack,w(stop,sample,category)+f(pan,volume));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x505560,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    starts+=any(struct.unpack('<I',r[:4])[0]==3 for r in trace);exhausted+=queried==30 and not any(struct.unpack('<I',r[:4])[0]==3 for r in trace)
    want=w(u.reg_read(UC_X86_REG_EAX))+bytes(u.mem_read(0x1753c38,1320))+w(len(trace))+b''.join(trace)+bytes((64-len(trace))*24);expected.append(want)
    x.mem_write(b,bytes(voices));x.mem_write(b+0x2000,f(gain)+w(loop));x.mem_write(b+0x2100,w(*[b+0x3000+i*16 for i in range(4)]))
    trace=[];queried=0;x.mem_write(stack,w(stop,b,enabled,sample,category)+f(pan,volume)+w(b+0x2000,b+0x2004,b+0x2100,0));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,1320))+w(len(trace))+b''.join(trace)+bytes((64-len(trace))*24)
    assert got==want,('NXDK',case,[(i,a,c) for i,(a,c) in enumerate(zip(got,want)) if a!=c][:20])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--control-start'],input=b''.join(commands))
assert actual==b''.join(expected),('PC',[(i,a,c) for i,(a,c) in enumerate(zip(actual,b''.join(expected))) if a!=c][:20])
report=dict(result='PASS',cases=len(commands),play_requests=starts,exhausted=exhausted,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original505560 and505680 reset vs PC/NXDK table/handle and ordered callback args. Sample preparation, status, device stop/start supplied. All30 cleanup, negative-sample selection, prepare/device failure, low bytes, generation wrapping, preserved positional upper bytes and callback metadata/generation mutations. No actual sample loading/device playback or positional wrapper.')
(root/'artifacts/audio-control-start.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
