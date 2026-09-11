"""Actual original emitter stopping/fade writes vs real shared emitter fields."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
ev=runpy.run_path(str(root/'tools/verify_burn_fade_trace.py'),init_globals={'actual_stop':True})
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=ev['w'];b=0x30000000;slots=b+0x1000;pool=b+0x2000;backend=b+0x2100;flags=b+0x2200;stack=b+0xe000;stop=b+0xf000
p=pefile.PE(str(root/'build/xbox/main.exe'));raw=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(raw)+4095)//4096*4096);x.mem_write(ib,raw);x.mem_map(b,65536)
entry=int(re.search(r'_rf_burn_fade_resolved\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];present=0
def hook(m,address,size,context):
    if address not in [b+0x3000+i*16 for i in range(4)]:return
    kind=(address-b-0x3000)//16;sp=m.reg_read(UC_X86_REG_ESP);ret,ctx,arg=struct.unpack('<3I',m.mem_read(sp,12));result=0
    if kind==0:trace.append(w(0x4174c0,arg,0));result=flags
    elif kind==1:trace.append(w(0x426fc0,arg,0));result=present
    elif kind==2:trace.append(w(0x407ee0,0x300042a0,0))
    else:trace.append(w(0x42ed20,b,0))
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook)
def runtime(view):
    raw=bytearray([0xa5]*184)
    # Original24/28/30/34/44/48 become compact20/24/2c/30/40/44.
    for i,offset in enumerate((32,36,44,48,64,68)):raw[offset:offset+4]=view[i*4:i*4+4]
    raw[131]=view[24];raw[172]=view[25]
    return bytes(raw)
expected=[]
for wire,want in zip(ev['wire_cases'],ev['wire_expected']):
    record=w(1,2,3,4)+want[20:68]
    count=struct.unpack('<I',want[184:188])[0]
    rows=[want[188+i*12:200+i*12] for i in range(count) if struct.unpack('<I',want[188+i*12:192+i*12])[0]!=0x4973d0]
    result=want[:4]+record+b''.join(runtime(want[68+i*28:96+i*28]) for i in range(4))+want[180:184]+w(len(rows))+b''.join(rows)+bytes((8-len(rows))*12)
    expected.append(result)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-fade-resolved'],input=b''.join(ev['wire_cases']))
assert actual==b''.join(expected),('PC',[(i,a,c) for i,(a,c) in enumerate(zip(actual,b''.join(expected))) if a!=c][:20])
for case,(wire,want) in enumerate(zip(ev['wire_cases'],expected)):
    x.mem_write(b,w(1,2,3,4)+wire[16:64]);x.mem_write(pool,w(slots));x.mem_write(flags,wire[176:180])
    for i in range(4):x.mem_write(slots+i*228,runtime(wire[64+i*28:92+i*28])+bytes(40)+w(1))
    x.mem_write(backend,w(*[b+0x3000+i*16 for i in range(4)],0))
    deadline,now,present=struct.unpack('<3I',wire[180:]);trace=[]
    x.mem_write(stack,w(stop,b,pool,1,deadline,now,backend));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,64))+b''.join(bytes(x.mem_read(slots+i*228,184)) for i in range(4))+bytes(x.mem_read(flags,4))+w(len(trace))+b''.join(trace)+bytes((8-len(trace))*12)
    assert got==want,('NXDK',case,[(i,a,c) for i,(a,c) in enumerate(zip(got,want)) if a!=c][:20])
report=dict(result='PASS',cases=len(expected),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original42f2f0 and actual4973d0 vs PC/NXDK real emitter fade. Exact full compact runtimes, including unaffected bytes, enable high bytes, packed color RGB and alpha, record, type7 flags and owner callback order. Owner lookup/reaction/release supplied; no actual resource release or live gameplay.')
(root/'artifacts/burn-fade-resolved.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
