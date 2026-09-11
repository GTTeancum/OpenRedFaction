"""Shared fade vs original42f2f0, normalized compact emitter views."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
ev=runpy.run_path(str(root/'tools/verify_burn_fade_trace.py'));cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_burn_fade\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];present=0
def hook(m,address,size,context):
    if address not in [b+0x3000+i*16 for i in range(5)]:return
    kind=(address-b-0x3000)//16;sp=m.reg_read(UC_X86_REG_ESP);ret,ctx,arg=struct.unpack('<3I',m.mem_read(sp,12));result=0
    if kind==0:trace.append(w(0x4973d0,arg,0))
    elif kind==1:trace.append(w(0x4174c0,arg,0));result=b+176
    elif kind==2:trace.append(w(0x426fc0,arg,0));result=present
    elif kind==3:assert arg==0x12340001;trace.append(w(0x407ee0,0x300042a0,0))
    else:assert arg==1;trace.append(w(0x42ed20,b,0))
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook)
for offset in (40,48,64,92,120,148):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+wire[:180]+bytes(100))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-fade'],input=b''.join(cases));assert len(actual)==len(cases)*284
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*284:(i+1)*284]==want,('PC',i,[(j,a,c) for j,(a,c) in enumerate(zip(actual[i*284:(i+1)*284],want)) if a!=c][:12])
    deadline,now,present=struct.unpack('<3I',wire[180:]);trace=[];x.mem_write(b,wire[:180]);x.mem_write(b+0x1000,w(*[b+64+j*28 for j in range(4)]));x.mem_write(b+0x2000,w(*[b+0x3000+j*16 for j in range(5)],0))
    x.mem_write(stack,w(stop,b,b+0x1000,1,deadline,now,b+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,180))+w(len(trace))+b''.join(trace)+bytes((8-len(trace))*12)
    assert got==want,('NXDK',i,[(j,a,c) for j,(a,c) in enumerate(zip(got,want)) if a!=c][:12])
report=dict(result='PASS',original_cases=8192,port_guards=6,pc_nxdk_cases=len(cases),original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Exact compact emitter values/bytes, record, type7 flags and ordered callbacks vs complete42f2f0 with original timer. Stable distinct emitters and valid type7 owner; supplied stop/reaction/release. No actual lifecycle conversion or per-frame geometry.')
(root/'artifacts/burn-fade.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
