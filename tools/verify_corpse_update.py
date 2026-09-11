"""Shared full corpse update versus original trace, including mutable playback."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;nodes=b+0x1000;enabled=b+0x2000;sound=b+0x3000;backend=b+0x4000;callbacks=b+0x5000;duration_ptr=b+0x6000;stub=b+0x7000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda v:struct.pack('<f',v)
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im);x.mem_map(b,65536);x.reg_write(UC_X86_REG_FPCW,0x27f)
x.mem_write(stub,b'\xdd\x05'+w(duration_ptr)+b'\xc3')
entry=int(re.search(r'\s_rf_corpse_update\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
opcodes=[0x5033f0,0x5033b0,0x5033e0,0x503360,0x4164c0,0x459a20,0x48ac70,0x48a230]
counts=[2,3,3,5,1,2,2,3];trace=[];found=mutation=0;inputs=[];outputs=[]
point=struct.pack('<3f',1.25,-2,3)
def read(a,n):return list(struct.unpack('<'+'I'*n,x.mem_read(a,4*n)))
def hook(m,a,size,data):
    if not callbacks<=a<callbacks+128 or (a-callbacks)%16:return
    op=(a-callbacks)//16;sp=m.reg_read(UC_X86_REG_ESP);ret=read(sp,1)[0];args=read(sp+4,counts[op]);result=0
    if op==0:
        row=[args[1]]
        if mutation:m.mem_write(b+28,w(args[1]+100));m.mem_write(b+32,w(7))
    elif op in (1,2):row=args[1:]+([0x3f800000,1] if op==1 else [])
    elif op==3:
        assert args[3] in (0,b+40) and args[4] in (0,b+52)
        row=[args[1],args[2],0,0x3000003c if args[3] else 0,0x30000048 if args[4] else 0,1]
    elif op==4:assert not read(b+12,1)[0]&8;row=[b]
    elif op==5:row=[args[1]];result=sound if found else 0
    elif op==6:row=[b];m.mem_write(args[1],point)
    else:
        assert args[1]==sound and bytes(m.mem_read(sound,12))==point and bytes(m.mem_read(args[2],12))==point
        row=[sound,*struct.unpack('<3I',point)]
    trace.append([opcodes[op],row])
    if op==2:m.reg_write(UC_X86_REG_EIP,stub);return
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook)
def state(body,scalar):
    return body[0x34:0x38]+body[0x298:0x29c]+body[0x7c:0x80]+body[0x29c:0x2a0]+body[0x2ac:0x2b4]+f(scalar)+body[0x80:0x84]+body[0x2b8:0x2bc]+body[0x2cc:0x2d0]+body[0x3c:0x6c]
def compare(c):
    global trace,found,mutation
    wire=state(c['input_body'],c['class_value']);assert len(wire)==88
    flags=[struct.unpack('<I',e[0x140:0x144])[0] for e in c['input_emitters']]+[0]*(4-c['n'])
    want_flags=[struct.unpack('<I',e[0x140:0x144])[0] for e in c['emitter_bodies']]+[0]*(4-c['n'])
    found=c['found'];mutation=c['reset_mutation'];trace=[]
    x.mem_write(b,wire);x.mem_write(enabled,w(*flags));x.mem_write(sound,bytes(16));x.mem_write(duration_ptr,struct.pack('<d',c['duration']))
    for i in range(c['n']):x.mem_write(nodes+i*8,w(nodes+(i+1)*8 if i+1<c['n'] else 0,enabled+4*i))
    x.mem_write(backend,w(*[callbacks+i*16 for i in range(8)],0));dt=struct.unpack('<I',f(c['dt']))[0]
    x.mem_write(stack,w(stop,b,dt,1000,nodes if c['n'] else 0,c['n'],backend));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,c['case']
    assert trace==c['trace'],(c['case'],trace,c['trace'])
    final=state(c['body'],c['class_value']);assert bytes(x.mem_read(b,88))==final,c['case'];assert read(enabled,4)==want_flags
    want_sound=point if any(a==0x48a230 for a,args in trace) else bytes(12);assert bytes(x.mem_read(sound,12))==want_sound
    rows=b''.join(w(a,*args).ljust(28,b'\0') for a,args in trace).ljust(336,b'\0')
    inputs.append(wire+w(dt,1000,c['n'],*flags,found,mutation)+struct.pack('<d',c['duration']))
    outputs.append(w(0)+final+w(*want_flags)+want_sound+w(len(trace))+rows)
runpy.run_path(str(root/'tools/verify_corpse_update_original.py'),init_globals={'observe_case':compare})
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-update'],input=b''.join(inputs))
assert actual==b''.join(outputs),'PC state/trace mismatch'
report=dict(result='PASS',cases=len(inputs),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full417290 orchestration matches shared PC/NXDK in4096 original cases, including timer/emitter writes, fade/value decay, mutable reset model/motion, exact animation arguments and sound follow state. Model/pose/sound effect implementations remain supplied. No live corpse creation/rendering/deletion or native scene dispatch.')
(root/'artifacts/corpse-update.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
