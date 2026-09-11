"""Original damage effect traces compared to PC and linked NXDK orchestrator."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
ev=runpy.run_path(str(root/'tools/verify_damage_effects_trace.py'));cases=ev['cases'];expected=ev['expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_entity_damage_effects\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(b+0x3100,b'\xd9\x05'+w(b+0x3200)+b'\xc3');x.mem_write(b+0x3200,struct.pack('<f',4))
trace=[];current=None
def record(address,*args):trace.append(w(address,*args).ljust(24,b'\0'))
def hook(m,address,size,context):
    if address<b+0x3000 or address>b+0x3070 or (address-b)%16:return
    kind=(address-b-0x3000)//16;sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<7I',m.mem_read(sp,28));result=0;facts=current[17:]
    if kind==0:
        stage,handle=a[2:4];record((0x429990,0x4290d0,0x42a8e0,0x429a80,0x4895d0)[stage],b if handle==0x12340001 else b+0x6000)
        result=facts[4 if handle==0x12340001 else 5] if stage==2 else facts[7+stage if stage<2 else 9 if stage==3 else 10]
    elif kind==1:record(0x425210,a[2]);result=0x34560003 if facts[2] else 0xffffffff
    elif kind==2:record(0x426fc0,a[2]);m.mem_write(a[3],w(facts[1]));result=facts[0]
    elif kind==3:record(0x42e910,*a[2:4]);result=b+0x7000 if facts[3] else 0
    elif kind==4:
        record(0x504e40,*a[2:4]);m.reg_write(UC_X86_REG_EIP,b+0x3100);return
    elif kind==5:
        stage,target,value,source=a[2:6];assert target==0x12340001
        if stage==0:record(0x428740,b)
        elif stage==1:record(0x4196f0,b,value)
        elif stage==2:record(0x4089f0,b+0x2a0,value,0)
        elif stage==3:record(0x4085f0,b+0x2a0,source,value)
        elif stage==4:record(0x4a7520)
        elif stage==5:record(0x407fb0,b+0x2a0,source,value,0)
        else:raise AssertionError(stage)
    elif kind==6:record(0x505c00,a[2]);result=facts[6]
    else:record(0x5056a0,0x23,b+0x3c,0x3f800000,0x173c378,0);result=0x76540001
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
x.hook_add(UC_HOOK_CODE,hook)
for offset in (0,4,8,12,44,48,52):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+wire[:44]+bytes(388))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--damage-effects'],input=b''.join(cases));assert len(actual)==len(cases)*436
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*436:(i+1)*436]==want,('PC',i,actual[i*436:(i+1)*436].hex(),want.hex())
    current=struct.unpack('<28I',wire);trace=[];x.mem_write(b,wire[:44]);x.mem_write(b+0x1000,wire[44:68]);x.mem_write(b+0x2000,w(*[b+0x3000+j*16 for j in range(8)],0))
    x.mem_write(stack,w(stop,b,b+0x1000,b+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,44))+w(len(trace))+b''.join(trace)+bytes((16-len(trace))*24)
    assert got==want,('NXDK',i,got.hex(),want.hex())
report=dict(result='PASS',original_cases=8192,port_guards=7,pc_nxdk_cases=len(cases),original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete effect orchestration41a505..41a7ab: exact state and ordered calls against original. Supplied predicates/lookups/random and downstream effects, stable callback state. No live ownership or implemented burn/AI reaction.')
(root/'artifacts/damage-effects.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
