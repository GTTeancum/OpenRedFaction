"""Shared traversal vs original with the documented missing-owner branch fix."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
ev=runpy.run_path(str(root/'tools/verify_burn_iteration.py'),init_globals={'corrected':True});cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_burn_pool_update\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];missing=0
def hook(m,address,size,context):
    if address not in [b+0x3000+i*16 for i in range(6)]:return
    kind=(address-b-0x3000)//16;sp=m.reg_read(UC_X86_REG_ESP);ret,ctx,arg=struct.unpack('<3I',m.mem_read(sp,12))
    trace.append(w((0x40a0e0,0x42ef3e,0x4973d0,0x497d80,0x505a40,0x42ee13)[kind],arg))
    m.reg_write(UC_X86_REG_EAX,int(arg!=missing) if kind==0 else 0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook)
for offset,value in ((516,9),(56,9)):
    wire=bytearray(cases[0]);wire[offset:offset+4]=w(value);cases.append(bytes(wire));expected.append(w(-2)+wire[:524]+bytes(516))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-update'],input=b''.join(cases));assert len(actual)==len(cases)*1044
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*1044:(i+1)*1044]==want,('PC',i,[(j,a,c) for j,(a,c) in enumerate(zip(actual[i*1044:(i+1)*1044],want)) if a!=c][:12])
    missing=struct.unpack('<I',wire[524:])[0];trace=[];x.mem_write(b,wire[:524]);x.mem_write(b+0x2000,w(b+0x3000,b+0x3010,b+0x2200,0));x.mem_write(b+0x2200,w(b+0x3020,b+0x3030,b+0x3040,b+0x3050,0))
    x.mem_write(stack,w(stop,b,1000,b+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,524))+w(len(trace))+b''.join(trace)+bytes((64-len(trace))*8)
    assert got==want,('NXDK',i,[(j,a,c) for j,(a,c) in enumerate(zip(got,want)) if a!=c][:12])
report=dict(result='PASS',corrected_original_cases=90,port_guards=2,pc_nxdk_cases=len(cases),original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Traversal/release/timer against42ee80 with42ef39 redirected to42f2a2, the explicit missing-owner cycle fix. Live-owner body supplied/skipped; real original release and timer vs compiled shared functions. Exact pool bytes and callback order, head/interior/tail removal and missing emitters. No full geometry/spread/audio/damage body or live campaign integration.')
(root/'artifacts/burn-update.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
