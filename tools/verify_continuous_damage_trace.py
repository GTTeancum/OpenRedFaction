"""Trace original Continuous_Damage on/off actions at explicit backend boundaries."""
import hashlib,json,struct,sys,itertools,re,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,65536);stack=b+0xe000;stop=b+0xf000;return_float=b+0xf100
u.mem_write(return_float,b'\xd9\xee\xc3') # fldz; ret: damage's ignored float return
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
handles=[0x12340001,0x23450002];objects=[b+0x2000,b+0x4000]
for h,obj in zip(handles,objects):
    u.mem_write(0x7394cc+(h&65535)*4,w(obj));u.mem_write(obj+0x2c,w(h))
    u.mem_write(obj+0x1430,w(b+0x6000));u.mem_write(b+0x60c4,w(0x76543210))
trace=[];predicates=[0,0,0]
def hook(m,address,size,context):
    sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    if address==0x4892c0:
        trace.append(['damage',*struct.unpack('<8I',m.mem_read(sp+4,32))])
        m.reg_write(UC_X86_REG_EIP,return_float);return
    if address==0x40e0b0:trace.append(['feedback',*struct.unpack('<3I',m.mem_read(sp+4,12))]);value=0
    elif address==0x426fc0:value=objects[1]
    else:value=predicates[(0x4290d0,0x427020,0x48acf0).index(address)]
    m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for a in (0x4892c0,0x40e0b0,0x426fc0,0x4290d0,0x427020,0x48acf0):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
xp=pefile.PE(str(root/'build/xbox/main.exe'));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(b,65536)
entry=int(re.search(r'_rf_event_continuous_damage_action\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
native_trace=[]
def native_hook(m,address,size,context):
    sp=m.reg_read(UC_X86_REG_ESP);ret,ctx=struct.unpack('<2I',m.mem_read(sp,8))
    if address==b+0xf200:
        handle,stage,dest=struct.unpack('<3I',m.mem_read(sp+8,12))
        m.mem_write(dest,w(int(handle in handles),handles[1],*predicates,0x76543210))
    elif address==b+0xf210:
        ptr=struct.unpack('<I',m.mem_read(sp+8,4))[0];native_trace.append(['damage',*struct.unpack('<8I',m.mem_read(ptr,32))])
    else:native_trace.append(['feedback',*struct.unpack('<3I',m.mem_read(sp+8,12))])
    m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for address in (b+0xf200,b+0xf210,b+0xf220):x.hook_add(UC_HOOK_CODE,native_hook,begin=address,end=address)
x.mem_write(b+0x1000,w(handles[0],0x99990001,handles[1]));x.mem_write(b+0x2000,w(b+0xf200,b+0xf210,b+0xf220,0))
commands=bytearray();expected=bytearray()
def encoded(trace):
    rows=[]
    for t in trace:rows.append(w(0 if t[0]=='damage' else 1,*t[1:])+bytes((8-len(t[1:]))*4))
    return w(0,len(trace))+b''.join(rows)+bytes((4-len(rows))*36)
cases=damage_calls=feedback_calls=0
for rate,dt,kind,actor_mode in itertools.product((0,1,-1,100000,2147483647,-2147483648),(0.0,1/60,.125),(0,3,5,6,7),range(11)):
    dt=struct.unpack('<f',struct.pack('<f',dt))[0];actor=0xffffffff if actor_mode==0 else handles[1]
    predicates[:]=[2 if actor_mode==5 else 257 if actor_mode==6 else int(actor_mode==2),2 if actor_mode==8 else 257 if actor_mode==9 else int(actor_mode==3),256 if actor_mode==7 else 257 if actor_mode==10 else int(actor_mode==4)]
    event=bytearray(0x2c0);event[0x290:0x294]=w(17);event[0x29c:0x2a8]=w(3,3,b+0x1000)
    event[0x2a8:0x2ac]=w(actor);event[0x2b8:0x2c0]=w(rate,kind)
    u.mem_write(b,bytes(event));u.mem_write(b+0x1000,w(handles[0],0x99990001,handles[1]))
    u.mem_write(0x5a4014,struct.pack('<f',dt));u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b);u.reg_write(UC_X86_REG_FPCW,0x27f)
    trace.clear();u.emu_start(0x4bb4d0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    amount=struct.unpack('<I',struct.pack('<f',10000.0 if rate==0 else rate*dt))[0]
    def damage(target,source):return ['damage',target,amount,source,0xffffffff,kind,0,0xffffffff,1]
    want=[damage(handles[0],0xffffffff),damage(handles[1],0xffffffff)]
    if actor_mode and not any((v&255)==1 for v in predicates[:2]):
        want.append(damage(handles[1],handles[1]))
        if (predicates[2]&255) and kind in (0,3,5,6):want.append(['feedback',0x76543210,0x3c23d70a,0x3f000000])
    assert trace==want,(rate,dt,kind,actor_mode,trace,want)
    assert bytes(u.mem_read(b,len(event)))==event
    for action in (0,1):
        commands.extend(w(rate,struct.unpack('<I',struct.pack('<f',dt))[0],kind,actor_mode,action));expected.extend(encoded(want if action else []))
        x.mem_write(b,w(3,b+0x1000,rate,kind,actor)+struct.pack('<f',dt));x.mem_write(stack,w(stop,b,action,b+0x2000))
        x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);native_trace.clear()
        x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
        assert native_trace==(want if action else []),('NXDK',rate,dt,kind,actor_mode,action,native_trace,want)

    damage_calls+=sum(t[0]=='damage' for t in trace);feedback_calls+=sum(t[0]=='feedback' for t in trace);cases+=1
    trace.clear();u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b)
    u.emu_start(0x4b9f80,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop and not trace
guards=[(1,0x7f800000,1,-2),(1,0x7fc00000,1,-2),(2147483647,0x7f7fffff,1,-2),(1,0x3f800000,2,-4)]
for rate,dt_bits,action,status in guards:
    commands.extend(w(rate,dt_bits,0,0,action));expected.extend(w(status,0)+bytes(144))
    x.mem_write(b,w(3,b+0x1000,rate,0,0xffffffff,dt_bits));x.mem_write(stack,w(stop,b,action,b+0x2000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);native_trace.clear()
    x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==(status&0xffffffff) and not native_trace
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--continuous-damage'],input=commands)
assert actual==expected, next((i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b),('size',len(actual),len(expected)))
report=dict(result='PASS',pc_nxdk_cases=cases*2+len(guards),port_guards=len(guards),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),original_sha256=digest,cases=cases,off_cases=cases,damage_calls=damage_calls,feedback_calls=feedback_calls,scope='Full original4bb4d0 and type17 off4b9f80; real array and generation-checked object lookup. Entity lookup and three predicates supplied. Damage4892c0 and feedback40e0b0 intercepted; ignored float return provided. Signed rates, binary32 frame duration, zero-rate10000, linked stale handles, actor suppression, duplicate linked/actor dispatch and feedback kind selection. Exact PC/NXDK request equivalence with stable event/links and supplied lookup facts. No actual damage, health/death or time scheduling.')
(root/'artifacts/continuous-damage-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
