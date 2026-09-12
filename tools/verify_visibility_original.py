"""Full original4991c0 traversal/query orchestration with model/world boundaries.

Actual40a110,508b70, constructors and vector/matrix helpers execute. Supplied
5031f0 model effects exercise query restoration and middle-list clipping; this
does not prove model intersection or world visibility implementations.
"""
import hashlib
import json
import math
import random
import re
import struct
import subprocess
import sys
from pathlib import Path
import pefile

root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW

exe=root/'Installed_Game/RF.exe'
digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));image=p.get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536)
stack,stop=base+0xe000,base+0xf000
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
addresses=[base+0x1000+i*0x400 for i in range(9)]
sentinels=(0x5cb060,0x5c9360,0x5cabb8)
heads=(0x5cb2ec,0x5c95ec,0x5cae44)
rows=[];wanted=[];observed=[];flags=world_result=0
query_start=[0,0,0];query_delta=[0,0,0];query_flags=0
model_calls=world_calls=restores=clips=0
shared_failure=0
shared='--shared' in sys.argv
commands=[];answers=[]
if shared:
    xp=pefile.PE(str(root/'build/xbox/main.exe'));xi=xp.get_memory_mapped_image()
    x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xp.OPTIONAL_HEADER.ImageBase,(len(xi)+4095)//4096*4096)
    x.mem_write(xp.OPTIONAL_HEADER.ImageBase,xi);x.mem_map(base,65536)
    entry=int(re.search(r'\s_rf_collision_visibility\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)


def hook(cpu,address,size,context):
    global model_calls,world_calls
    if address not in (0x5031f0,0x498e80,base+0xf100,base+0xf200):return
    sp=cpu.reg_read(UC_X86_REG_ESP)
    shared_call=address>=base
    if address in (0x5031f0,base+0xf100):
        if shared_call:
            context,obj,query,hit,reset,out=struct.unpack('<6I',cpu.mem_read(sp+4,24))
            assert context==0
            index=struct.unpack('<I',cpu.mem_read(obj,4))[0]-1
        else:
            model,query,hit,reset=struct.unpack('<4I',cpu.mem_read(sp+4,16))
            index=model-100
        row=rows[index]
        assert reset==1
        observed.append(('model',index,bytes(cpu.mem_read(query,80))))
        cpu.mem_write(hit,f(row['time'])+b'\x5a'*28)
        cpu.mem_write(query+48,f(*row['new_start'],*row['new_delta']))
        cpu.mem_write(query+76,w(row['new_flags']))
        result=row['result'];model_calls+=not shared_call
    else:
        if shared_call:
            context,*args,out=struct.unpack('<6I',cpu.mem_read(sp+4,24))
            assert context==0
            args=tuple(args)
        else:args=struct.unpack('<4I',cpu.mem_read(sp+4,16))
        assert args==(base,base+12,flags,0x12345678)
        observed.append(('world',));result=world_result;world_calls+=not shared_call
    if shared_call:
        cpu.mem_write(out,w(result))
        result=0xffffffff if shared_failure==address else 0
    cpu.reg_write(UC_X86_REG_EAX,result)
    cpu.reg_write(UC_X86_REG_EIP,struct.unpack('<I',cpu.mem_read(sp,4))[0]);cpu.reg_write(UC_X86_REG_ESP,sp+4)


u.hook_add(UC_HOOK_CODE,hook)
if shared:x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x4991c0);early=0;nonboolean=0;nan_rejections=0
for case in range(4096):
    start=list(struct.unpack('<3f',f(*[rng.uniform(-4,0) for _ in range(3)])))
    end=list(struct.unpack('<3f',f(*[rng.uniform(0,4) for _ in range(3)])))
    delta=list(struct.unpack('<3f',f(*[b-a for a,b in zip(start,end)])))
    u.mem_write(base,f(*start,*end))
    flags=rng.choice((0,1,2,3,256,257,0x80000001))
    threshold=rng.choice((0,2.5,5,math.nan,math.inf,-math.inf))
    exclude=rng.sample([0]+addresses,2)
    world_result=rng.choice((0,1,2,0x100,0xabcdef80))
    rows=[]
    for index,address in enumerate(addresses):
        family=index//3
        size=rng.choice((0,2.5,4,8,math.nan,math.inf,-math.inf))
        object_flags=rng.choice((0,0,2,0x4000,0x4002,0x100))
        intersects=rng.randrange(4)!=0
        row=dict(size=size,flags=object_flags,intersects=intersects,
                 result=rng.choice((0,0,1,2,0x100,0xabcdef80)),
                 time=struct.unpack('<f',f(rng.choice((0,.25,.5,1,rng.random()))))[0],new_flags=rng.choice((0,1,2,3,0x40,0x42)),
                 new_start=[index+1,2,3],new_delta=[3,2,index+1],
                 position=[index,2*index,-index],matrix=[1,0,0,0,1,0,0,0,1])
        rows.append(row)
        raw=bytearray(b'\xa5'*0x300)
        raw[0x7c:0x84]=w(object_flags,100+index)
        raw[0xe4:0xf0]=f(*row['position']);raw[0xfc:0x120]=f(*row['matrix'])
        raw[0x180:0x184]=f(size)
        raw[0x190:0x1a8]=f(*([-10]*3+[10]*3 if intersects else [20]*3+[30]*3))
        raw[0x28c:0x290]=w(addresses[index+1] if index%3!=2 else sentinels[family])
        u.mem_write(address,bytes(raw))
    ordered=[];orders=[]
    for family,head in enumerate(heads):
        order=list(range(family*3,family*3+3));rng.shuffle(order)
        order=order[:rng.randrange(4)] if case%17!=0 else []
        orders.append(order)
        ordered.extend(order)
        u.mem_write(head,w(addresses[order[0]] if order else sentinels[family]))
        for j,index in enumerate(order):
            u.mem_write(addresses[index]+0x28c,w(addresses[order[j+1]] if j+1<len(order) else sentinels[family]))
    before=[bytes(u.mem_read(address,0x300)) for address in addresses]
    wanted=[];result=0;query_start=start[:];query_delta=delta[:];query_flags=flags&1
    if ordered:
        for index in ordered:
            row=rows[index]
            if not row['size']>=threshold:
                nan_rejections+=math.isnan(row['size']) or math.isnan(threshold)
                continue
            if row['flags']&0x4000 or (index//3!=1 and row['flags']&2):continue
            if addresses[index] in exclude or not row['intersects']:continue
            query=f(*row['position'],*row['matrix'],*query_start,*query_delta,0)+w(query_flags)
            assert len(query)==80
            wanted.append(('model',index,query))
            result|=row['result']&255
            query_start=row['new_start'][:];query_delta=row['new_delta'][:];query_flags=row['new_flags']
            if index//3==1:
                query_delta=list(struct.unpack('<3f',f(*[v*row['time'] for v in delta])));clips+=1
            if result and flags&1:
                result=1;early+=1
                break
            if query_flags&2:
                query_start=start[:];query_delta=delta[:];query_flags&=~2;restores+=1
        else:
            wanted.append(('world',));result|=world_result&255
    else:
        wanted.append(('world',));result=world_result&255
    nonboolean+=result not in (0,1)
    observed=[]
    u.mem_write(stack-0x400,b'\xa5'*0x400)
    u.mem_write(stack,w(stop,base,base+12,struct.unpack('<I',f(threshold))[0],flags,*exclude,0x12345678))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x4991c0,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
    assert u.reg_read(UC_X86_REG_EAX)&255==result,(case,'return',u.reg_read(UC_X86_REG_EAX)&255,result)
    assert observed==wanted,(case,observed,wanted)
    assert [bytes(u.mem_read(address,0x300)) for address in addresses]==before
    assert bytes(u.mem_read(base,24))==f(*start,*end)
    if shared:
        exclusions=[0 if a==0 else addresses.index(a)+1 for a in exclude]
        wire=w(flags)+f(threshold)+w(*exclusions,world_result)+f(*start,*end)
        wire+=w(*[len(order) for order in orders])+b''.join(w(*(order+[0]*(3-len(order)))) for order in orders)
        objects=[]
        for index,row in enumerate(rows):
            obj=w(index+1,row['flags'])+f(row['size'])+w(100+index)
            obj+=f(*row['position'],*row['matrix'])+before[index][0x190:0x1a8]
            assert len(obj)==88;objects.append(obj)
            wire+=obj+w(row['result'])+f(row['time'])+w(row['new_flags'])+f(*row['new_start'],*row['new_delta'])
        assert len(wire)==1208;commands.append(wire)
        output=w(0,result,len(wanted))
        for row in wanted:output+=w(1,row[1])+row[2] if row[0]=='model' else w(2,0)+bytes(80)
        output+=bytes((10-len(wanted))*88);assert len(output)==892;answers.append(output)
        x.mem_write(base,f(*start,*end));x.mem_write(base+0x6100,w(0xa5a5a5a5))
        for family,order in enumerate(orders):
            ptr=base+0x4000+family*0x400
            x.mem_write(ptr,b''.join(objects[index] for index in order) or b'\0')
            x.mem_write(base+0x3000+family*8,w(ptr,len(order)))
        x.mem_write(base+0x6000,w(base+0xf100,base+0xf200,0))
        x.mem_write(stack,w(stop,base+0x3000,base,base+12,struct.unpack('<I',f(threshold))[0],flags,
                            *exclusions,0x12345678,base+0x6000,base+0x6100))
        x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);observed=[]
        x.emu_start(entry,stop,count=100000)
        assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
        assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(base+0x6100,4))==w(result),(case,'NXDK result')
        assert observed==wanted,(case,'NXDK trace',observed,wanted)
