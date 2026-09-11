"""Original burn emitter placement with supplied model attachments and emitters."""
import hashlib,json,math,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
v=lambda xyz:struct.pack('<3f',*xyz)
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;owner=b+0x1000;emitters=[b+0x3000+j*0x200 for j in range(4)];stack=b+0xe000;u.mem_map(b,65536)
trace=[];positions={}
def hook(m,address,size,context):
    if address not in (0x503230,0x4972a0,0x4972f0):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<5I',m.mem_read(sp,20));pop=4;result=0
    if address==0x503230:
        assert a[1]==0x12345678
        trace.append(('attachment',a[4]));m.mem_write(a[2],v(attachments[a[4]]));m.mem_write(a[3],bytes(36))
    elif address==0x4972a0:
        emitter=m.reg_read(UC_X86_REG_ECX);index=emitters.index(emitter);assert a[2]==emitter+0x14
        xyz=struct.unpack('<3f',m.mem_read(a[1],12));trace.append(('position',index));positions[index]=xyz;pop=12
    else:
        index=emitters.index(m.reg_read(UC_X86_REG_ECX));trace.append(('update',index))
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+pop);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x42ef3e);maximum_error=0.;cases=0
for i in range(1024):
    attachments=[struct.unpack('<3f',v([rng.uniform(-10,10) for _ in range(3)])) for j in range(4)]
    if i%17==0:attachments[1]=attachments[0]
    elapsed=(0.,12.,12.000001,17.)[i%4]
    u.mem_write(b,w(*emitters,0xabcdef,0,1,2,3));u.mem_write(b+0x30,struct.pack('<f',elapsed));u.mem_write(owner+0x80,w(0x12345678))
    for reg,value in ((UC_X86_REG_ESP,stack),(UC_X86_REG_ESI,b),(UC_X86_REG_EBX,owner),(UC_X86_REG_FPCW,0x27f)):u.reg_write(reg,value)
    trace=[];positions={};end=0x42f0bd if elapsed<=12 else 0x42f1dc
    u.emu_start(0x42ef3e,end,count=10000);assert u.reg_read(UC_X86_REG_EIP)==end
    want=[('attachment',2),('position',3),('update',3)];expected={3:attachments[2]}
    if elapsed<=12:
        want.extend([('attachment',0),('attachment',1),('attachment',3),('position',0),('update',0),('position',1),('update',1),('position',2),('update',2)])
        expected.update({0:tuple((a+c)*.5 for a,c in zip(attachments[0],attachments[1])),1:attachments[2],2:attachments[3]})
    assert trace==want,(i,trace,want)
    for index,xyz in expected.items():
        for actual,desired in zip(positions[index],xyz):
            if index==0 and attachments[0]==attachments[1]:
                assert math.isnan(actual),(i,index,positions[index]);continue
            error=abs(actual-desired);assert math.isfinite(actual) and error<3e-6,(i,index,positions[index],xyz)
            maximum_error=max(maximum_error,error)
    cases+=1
report=dict(result='PASS',cases=cases,maximum_midpoint_error=maximum_error,original_sha256=digest,scope='Unchanged42ef3e..42f0bd (or age skip42f1dc), original vector subtraction/normalization/distance/scaling helpers. Supplied model attachment vectors and intercepted position/update calls. Fourth emitter follows spine at every age; first3 only through12 seconds; Coincident leg tags produce NaN midpoint components (original zero normalization). Does not execute model evaluation, room relocation4972a0, emission4972f0, timer/spread, or callback mutation.')
(root/'artifacts/burn-attachment-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
