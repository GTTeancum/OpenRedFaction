"""Compare C SP damage dispatch to the original complete wrapper trace."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
evidence=runpy.run_path(str(root/'tools/verify_damage_wrapper_trace.py'));cases=evidence['cases'];expected=evidence['expected'];w=evidence['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_damage_dispatch_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(b+0x3100,b'\xd9\x05'+w(b+0x3200)+b'\xc3');x.mem_write(b+0x3200,struct.pack('<f',7.25))
trace=[];effect=bytes(24);current=None
def hook(m,address,size,context):
    global effect
    if address not in (b+0x3000,b+0x3010,b+0x3020):return
    sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<8I',m.mem_read(sp,32));result=0
    if address==b+0x3000:
        assert args[2]==0x12340001;trace.append(0x40a0e0);result=b if current[10] else 0
    elif address==b+0x3010:
        stage=args[2];assert args[3:5]==(0x12340001,b)
        trace.append((0x426fc0,0x42cca0,0x48aaf0)[stage]);result=current[11+stage]
    else:
        typ=current[0];assert args[2]==b
        call={0:0x41a350,4:0x410270,7:0x417c60}[typ];trace.append(call)
        effect=w(call,b,*args[3:7]);m.mem_write(b+8,w(current[14]));m.reg_write(UC_X86_REG_EIP,b+0x3100);return
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
x.hook_add(UC_HOOK_CODE,hook)
# Explicit port guards preserve result/object and emit no callbacks.
for offset in (12,36):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+wire[:12]+bytes([0xa5])*4+bytes(60))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--damage-dispatch'],input=b''.join(cases));assert len(actual)==len(cases)*80
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*80:(i+1)*80]==want,('PC',i,actual[i*80:(i+1)*80].hex(),want.hex())
    current=struct.unpack('<15I',wire);trace=[];effect=bytes(24)
    x.mem_write(b,wire[:12]);x.mem_write(b+0x1000,wire[12:36]);x.mem_write(b+0x2000,w(b+0x3000,b+0x3010,b+0x3020,0));x.mem_write(b+0x2100,bytes([0xa5])*4)
    x.mem_write(stack,w(stop,0x12340001,b+0x1000,current[9],b+0x2000,b+0x2100));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,12))+bytes(x.mem_read(b+0x2100,4))+w(len(trace))+w(*trace).ljust(32,b'\0')+effect
    assert got==want,('NXDK',i,got.hex(),want.hex())
report=dict(result='PASS',original_cases=8192,port_guards=2,pc_nxdk_cases=len(cases),original_sha256=evidence['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Exact state, return and ordered callback arguments against original SP4892c0 trace with supplied lookup/predicates/effects. No actual effect lifecycle, campaign registration or alternate6fc4d8 mode.')
(root/'artifacts/damage-dispatch.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