if shared:
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--visibility'],input=b''.join(commands))
    assert len(actual)==len(answers)*892
    for i,want in enumerate(answers):assert actual[i*892:(i+1)*892]==want,(i,'PC mismatch')
    # Port-specific rejection/cleanup behavior, not original behavior claims.
    x.mem_write(base,f(0,0,0,1,1,1));flags=0;world_result=0
    obj=w(1,0)+f(5)+w(100)+f(0,0,0,1,0,0,0,1,0,0,0,1,-10,-10,-10,10,10,10)
    x.mem_write(base+0x4000,obj)
    x.mem_write(base+0x3000,w(base+0x4000,1,0,0,0,0))
    args=[base+0x3000,base,base+12,0,0,0,0,0x12345678,base+0x6000,base+0x6100]
    rows=[dict(time=.5,new_start=[1,2,3],new_delta=[4,5,6],new_flags=2,result=1)]
    def guarded(arguments,status,expected_kinds):
        global observed
        observed=[];x.mem_write(base+0x6100,w(0xa5a5a5a5))
        x.mem_write(stack,w(stop,*arguments));x.reg_write(UC_X86_REG_ESP,stack)
        x.emu_start(entry,stop,count=100000)
        assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
        assert x.reg_read(UC_X86_REG_EAX)==status&0xffffffff
        assert bytes(x.mem_read(base+0x6100,4))==w(0xa5a5a5a5)
        assert [r[0] for r in observed]==expected_kinds
    bad=args[:];bad[0]=0;guarded(bad,-4,[])
    bad=args[:];bad[8]=0;guarded(bad,-4,[])
    x.mem_write(base+0x3000,w(0,1));guarded(args,-4,[])
    x.mem_write(base+0x3000,w(base+0x4000,1))
    x.mem_write(base,f(math.nan));guarded(args,-2,[]);x.mem_write(base,f(0))
    x.mem_write(base+0x4000+64,f(20));guarded(args,-2,[])
    x.mem_write(base+0x4000,obj)
    shared_failure=base+0xf100;guarded(args,-1,['model'])
    shared_failure=base+0xf200;guarded(args,-1,['model','world'])
    shared_failure=0
report=dict(result='PASS',cases=4096,model_calls=model_calls,world_calls=world_calls,
            early_returns=early,query_restores=restores,middle_list_clips=clips,
            nonboolean_results=nonboolean,nan_size_rejections=nan_rejections,original_sha256=digest,
            scope='Full original4991c0 with actual constructors, flag predicate, AABB and vector/matrix helpers. '
            'Only5031f0 model effects and498e80 world results supplied. Ordered queries, initialized80-byte '
            'query prefix, flag restoration, middle-list clipping and low-byte result verified. '
            'No shared C implementation, live collection ownership, model/world geometry or native Xbox proof.')
if shared:
    report['nxdk_error_cases']=7
    report['scope']='Full original4991c0 versus shared PC/compiled NXDK; real AABB and orchestration, supplied model/world effects. Exact traces and low-byte results. No live ownership or native Xbox proof.'
(root/('artifacts/visibility-shared.json' if shared else 'artifacts/visibility-original.json')).write_text(json.dumps(report,indent=2))
print(report)
