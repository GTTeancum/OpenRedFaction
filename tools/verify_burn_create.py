"""Shared burn creation against complete original42e910 allocation traces."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
ev=runpy.run_path(str(root/'tools/verify_burn_creation_trace.py'));cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_burn_create\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];gate=mask=emitters=template=0
def hook(m,address,size,context):
    global emitters,template
    if address not in [b+0x3000+i*16 for i in range(6)]:return
    kind=(address-b-0x3000)//16;sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<5I',m.mem_read(sp,20));result=0
    if kind==0:trace.append(0x42f810 if a[2]==0xffffffff else 0x42f840);template=a[2]
    elif kind==1:
        stage,target=a[2:4];assert target==0x12340001;trace.append((0x426fc0,0x40a1e0,0x42cca0,0x5001d0)[stage]);result=int(gate!=stage+1 if stage<2 else gate==stage+1)
    elif kind==2:
        assert a[2]==0x12340001;trace.append(0x42eb20);m.mem_write(a[3],w(10,11,12,13));result=int(gate!=5)
    elif kind==3:
        assert a[2]==0x12340001 and template==(0 if emitters<3 else 1);trace.append(0x497ca0);result=0 if mask&(1<<emitters) else 0x45670000+emitters;emitters+=1
    elif kind==4:trace.append(0x434da0);result=0xffffffff if mask&16 else 28
    else:
        assert a[2:4]==(0x12340001,0xffffffff if mask&16 else 28);trace.append(0x5056a0);result=0xffffffff if mask&32 else 0x56780001
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
x.hook_add(UC_HOOK_CODE,hook)
for offset,value in ((512,9),(512,1)):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(value);cases.append(bytes(wire));expected.append(w(-2)+wire[:524]+w(0xa5a5a5a5)+bytes(68))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-create'],input=b''.join(cases));assert len(actual)==len(cases)*600
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*600:(i+1)*600]==want,('PC',i,[(j,a,c) for j,(a,c) in enumerate(zip(actual[i*600:(i+1)*600],want)) if a!=c][:12])
    gate,mask=struct.unpack('<2I',wire[524:]);trace=[];emitters=0;template=0
    x.mem_write(b,wire[:524]);x.mem_write(b+0x2000,w(*[b+0x3000+j*16 for j in range(6)],0));x.mem_write(b+0x2100,w(0xa5a5a5a5))
    x.mem_write(stack,w(stop,b,0x12340001,0x23450002,b+0x2000,b+0x2100));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,524))+bytes(x.mem_read(b+0x2100,4))+w(len(trace))+w(*trace).ljust(64,b'\0')
    assert got==want,('NXDK',i,[(j,a,c) for j,(a,c) in enumerate(zip(got,want)) if a!=c][:12])
report=dict(result='PASS',original_cases=len(ev["wire_cases"])-2,port_guards=2,pc_nxdk_cases=len(cases),original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete42e910 routing and ring/payload writes normalized to slot indices, compared on PC/NXDK. Supplied predicates, bones, descriptor preparation, emitter and audio backends; callback order and arguments checked. No live effect adapters or per-frame burn update.')
(root/'artifacts/burn-create.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
