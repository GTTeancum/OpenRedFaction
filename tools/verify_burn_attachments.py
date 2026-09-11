"""PC and NXDK burn placement vs original vectors/order, explicit zero-leg guard."""
import hashlib,json,math,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
ev=runpy.run_path(str(root/'tools/verify_burn_attachment_trace.py'));cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_burn_attachments\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];points=b''
def hook(m,address,size,context):
    if address not in (b+0x3000,b+0x3010):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<4I',m.mem_read(sp,16))
    if address==b+0x3000:
        assert a[2]<4;trace.append(w(0,a[2],0,0,0));m.mem_write(a[3],points[a[2]*12:a[2]*12+12])
    else:trace.append(w(1,a[2])+bytes(m.mem_read(a[3],12)))
    m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
x.hook_add(UC_HOOK_CODE,hook)
# Nonfinite age fails before any callbacks.
wire=bytearray(cases[1]);wire[48:52]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+bytes(180))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-attachments'],input=b''.join(cases));assert len(actual)==len(cases)*184
maximum=0.
def compare(got,want,platform,index):
    global maximum
    assert got[:24]==want[:24],(platform,index,'result',got[:24].hex(),want[:24].hex())
    for row in range(8):
        a=got[24+20*row:44+20*row];c=want[24+20*row:44+20*row]
        assert a[:8]==c[:8],(platform,index,row,'order')
        if a[:8]==w(1,1):
            for value,reference in zip(struct.unpack('<3f',a[8:]),struct.unpack('<3f',c[8:])):
                error=abs(value-reference);assert math.isfinite(value) and error<2e-6,(platform,index,value,reference);maximum=max(maximum,error)
        else:assert a[8:]==c[8:],(platform,index,row,'point')
for i,(wire,want) in enumerate(zip(cases,expected)):
    compare(actual[i*184:(i+1)*184],want,'PC',i)
    points=wire[64:];trace=[];x.mem_write(b,wire[:64]);x.mem_write(b+0x2000,w(b+0x3000,b+0x3010,0));x.mem_write(b+0x1000,bytes(16))
    x.mem_write(stack,w(stop,b,b+0x2000,b+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x1000,16))+w(len(trace))+b''.join(trace)+bytes((8-len(trace))*20)
    compare(got,want,'NXDK',i)
report=dict(result='PASS',cases=len(cases),maximum_position_error=maximum,original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original attachment/emitter order and age gate vs shared PC/NXDK; exact non-midpoint fields, midpoint tolerance2e-6. Coincident leg tags explicitly return RF_RANGE after earlier effects instead of original NaN; nonfinite age guard. Supplied model/emitters, no room/emission or live integration.')
(root/'artifacts/burn-attachments.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
