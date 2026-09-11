"""Shared burn release/init vs original, with pointer links normalized to slots."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
ev=runpy.run_path(str(root/'tools/verify_burn_release_trace.py'));cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536);mapping=(root/'build/xbox/main.map').read_text()
entries=[int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ('rf_burn_release','rf_burn_pool_initialize')]
trace=[]
def hook(m,address,size,context):
    if address not in (b+0x3000,b+0x3010,b+0x3020,b+0x3030):return
    sp=m.reg_read(UC_X86_REG_ESP);ret,ctx,value=struct.unpack('<3I',m.mem_read(sp,12));kind=(address-b-0x3000)//16
    if kind<3:trace.append(w((0x4973d0,0x497d80,0x505a40)[kind],value))
    else:
        refs=list(struct.unpack('<4I',m.mem_read(b+0x1000,16)))
        if value in refs:refs[refs.index(value)]=0;m.mem_write(b+0x1000,w(*refs))
    m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook)
# Invalid token and broken/overlapping active/free rings fail before callbacks.
for at,value,status in [(540,0,-4),(540,9,-4),(56,9,-2),(512,1,-2)]:
    wire=bytearray(cases[0]);wire[at:at+4]=w(value);cases.append(bytes(wire));expected.append(w(status)+wire[:540]+bytes(580))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-pool'],input=b''.join(cases));assert len(actual)==len(cases)*1124
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*1124:(i+1)*1124]==want,('PC',i,actual[i*1124:(i+1)*1124].hex(),want.hex())
    token,mode,initialize=struct.unpack('<3I',wire[540:]);trace=[];x.mem_write(b,wire[:524]);x.mem_write(b+0x1000,wire[524:540]);x.mem_write(b+0x2000,w(b+0x3000,b+0x3010,b+0x3020,b+0x3030,0))
    args=w(stop,b,b+0x2000) if initialize else w(stop,b,token,mode,b+0x2000)
    x.mem_write(stack,args);x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[bool(initialize)],stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,524))+bytes(x.mem_read(b+0x1000,16))+w(len(trace))+b''.join(trace)+bytes((72-len(trace))*8)
    assert got==want,('NXDK',i,got.hex(),want.hex())
report=dict(result='PASS',original_release_cases=432,original_initialization_cases=1,port_guards=4,pc_nxdk_cases=len(cases),original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Exact initialized/released payloads, rings, deadline, owner references and external call order against original42ed20/42e8a0. Pointer links normalized to1..8 tokens. Owner first-match adapter supplied; callback state stable. No live particles/audio, burn creation or per-frame update.')
(root/'artifacts/burn-pool.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
