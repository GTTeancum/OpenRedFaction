"""Shared burn owner tail vs original audio/damage/fade update."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
ev=runpy.run_path(str(root/'tools/verify_burn_tick_trace.py'));cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_burn_owner_tick\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(b+0x3100,b'\xd9\x05'+w(b+0x3200)+b'\xc3');trace=[];random_integer=0
def hook(m,address,size,context):
    if address not in [b+0x3000+i*16 for i in range(5)]:return
    kind=(address-b-0x3000)//16;sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<6I',m.mem_read(sp,24));result=0
    if kind==0:row=w(0x5058c0,a[2])+bytes(m.mem_read(a[3],12))+bytes(m.mem_read(a[4],12))+w(a[5])
    elif kind==1:row=w(0x504e40,*a[2:4]).ljust(36,b'\0')
    elif kind==2:row=w(0x4892c0,*a[2:4],-1,-1,4,0,-1,0)
    elif kind==3:row=w(0x57312d).ljust(36,b'\0');result=random_integer
    else:assert a[2]==1;row=w(0x42f2f0,b).ljust(36,b'\0')
    trace.append(row)
    if kind==1:m.reg_write(UC_X86_REG_EIP,b+0x3100);return
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
x.hook_add(UC_HOOK_CODE,hook)
for offset in (40,48,64,80,92,104):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+wire[:104]+bytes(148))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-owner'],input=b''.join(cases));assert len(actual)==len(cases)*256
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*256:(i+1)*256]==want,('PC',i,[(j,a,c) for j,(a,c) in enumerate(zip(actual[i*256:(i+1)*256],want)) if a!=c][:12])
    delta,divisor,random_integer=struct.unpack('<3I',wire[104:]);trace=[];x.mem_write(b,wire[:104]);x.mem_write(b+0x3200,w(divisor));x.mem_write(b+0x2000,w(*[b+0x3000+j*16 for j in range(5)],0))
    x.mem_write(stack,w(stop,b,b+64,1,delta,b+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,104))+w(len(trace))+b''.join(trace)+bytes((4-len(trace))*36)
    assert got==want,('NXDK',i,[(j,a,c) for j,(a,c) in enumerate(zip(got,want)) if a!=c][:12])
report=dict(result='PASS',original_cases=4096,port_guards=6,pc_nxdk_cases=len(cases),original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Owner tail42f1dc..42f2a2: exact record/owner state and normalized audio vectors, damage/random/fade calls on PC/NXDK. Original427020 unchanged; downstream calls/random supplied. No attachment/spread or actual damage/fade integration.')
(root/'artifacts/burn-owner.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
