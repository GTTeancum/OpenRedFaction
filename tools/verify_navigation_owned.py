"""All authored navigation owners on PC and compiled NXDK versus audited records.

NXDK executes loader/read bounds/ownership with supplied rf_vpp_read, calloc
and free. Native emulator startup and live navigation remain separate gates.
"""
import json
import re
import struct
import subprocess
import sys
import pefile
from inspect_navigation_records import ROOT, inspect, sections

sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

w = lambda *v: struct.pack('<%dI' % len(v), *(x & 0xffffffff for x in v))
pe = pefile.PE(str(ROOT / 'build/xbox/main.exe'))
image = pe.get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(pe.OPTIONAL_HEADER.ImageBase, (len(image)+4095)//4096*4096)
u.mem_write(pe.OPTIONAL_HEADER.ImageBase, image)
base = 0x30000000
u.mem_map(base, 0x800000)
level, owner, stack, stop, allocation = base+0x1000, base+0x4000, base+0xe000, base+0xf000, base+0x100000
symbols = (ROOT / 'build/xbox/main.map').read_text()
symbol = lambda name: int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)', symbols)[1],16)
entry, close = symbol('rf_level_owned_navigation_open'), symbol('rf_level_owned_navigation_close')
read_entry, calloc, free = symbol('rf_vpp_read'), symbol('calloc'), symbol('free')
read = lambda address: struct.unpack('<I',u.mem_read(address,4))[0]
data = b''
live = False
fail_allocation = False
allocation_bytes = 0
read_calls = fail_read_at = 0


def hook(cpu,address,size,context):
    global live, allocation_bytes, read_calls
    if address not in (read_entry,calloc,free):return
    sp=cpu.reg_read(UC_X86_REG_ESP); result=0
    if address==read_entry:
        archive,entry_pointer,offset,destination,length=struct.unpack('<5I',cpu.mem_read(sp+4,20))
        assert archive==base and entry_pointer==level+4 and offset>=8
        offset-=8
        assert offset+length<=len(data)
        read_calls+=1
        if read_calls==fail_read_at:result=0xffffffff
        else:cpu.mem_write(destination,data[offset:offset+length])
    elif address==calloc:
        assert not live
        count,size=struct.unpack('<2I',cpu.mem_read(sp+4,8))
        allocation_bytes=count*size
        assert allocation_bytes<0x100000
        if not fail_allocation:
            cpu.mem_write(allocation,b'\0'*allocation_bytes)
            result=allocation;live=True
    elif read(sp+4):
        assert live and read(sp+4)==allocation
        live=False
    cpu.reg_write(UC_X86_REG_EAX,result)
    cpu.reg_write(UC_X86_REG_EIP,read(sp))
    cpu.reg_write(UC_X86_REG_ESP,sp+4)


u.hook_add(UC_HOOK_CODE,hook)


