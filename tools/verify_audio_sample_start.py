"""Original5054d0 and5439d0/543a80 boundaries vs shared PC/NXDK."""
import hashlib,itertools,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(i&0xffffffff for i in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;modep=b+0x2000;state=modep+16;gainp=state+16;preparedp=gainp+8;backend=preparedp+16
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');symbols=(root/'build/xbox/main.map').read_text();trace=[]
entries=[int(re.search('_rf_audio_sample_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16) for name in ('start','prepare')]
def hook(m,address,size,data):
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<7I',m.mem_read(sp,28));original=m is u
    load=address in (0x543760,b+0x3000);args=a[1:] if original else a[2:]
    if load:
        if original:assert args[1:3]==(0,0)
        trace.append(w(0,args[0],0,0,0,0,0));result=loaded
        if mutation:
            if original:
                m.mem_write(0x1cfc5d4,w(1));m.mem_write(samplep+0x30,w(99));m.mem_write(samplep+0x20,f(.25));m.mem_write(0x1cd3b94,f(.5));m.mem_write(samplep+0x3d,bytes([7]))
            else:m.mem_write(modep,w(1));m.mem_write(state,w(99)+f(.25));m.mem_write(gainp,f(.5));m.mem_write(preparedp,w(7))
    else:trace.append(w(1,*args[:5],0));result=played
    m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
for address in (0x543760,0x522530):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
for address in (b+0x3000,b+0x3010):x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
rng=random.Random(0x5439d0);reports=[]
for prepare in (0,1):
    commands=[];expected=[];loads=plays=0
    if prepare:
        specs=((en,sa,1,0,0,lo,mu,pr) for en,sa,pr,lo,mu in itertools.product((0,1,256,257,2),(-1,0,5),(0,1,2,255),(-2,-1,0,1,2147483647),(0,1)))
    else:
        specs=((*row,0) for row in itertools.product((0,1,256,257,2),(-1,0,5),(0,1,2),(0,1,2,255,256,257),(0,1),(-2,-1,0,1,2147483647),(0,1)))
    for case,(enabled,sample,mode,bypass,loop,loaded,mutation,prepared) in enumerate(specs):
        volume=rng.randint(0,200)/64;pan=rng.randint(-64,64)/64;default=rng.randint(0,128)/64;gain=rng.randint(0,128)/64;buffer=123;extra=0x12345678;played=(-1,0,123456)[case%3]
        wire=w(enabled,mode,sample)+f(volume,pan)+w(extra,bypass,loop,buffer)+f(default,gain)+w(loaded,played,mutation,prepared);assert len(wire)==60;commands.append(wire)
        samplep=0x1cd3ba8+sample*64;u.mem_write(samplep+0x20,f(default));u.mem_write(samplep+0x30,w(buffer,0));u.mem_write(samplep+0x3d,bytes([prepared]));u.mem_write(0x1cd3b94,f(gain));u.mem_write(0x1cfc5d0,w(enabled&255,mode));u.mem_write(0x17543d8,w(enabled&255))
        trace=[];u.mem_write(stack,w(stop,sample) if prepare else w(stop,sample)+f(volume,pan)+w(extra,bypass));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x5054d0 if prepare else 0x543a80 if loop else 0x5439d0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
        loads+=sum(r[:4]==w(0) for r in trace);plays+=sum(r[:4]==w(1) for r in trace)
        want=w(u.reg_read(UC_X86_REG_EAX))+bytes(u.mem_read(0x1cfc5d4,4))+bytes(u.mem_read(samplep+0x30,4))+bytes(u.mem_read(samplep+0x20,4))+bytes(u.mem_read(0x1cd3b94,4))+w(u.mem_read(samplep+0x3d,1)[0],len(trace))+b''.join(trace)+bytes((2-len(trace))*28);expected.append(want)
        x.mem_write(modep,w(mode));x.mem_write(state,w(buffer)+f(default)+w(gainp));x.mem_write(gainp,f(gain));x.mem_write(preparedp,w(prepared));x.mem_write(backend,w(b+0x3000,b+0x3010))
        args=w(stop,enabled,sample,preparedp,b+0x3000,0) if prepare else w(stop,enabled,modep,sample)+f(volume,pan)+w(extra,bypass,loop,state,backend,0)
        trace=[];x.mem_write(stack,args);x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[prepare],stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
        got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(modep,4))+bytes(x.mem_read(state,8))+bytes(x.mem_read(gainp,4))+bytes(x.mem_read(preparedp,4))+w(len(trace))+b''.join(trace)+bytes((2-len(trace))*28)
        assert got==want,('NXDK',prepare,case,[(i,a,c) for i,(a,c) in enumerate(zip(got,want)) if a!=c][:12])
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--sample-prepare' if prepare else '--sample-start'],input=b''.join(commands));assert actual==b''.join(expected),('PC',prepare)
    reports.append(dict(prepare=bool(prepare),cases=len(commands),loads=loads,plays=plays))
report=dict(result='PASS',suites=reports,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original5054d0/5439d0/543a80 with actual543a60 gain vs PC/NXDK. Loader543760 and device522530 supplied; exact args/state, enabled/bypass low-byte differences, load-result distinction, playback mode and callback metadata mutation. No archive loader, decoder, device allocation or native playback.')
(root/'artifacts/audio-sample-start.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
