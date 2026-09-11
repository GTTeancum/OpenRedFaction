"""Shared traversal/tail/fade/release chain vs original retirement subsets."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
ev=runpy.run_path(str(root/'tools/verify_burn_retirement_trace.py'));cases=ev['wire_cases'];expected=ev['wire_expected'];w=ev['w']
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;x.mem_map(b,65536)
map_text=(root/'build/xbox/main.map').read_text();entry={name:int(re.search('_rf_burn_'+name+r'\s+([0-9a-fA-F]+)',map_text)[1],16) for name in ('pool_update','owner_tick','fade','release')}
callbacks=[b+0x3000+j*16 for j in range(11)];rows=[]
def enter(m,sp,args,old_count,address,thunk):
    ret=struct.unpack('<I',m.mem_read(sp,4))[0];padding=(len(args)-old_count)*4
    assert padding in (8,16)
    m.mem_write(thunk,b'\x83\xc4'+bytes([padding])+b'\x68'+w(ret)+b'\xc3')
    m.mem_write(sp-padding,w(thunk,*args));m.reg_write(UC_X86_REG_ESP,sp-padding);m.reg_write(UC_X86_REG_EIP,address)
def hook(m,address,size,context):
    if address not in callbacks:return
    kind=callbacks.index(address);sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<4I',m.mem_read(sp,16));result=0
    if kind==0:rows.append(w(0x40a0e0,a[2]));result=1
    elif kind==1:
        rows.append(w(0x42ef3e,a[2]));enter(m,sp,[a[3],b+0x2600,a[2],0x3e000000,b+0x2200],3,entry['owner_tick'],b+0x3400);return
    elif kind in (2,3,4,5):rows.append(w((0x4973d0,0x497d80,0x505a40,0x42ee13)[kind-2],a[2]))
    elif kind==6:rows.append(w(0x5058c0,a[2]))
    elif kind==7:
        rows.append(w(0x42f2f0,a[2]));deadline=struct.unpack('<I',m.mem_read(b+520,4))[0]
        enter(m,sp,[b+(a[2]-1)*64,b+0x2400,a[2],deadline,1000,b+0x2300],2,entry['fade'],b+0x3500);return
    elif kind==8:rows.append(w(0x426fc0,a[2]))
    elif kind==9:enter(m,sp,[b,a[2],0,b+0x2100],2,entry['release'],b+0x3600);return
    else:raise AssertionError('Unexpected callback in inactive-emitter, absent-entity retirement fixture')
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
x.hook_add(UC_HOOK_CODE,hook)
x.mem_write(b+0x2000,w(callbacks[0],callbacks[1],b+0x2100,0))
x.mem_write(b+0x2100,w(*callbacks[2:6],0))
x.mem_write(b+0x2200,w(callbacks[6],callbacks[10],callbacks[10],callbacks[10],callbacks[7],0))
x.mem_write(b+0x2300,w(callbacks[2],callbacks[10],callbacks[8],callbacks[10],callbacks[9],0))
x.mem_write(b+0x2400,w(*[b+0x2500+j*28 for j in range(4)]));x.mem_write(b+0x2600,w(0,1)+bytes(32))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-retirement'],input=b''.join(cases));assert len(actual)==len(cases)*1556
for i,(wire,want) in enumerate(zip(cases,expected)):
    assert actual[i*1556:(i+1)*1556]==want,('PC',i)
    rows=[];x.mem_write(b,wire);x.mem_write(b+0x2500,bytes(112));x.mem_write(stack,w(stop,b,1000,b+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry['pool_update'],stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,524))+w(len(rows))+b''.join(rows)+bytes((128-len(rows))*8)
    assert got==want,('NXDK',i,[(j,a,c) for j,(a,c) in enumerate(zip(got,want)) if a!=c][:12])
report=dict(result='PASS',cases=len(cases),retired_records=ev['retired'],original_sha256=ev['ev']['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Shared pool_update -> owner_tick -> fade -> release on PC and actual NXDK via callback thunks; exact pool/rings/timer/call order versus unmodified original540 retirement patterns. Attachment/spread skipped, inactive emitter views, absent entity/owner links; external resources supplied. No live campaign adapters.')
(root/'artifacts/burn-retirement.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
