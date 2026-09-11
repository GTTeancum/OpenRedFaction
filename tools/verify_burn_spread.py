"""Shared PC/NXDK burn spread against original ordered target processing."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
ev=runpy.run_path(str(root/'tools/verify_burn_spread_trace.py'));cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;owner=b+0x1000;target=b+0x1100;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_burn_spread\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(b+0x3100,b'\xd9\x05'+w(b+0x3200)+b'\xc3');trace=[];gates=[]
def hook(m,address,size,context):
    if address not in (b+0x3000,b+0x3010,b+0x3020):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<7I',m.mem_read(sp,28));result=0
    if address==b+0x3000:
        assert a[3]==target;trace.append(w(ev['predicates'][a[2]],0x30003000).ljust(36,b'\0'));result=gates[a[2]]
    elif address==b+0x3010:
        assert struct.unpack('<I',m.mem_read(target+20,4))[0]&0x2000
        trace.append(w(0x504e40,*a[2:4]).ljust(36,b'\0'));m.reg_write(UC_X86_REG_EIP,b+0x3100);return
    else:
        assert a[2]==target;handle=struct.unpack('<I',m.mem_read(target+24,4))[0]
        trace.append(w(0x4892c0,handle,a[3],a[4],a[5],4,0,a[6],0))
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
x.hook_add(UC_HOOK_CODE,hook)
for offset,value,status in ((40,0x7fc00000,-2),(68,0,-4),(68,1,-4)):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(value);cases.append(bytes(wire));expected.append(w(status,0x800001,0)+bytes(216))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-spread'],input=b''.join(cases));assert len(actual)==len(cases)*228
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*228:(i+1)*228]==want,('PC',i)
    values=struct.unpack('<20I',wire);gates=values[:4];trace=[]
    x.mem_write(owner,w(target)+bytes(24));x.mem_write(target,w(0,*values[4:10]));x.mem_write(b,bytes(64));x.mem_write(b+16,w(values[13]));x.mem_write(b+0x1200,w(*values[10:13]));x.mem_write(b+0x3200,w(values[16]));x.mem_write(b+0x2000,w(b+0x3000,b+0x3010,b+0x3020,0))
    x.mem_write(stack,w(stop,owner,owner,b+0x1200,b,values[14],values[15],values[17],b+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(target+20,4))+w(len(trace))+b''.join(trace)+bytes((6-len(trace))*36)
    assert got==want,('NXDK',i,got.hex(),want.hex())
report=dict(result='PASS',cases=len(cases),original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='3125 original stable-list spread paths and3 explicit input/visit guards, PC and actual NXDK. Exact flags, ordered predicates/random/full damage arguments. Supplied predicates/random/damage, world point and owner identity; excludes world transform, timer, callback mutation and live ownership.')
(root/'artifacts/burn-spread.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
