"""Authored clutter ownership on PC/compiled NXDK versus original-audited records."""
import json,re,struct,subprocess,sys
import pefile
from inspect_clutter_records import ROOT,inspect,sections
sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP

w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));image=p.get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(image)+4095)//4096*4096)
u.mem_write(p.OPTIONAL_HEADER.ImageBase,image);base=0x30000000;u.mem_map(base,0x800000)
level,owner,stack,stop,allocation=base+0x1000,base+0x4000,base+0xe000,base+0xf000,base+0x100000
symbols=(ROOT/'build/xbox/main.map').read_text()
symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
entry,close,link=map(symbol,('rf_level_owned_clutter_open','rf_level_owned_clutter_close','rf_level_clutter_link'))
read_entry,malloc,free=map(symbol,('rf_vpp_read','malloc','free'))
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
data=b'';live=False;fail_allocation=False;fail_read_at=read_calls=0

def hook(cpu,address,size,context):
    global live,read_calls
    if address not in (read_entry,malloc,free):return
    sp=cpu.reg_read(UC_X86_REG_ESP);result=0
    if address==read_entry:
        archive,entry_pointer,offset,destination,length=struct.unpack('<5I',cpu.mem_read(sp+4,20))
        assert archive==base and entry_pointer==level+4 and offset>=8
        offset-=8;assert offset+length<=len(data);read_calls+=1
        if read_calls==fail_read_at:result=0xffffffff
        elif length:cpu.mem_write(destination,data[offset:offset+length])
    elif address==malloc:
        assert not live;size=read(sp+4);assert size<0x100000
        if not fail_allocation:
            cpu.mem_write(allocation,b'\xa5'*size);result=allocation;live=True
    elif read(sp+4):
        assert live and read(sp+4)==allocation;live=False
    cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook)

def call(address,*args):
    global read_calls
    read_calls=0;u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(address,stop,count=10000000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
    return u.reg_read(UC_X86_REG_EAX)

def expected_wire(records,payload):
    wire=w(len(records))
    for r in records:
        common_offset=61+len(r['class_name'])+len(r['name'])
        wire+=w(r['uid'],r['enabled'],r['bytes'],common_offset,len(r['common']),r['bytes']-len(r['links'])*4,len(r['links']))
        wire+=r['position']+r['matrix']
        for key in ('class_name','name','resource_name'):wire+=w(len(r[key]))+r[key]
        wire+=payload[r['offset']:r['offset']+r['bytes']]
    return wire

def setup(payload):
    global data
    data=payload;u.mem_write(level,b'\0'*2184);u.mem_write(level,w(base))
    u.mem_write(level+72,w(len(data)+8,180,0,1));u.mem_write(level+600,w(0x50000,0,len(data)))
    u.mem_write(owner,b'\0'*16)

def verify(payload,label):
    global fail_allocation,fail_read_at
    setup(payload);records=inspect(payload)
    assert call(entry,level,0x100000,owner)==0,label
    success_reads=read_calls
    storage,items,count,budget=struct.unpack('<4I',u.mem_read(owner,16))
    assert count==len(records)
    name_bytes=sum(sum(len(r[k])+1 for k in ('class_name','name','resource_name')) for r in records)
    assert budget==16+92*count+(len(data) if count else 0)+name_bytes
    wire=w(count)
    for i,r in enumerate(records):
        node=items+i*92;uid=read(node);enabled=read(node+64)
        raw,size,common_offset,common_count,links_offset,links_count=struct.unpack('<6I',u.mem_read(node+68,24))
        wire+=w(uid,enabled,size,common_offset,common_count,links_offset,links_count)+bytes(u.mem_read(node+4,48))
        for j,key in enumerate(('class_name','name','resource_name')):
            length=len(r[key]);ptr=read(node+52+j*4)
            assert bytes(u.mem_read(ptr+length,1))==b'\0'
            wire+=w(length)+bytes(u.mem_read(ptr,length))
        wire+=bytes(u.mem_read(raw,size))
        for j,value in enumerate(r['links']):
            assert call(link,node,j,base+0x5000)==0 and read(base+0x5000)==value
        u.mem_write(base+0x5000,w(0xa5a5a5a5))
        assert call(link,node,links_count,base+0x5000)==0xfffffffc and read(base+0x5000)==0xa5a5a5a5
    assert wire==expected_wire(records,payload),label
    before=bytes(u.mem_read(owner,16))
    assert call(entry,level,budget,owner)==0xfffffffc and bytes(u.mem_read(owner,16))==before
    call(close,owner);call(close,owner);assert not live and bytes(u.mem_read(owner,16))==bytes(16)
    assert call(entry,level,budget-1,owner)==0xfffffffc
    if count:
        fail_allocation=True;assert call(entry,level,budget,owner)==0xfffffffc;fail_allocation=False
        assert not live and bytes(u.mem_read(owner,16))==bytes(16)
        fail_read_at=success_reads-1
        assert call(entry,level,budget,owner)==0xffffffff;fail_read_at=0
        assert not live and bytes(u.mem_read(owner,16))==bytes(16)
    assert call(entry,level,budget,owner)==0;call(close,owner)
    u.mem_write(level+608,w(len(data)-1))
    assert call(entry,level,0x100000,owner)==0xfffffffe
    assert not live and bytes(u.mem_read(owner,16))==bytes(16)
    return budget

count=total=maximum=opening=0
for level_info,payload in sections():
    records=inspect(payload)
    pc=subprocess.run([str(ROOT/'build/pc/Release/rf_level_entity_probe.exe'),
        str(ROOT/'Installed_Game'/level_info['archive']),level_info['file'],'--clutter'],capture_output=True,check=True)
    assert pc.stdout==expected_wire(records,payload),level_info['file']
    budget=verify(payload,level_info['file']);assert int(pc.stderr)==budget
    count+=1;total+=len(records);maximum=max(maximum,budget)
    if level_info['file'].lower()=='l1s1.rfl':opening=budget
text=lambda s:struct.pack('<H',len(s))+s
def synthetic(name):
    return w(1,99)+text(name)+struct.pack('<12f',0,1,2,1,0,0,0,1,0,0,0,1)+text(b'')+b'\xff'+w(2)+text(b'key')+w(1)+text(b'key')+w(2)+text(b'resource')+w(3,5,6,5)
verify(synthetic(b'long'+b'x'*1024),'common pairs/long name/duplicate links')
verify(w(0),'empty')
setup(synthetic(b'embedded\0null'))
assert call(entry,level,0x100000,owner)==0xfffffffe
assert not live and bytes(u.mem_read(owner,16))==bytes(16)
report=dict(result='PASS',sections=count,records=total,opening_bytes=opening,maximum_bytes=maximum,nxdk_synthetic_cases=3,
    scope='Shared PC/compiled NXDK owned fields, terminated names, complete raw spans, link accessor, budgets, '
    'archive-independent PC export, truncation, repeat close and NXDK allocation/I/O failure. '
    'NXDK VPP/malloc/free supplied. No scene retention, factories or native XEMU clutter proof.')
(ROOT/'artifacts/clutter-owned.json').write_text(json.dumps(report,indent=2));print(report)