def call(address,*args):
    global read_calls
    read_calls=0
    u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(address,stop,count=10000000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    assert u.reg_read(UC_X86_REG_ESP)==stack+4
    return u.reg_read(UC_X86_REG_EAX)


def expected_wire(nodes):
    wire=w(len(nodes))
    for i,n in enumerate(nodes):
        candidate=bytearray(68)
        candidate[:12]=n['position']
        candidate[24:40]=n['radius']*2+n['height']+n['word_024']
        candidate[64:68]=w(n['word_040'])
        wire+=w(n['uid'],n['oriented'],len(n['tags']),len(n['neighbors']),i)
        wire+=candidate+(n['orientation'] or b'\0'*36)+w(*n['tags'])+w(*n['neighbors'])
    return wire


def verify_nxdk(payload,label):
    global data,fail_allocation,fail_read_at
    data=payload;nodes=inspect(data)
    u.mem_write(level,b'\0'*2184)
    u.mem_write(level,w(base))
    u.mem_write(level+72,w(len(data)+8))
    u.mem_write(level+76,w(180,0,1));u.mem_write(level+600,w(0x20000,0,len(data)))
    u.mem_write(owner,b'\0'*20)
    status=call(entry,level,0x100000,owner)
    assert status==0,(label,hex(status))
    successful_reads=read_calls
    storage,node_pointer,refs,count,budget=struct.unpack('<5I',u.mem_read(owner,20))
    assert count==len(nodes)
    words=sum(len(n['tags'])+len(n['raw_neighbors']) for n in nodes)
    assert budget==20+136*count+4*words
    wire=w(count)
    for i in range(count):
        addr=node_pointer+i*120
        ref,key,neighbors,neighbor_count=struct.unpack('<4I',u.mem_read(refs+i*16,16))
        assert ref==addr
        uid,oriented=struct.unpack('<2I',u.mem_read(addr+68,8))
        tags,tag_count=struct.unpack('<2I',u.mem_read(addr+112,8))
        wire+=w(uid,oriented,tag_count,neighbor_count,key)+bytes(u.mem_read(addr,68))+bytes(u.mem_read(addr+76,36))
        if tag_count:wire+=bytes(u.mem_read(tags,4*tag_count))
        if neighbor_count:wire+=bytes(u.mem_read(neighbors,4*neighbor_count))
    assert wire==expected_wire(nodes),label
    before=bytes(u.mem_read(owner,20))
    assert call(entry,level,budget,owner)==0xfffffffc
    assert bytes(u.mem_read(owner,20))==before
    call(close,owner);call(close,owner)
    assert not live and bytes(u.mem_read(owner,20))==b'\0'*20
    assert call(entry,level,budget-1,owner)==0xfffffffc
    assert not live and bytes(u.mem_read(owner,20))==b'\0'*20
    if count:
        fail_allocation=True
        assert call(entry,level,budget,owner)==0xfffffffc
        fail_allocation=False
        assert not live and bytes(u.mem_read(owner,20))==b'\0'*20
        fail_read_at=(successful_reads-1)//2+2
        assert call(entry,level,budget,owner)==0xffffffff
        fail_read_at=0
        assert not live and bytes(u.mem_read(owner,20))==b'\0'*20
    assert call(entry,level,budget,owner)==0
    call(close,owner)
    u.mem_write(level+608,w(len(data)-1))
    assert call(entry,level,0x100000,owner)==0xfffffffe
    assert not live and bytes(u.mem_read(owner,20))==b'\0'*20
    return budget


count=nodes_total=0;opening_budget=maximum=0
for level_info,payload in sections():
    nodes=inspect(payload)
    result=subprocess.run([str(ROOT/'build/pc/Release/rf_level_entity_probe.exe'),
                           str(ROOT/'Installed_Game'/level_info['archive']),level_info['file'],'--navigation'],
                          capture_output=True,check=True)
    assert result.stdout==expected_wire(nodes),level_info['file']
    budget=verify_nxdk(payload,level_info['file'])
    assert int(result.stderr)==budget
    maximum=max(maximum,budget)
    if level_info['file'].lower()=='l1s1.rfl':opening_budget=budget
    count+=1;nodes_total+=len(nodes)
synthetic=w(3)
for i in range(3):
    oriented=(0,2,255)[i]
    synthetic+=w(100+i)+b'\xff'+struct.pack('<5fI',4,i,0,1,2,i)+bytes([oriented])
    if oriented:synthetic+=struct.pack('<9f',*range(9))
    synthetic+=b'\x02\x00\xff'+struct.pack('<f',.5)+w(3,9,9,11)
for neighbors in ([1,1,0,0xffffffff,3,2],[],[2,0,2,0x80000000]):
    synthetic+=bytes([len(neighbors)])+w(*neighbors)
verify_nxdk(synthetic,'duplicate/invalid connections')
verify_nxdk(w(0),'empty')
report=dict(result='PASS',sections=count,nodes=nodes_total,opening_bytes=opening_budget,
            maximum_bytes=maximum,nxdk_synthetic_cases=2,scope='PC and compiled NXDK owned records, query references, tags, '
            'ordered adjacency; exact/insufficient budget, nonempty owner, truncation, repeat close, '
            'NXDK allocation and post-allocation I/O failure. PC archive closes before export. NXDK I/O/allocator supplied; '
            'no native XEMU or live AI proof.')
(ROOT/'artifacts/navigation-owned.json').write_text(json.dumps(report,indent=2))
print(report)
