"""Original damage effect traces compared to PC and linked NXDK orchestrator."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
full='--full' in sys.argv
ev=runpy.run_path(str(root/'tools/verify_damage_effects_trace.py'),init_globals={'full':full});cases=ev['cases'];expected=ev['expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_entity_damage_'+('sp' if full else 'effects')+r'\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(b+0x3100,b'\xd9\x05'+w(b+0x3200)+b'\xc3');x.mem_write(b+0x3200,struct.pack('<f',4))
trace=[];current=None
def record(address,*args):
    trace.append(w(address,*args).ljust(24,b'\0'))
    mutation=current[28:]
    if address==mutation[0]:
        x.mem_write(b,w(mutation[1]))
        for off,mask in zip((20,24,32,36),mutation[2:]):
            x.mem_write(b+off,w(struct.unpack('<I',x.mem_read(b+off,4))[0]^mask))
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
for offset in (() if full else (0,4,8,12,44,48,52)):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+wire[:44]+bytes(388))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--damage-full' if full else '--damage-effects'],input=b''.join(cases));size=452 if full else 436;assert len(actual)==len(cases)*size
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*size:(i+1)*size]==want,('PC',i,actual[i*size:(i+1)*size].hex(),want.hex())
    current=struct.unpack('<39I' if full else '<34I',wire);trace=[];x.mem_write(b,wire[:44]);x.mem_write(b+0x1000,wire[44:68]);x.mem_write(b+0x2000,w(*[b+0x3000+j*16 for j in range(8)],0))
    if full:
        x.mem_write(b+44,w(*current[36:39]));x.mem_write(b+0x2100,bytes([0xa5])*4)
        args=w(stop,b,current[11],current[14],current[15],current[16],current[34],current[35],b+0x2000,b+0x2100)
    else:args=w(stop,b,b+0x1000,b+0x2000)
    x.mem_write(stack,args);x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,56 if full else 44))+(bytes(x.mem_read(b+0x2100,4)) if full else b'')+w(len(trace))+b''.join(trace)+bytes((16-len(trace))*24)
    assert got==want,('NXDK',i,got.hex(),want.hex())
report=dict(full_entity_function=full,result='PASS',original_cases=16384,mutation_cases=8192,port_guards=0 if full else 7,pc_nxdk_cases=len(cases),original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete effect orchestration41a505..41a7ab: exact state and ordered calls against original. Supplied predicates/lookups/random and downstream effects, including8192 mutation scenarios for health/flags/burn/voice during effect callbacks. No live ownership or implemented burn/AI reaction.')
if full:report['scope']='Composed SP entity damage vs original41a350 entry through return preparation41a7ab. Exact vitals, time, lethal credit, effect state, prepared float return and ordered callbacks;8192 mutation scenarios. Supplied UID/entity lookups and downstream effects; no live campaign ownership or multiplayer.'
(root/('artifacts/damage-full.json' if full else 'artifacts/damage-effects.json')).write_text(json.dumps(report,indent=2)+'\n');print(report)
