"""Complete shared burn body vs original, with live callback state changes."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
ev=runpy.run_path(str(root/'tools/verify_burn_body_trace.py'));cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;owner=b+0x100;basis=b+0x200;nodes=[b+0x400+j*64 for j in range(3)];ctx=b+0x800;be=b+0x900;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
entry=int(re.search(r'_rf_burn_body\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
callbacks=[b+0x3000+j*16 for j in range(9)];x.mem_write(b+0x3100,b'\xd9\x05'+w(b+0x3200)+b'\xc3');x.mem_write(b+0x3200,struct.pack('<f',6.5))
rows=[];points=b'';mode=dead=0
ids=(0x429990,0x427020,0x40a110,0x4290d0)
def hook(m,address,size,context):
    global dead
    if address not in callbacks:return
    kind=callbacks.index(address);sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<7I',m.mem_read(sp,28));result=0
    if kind==0:
        rows.append(w(0x503230,a[2]).ljust(36,b'\0'));m.mem_write(a[3],points[a[2]*12:a[2]*12+12])
    elif kind==1:
        rows.append((w(0x4972a0,a[2])+bytes(m.mem_read(a[3],12))).ljust(36,b'\0'));rows.append(w(0x4972f0,a[2]).ljust(36,b'\0'))
    elif kind==2:
        index=nodes.index(a[3]);rows.append(w(ids[a[2]],index).ljust(36,b'\0'));result=dead if a[2]==1 and index==2 else 0
    elif kind==3:
        rows.append(w(0x504e40,*a[2:4]).ljust(36,b'\0'));m.reg_write(UC_X86_REG_EIP,b+0x3100);return
    elif kind==4:
        index=nodes.index(a[2]);handle=index*0x1111;rows.append(w(0x4892c0,handle,a[3],a[4],a[5],4,0,a[6],0))
        if index==1 and mode==1:dead=1
        if index==1 and mode==2:m.mem_write(b+44,b'\1')
    elif kind==5:rows.append(w(0x5058c0,a[2])+bytes(m.mem_read(a[3],12))+bytes(m.mem_read(a[4],12))+w(a[5]))
    elif kind==6:rows.append(w(0x4892c0,a[2],a[3],-1,-1,4,0,-1,0))
    elif kind==7:rows.append(w(0x57312d).ljust(36,b'\0'));result=19
    else:rows.append(w(0x42f2f0,a[2]).ljust(36,b'\0'))
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
x.hook_add(UC_HOOK_CODE,hook)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-body'],input=b''.join(cases));assert len(actual)==len(cases)*1276
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*1276:(i+1)*1276]==want,('PC',i,[(j,a,c) for j,(a,c) in enumerate(zip(actual[i*1276:(i+1)*1276],want)) if a!=c][:10])
    mode=struct.unpack('<I',wire[144:148])[0];points=wire[172:];dead=0;rows=[]
    x.mem_write(b,wire[:64]);x.mem_write(owner,wire[64:104]);x.mem_write(basis,wire[104:140]);x.mem_write(b+0x600,wire[140:144]);x.mem_write(b+0x604,w(nodes[0]))
    for j,node in enumerate(nodes):
        x.mem_write(node,w(nodes[j+1] if j<2 else 0)+bytes(24))
        if j:x.mem_write(node+4,wire[148+(j-1)*12:160+(j-1)*12]+struct.pack('<f',100)+w(0,j*0x1111))
    x.mem_write(ctx,w(owner,basis,nodes[0],b+0x604,b+0x600,1000,ev['bits'](.125),0x4321,0xabcdef01,3))
    x.mem_write(be,w(callbacks[0],callbacks[1],0,callbacks[2],callbacks[3],callbacks[4],0,callbacks[5],callbacks[3],callbacks[6],callbacks[7],callbacks[8],0))
    x.mem_write(stack,w(stop,b,1,ctx,be));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,64))+bytes(x.mem_read(owner,40))+b''.join(bytes(x.mem_read(node+20,4)) for node in nodes[1:])+bytes(x.mem_read(b+0x600,4))+w(len(rows))+b''.join(rows)+bytes((32-len(rows))*36)
    assert got==want,('NXDK',i,[(j,a,c) for j,(a,c) in enumerate(zip(got,want)) if a!=c][:10])
report=dict(result='PASS',cases=len(cases),original_sha256=ev['ev']['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete shared body vs original54 age/timer/rotation/two-target mutation scenarios on PC and actual NXDK; exact record/owner/flags/deadline and all ordered normalized callbacks. Model/particle/audio/damage/fade supplied, no pool release or native campaign integration.')
(root/'artifacts/burn-body.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
