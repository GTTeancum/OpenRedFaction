"""Full465220/464f90 authored clutter loader with supplied resource boundaries."""
import hashlib,json,struct,sys
import pefile
from inspect_clutter_records import ROOT,inspect,sections
sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP

exe=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));image=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(0,4096);u.mem_map(base,0x4000000)
stack,stop=base+0xe000,base+0xf000
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
data=b'';cursor=string_pool=array_pool=0;strings={};arrays={};calls=[];resources=[];posts=[]
rows=[];fail=False;factory_index=0;current_class=b''

def take(n):
    global cursor
    assert cursor+n<=len(data)
    value=data[cursor:cursor+n];cursor+=n;return value
def set_string(address,value):
    global string_pool
    u.mem_write(string_pool,value+b'\0');u.mem_write(address,w(len(value),string_pool))
    strings[string_pool]=value;string_pool+=len(value)+1

def hook(cpu,address,size,context):
    global array_pool,factory_index,current_class
    if address not in (0x4ff3b0,0x4ff470,0x4ff480,0x523970,0x5239c0,0x52c910,
                       0x52c780,0x52cc10,0x52ca00,0x52cac0,0x4bf580,0x45ec40,
                       0x410b60,0x4104a0,0x410d30,0x525c70):return
    sp=cpu.reg_read(UC_X86_REG_ESP);owner=cpu.reg_read(UC_X86_REG_ECX);result=pop=0
    if address==0x4ff3b0:cpu.mem_write(owner,w(0,base));result=owner
    elif address==0x4ff470:pass
    elif address==0x4ff480:result=read(owner+4)
    elif address==0x523970:assert read(sp+4)==1;result=1;pop=4
    elif address==0x5239c0:result=180
    elif address==0x52c910:
        assert read(sp+4)<=180;result,=struct.unpack('<I',take(4));pop=8
    elif address==0x52c780:result=int(take(1)[0]!=0);pop=8
    elif address==0x52cc10:
        assert read(sp+8)<=180
        length,=struct.unpack('<H',take(2));set_string(read(sp+4),take(length));pop=12
    elif address in (0x52ca00,0x52cac0):
        cpu.mem_write(read(sp+4),take(12 if address==0x52ca00 else 36));pop=12
    elif address==0x4bf580:
        assert owner==0x6469c0;cpu.mem_write(owner,w(0,0,0));arrays.pop(owner,None)
    elif address==0x45ec40:
        count=read(owner)
        if owner not in arrays:
            assert count==0;arrays[owner]=array_pool;array_pool+=4096
        assert count<1024 and array_pool<base+0x4000000
        result=arrays[owner]+count*4
        cpu.mem_write(result,w(read(sp+4)));cpu.mem_write(owner,w(count+1,1024,arrays[owner]));pop=4
    elif address==0x410b60:
        current_class=strings[read(sp+4)];result=0x1234
    elif address==0x4104a0:
        cls,name,uid,position,matrix,final=struct.unpack('<6I',cpu.mem_read(sp+4,24))
        assert cls==0x1234 and uid==0xffffffff and final==1
        calls.append((current_class,strings[name],bytes(cpu.mem_read(position,12)),bytes(cpu.mem_read(matrix,36))))
        if not fail:
            result=base+0x100000+factory_index*0x400
            cpu.mem_write(result,b'\xa5'*0x2d8);cpu.mem_write(result+0x2c0,w(0,0,0))
        factory_index+=1
    elif address==0x410d30:
        obj,name=struct.unpack('<2I',cpu.mem_read(sp+4,8))
        resources.append((obj,strings[name],read(obj+0x20)))
        result=0xabc00000+len(resources)
    else:
        assert bytes(cpu.mem_read(sp+4,8))==w(0,0)
        posts.append(factory_index-1)
    cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)

u.hook_add(UC_HOOK_CODE,hook)
section_count=record_count=common_count=link_count=resource_count=0
text=lambda s:struct.pack('<H',len(s))+s
synthetic=w(2)
for i in range(2):
    synthetic+=w(100+i)+text(b'fixture')+struct.pack('<12f',0,1,2,1,0,0,0,1,0,0,0,1)+text(b'name')+bytes([255])
    synthetic+=w(2)+text(b'property')+w(12)+text(b'property')+w(0xffffffff)
    synthetic+=text(b'resource' if i else b'')+w(4,10,11,10,12)
cases=list(sections())+[({'file':'synthetic'},synthetic)]
for level,payload in cases:
    rows=inspect(payload)
    ids=list(dict.fromkeys(v for row in rows for v in row['links']))
    lookup={v:0x900000+i for i,v in enumerate(ids) if i%2==0}
    supplied=list(lookup.items())+[(key,0xdeadbeef) for key in lookup]
    assert len(supplied)<4096
    u.mem_write(0x646098,w(len(supplied),len(supplied),base+0x500000))
    for i,(uid,handle) in enumerate(supplied):
        ptr=base+0x510000+i*0x100
        u.mem_write(base+0x500000+i*4,w(ptr));u.mem_write(ptr,w(uid));u.mem_write(ptr+0x94,w(handle))
    for fail in (False,True):
        data=payload;cursor=0;string_pool=base+0x600000;array_pool=base+0x800000
        strings={base:b''};arrays={};calls=[];resources=[];posts=[];factory_index=0
        u.mem_write(base,b'\0');u.mem_write(0x6469a4,w(0));u.mem_write(0x6469c0,w(0,0,0))
        u.mem_write(stack,w(stop,base+0x200));u.reg_write(UC_X86_REG_ESP,stack)
        u.emu_start(0x465220,stop,count=10000000)
        assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
        assert cursor==len(data) and len(calls)==len(rows),level['file']
        assert calls==[(r['class_name'],r['name'],r['position'],r['matrix']) for r in rows],level['file']
        expected_resources=[]
        for i,row in enumerate(rows):
            if fail:continue
            obj=base+0x100000+i*0x400
            expected=bytearray(b'\xa5'*0x2d8);expected[0x20:0x24]=w(row['uid'])
            links=[lookup[v] for v in row['links'] if v in lookup]
            if links:
                pointer=arrays[obj+0x2c0];assert bytes(u.mem_read(pointer,len(links)*4))==w(*links)
                expected[0x2c0:0x2cc]=w(len(links),1024,pointer)
            else:expected[0x2c0:0x2cc]=w(0,0,0)
            if row['resource_name']:
                expected_resources.append((obj,row['resource_name'],row['uid']))
                expected[0x2bc:0x2c0]=w(0xabc00000+len(expected_resources))
            assert bytes(u.mem_read(obj,len(expected)))==expected,(level['file'],i,'publication')
        assert resources==expected_resources and posts==([] if fail else list(range(len(rows))))
    if level['file']=='synthetic':continue
    section_count+=1;record_count+=len(rows)
    common_count+=sum(len(r['common']) for r in rows);link_count+=sum(len(r['links']) for r in rows)
    resource_count+=sum(bool(r['resource_name']) for r in rows)
report=dict(result='PASS',sections=section_count,records=record_count,common=common_count,
            links=link_count,resources=resource_count,synthetic_cases=1,original_sha256=digest,
            scope='Full465220/464f90 iteration, factory arguments, first matching association linkage, UID/resource publication and post callback. '
            'Actual array lookup and string inequality execute. Supplied version180 I/O, string storage, class lookup, '
            'factory success/failure, array growth, resource and post effects. No shared C owner or real clutter factory/geometry proof.')
(ROOT/'artifacts/clutter-loader.json').write_text(json.dumps(report,indent=2));print(report)
