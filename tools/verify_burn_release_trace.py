"""Original burn reset/release and eight-slot pool initialization."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536);trace=[]
def hook(m,address,size,context):
    if address not in (0x4973d0,0x497d80,0x505a40):return
    sp=m.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',m.mem_read(sp,8))
    if address==0x4973d0:arg=m.reg_read(UC_X86_REG_ECX)
    trace.append((address,arg));m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
def ring(nodes):
    for i,node in enumerate(nodes):u.mem_write(node+56,w(nodes[(i+1)%len(nodes)],nodes[i-1]))
def check_ring(head,nodes):
    assert head==(nodes[0] if nodes else 0)
    for i,node in enumerate(nodes):assert bytes(u.mem_read(node+56,8))==w(nodes[(i+1)%len(nodes)],nodes[i-1])
def clean(data):
    result=bytearray(data);result[:16]=bytes(16);result[16:40]=w(-1,-1,-1,-1,-1,-1);result[40:44]=bytes(4);result[44]=0;result[48:52]=bytes(4);return result
def calls(data):
    values=struct.unpack('<16I',data);out=[]
    for emitter in values[:4]:
        if emitter:out.extend([(0x4973d0,emitter),(0x497d80,emitter)])
    if values[9]!=0xffffffff:out.append((0x505a40,values[9]))
    return out
wire_cases=[];wire_expected=[]
def snapshot(nodes,refs):
    mapping={node:i+1 for i,node in enumerate(nodes)}
    data=bytearray()
    for node in nodes:
        record=bytearray(u.mem_read(node,64))
        for off in (56,60):
            ptr=struct.unpack('<I',record[off:off+4])[0];record[off:off+4]=w(mapping.get(ptr,ptr))
        data+=record
    free_head,active_head=struct.unpack('<2I',u.mem_read(0x62f76c,8))
    return bytes(data)+w(mapping.get(free_head,free_head),mapping.get(active_head,active_head))+bytes(u.mem_read(0x62f768,4))+w(*[mapping.get(ref,ref) for ref in refs])
def result_wire(nodes,refs):
    return w(0)+snapshot(nodes,refs)+w(len(trace))+b''.join(w(*row) for row in trace)+bytes((72-len(trace))*8)
cases=0
for active_count in (1,2,3):
 for index in range(active_count):
  for free_count in (0,1,3):
   for mode in (0,1,256,257):
    for owner_case in range(6):
     cases+=1;active=[b+i*64 for i in range(active_count)];free=[b+0x1000+i*64 for i in range(free_count)];target=active[index]
     u.mem_write(b,bytes([0xa5])*0x2000);ring(active);ring(free)
     u.mem_write(0x62f770,w(active[0]));u.mem_write(0x62f76c,w(free[0] if free else 0))
     u.mem_write(target,w(0,0xffffffff,0x12340001,0 if cases%2 else 0x12340002));u.mem_write(target+36,w(0xffffffff if cases%3==0 else 0 if cases%3==1 else 0x23450001))
     entities=[b+0x4000,b+0x5800];others=[b+0x7000,b+0x8800]
     # Cases: no owner, first/second entity, duplicate entity, other owner,
     # duplicate ownership across both lists (entity match wins).
     entity_refs=[target if owner_case in (1,3,5) else 0,target if owner_case in (2,3) else 0]
     other_refs=[target if owner_case in (4,5) else 0,target if owner_case==4 else 0]
     u.mem_write(0x5cb2ec,w(entities[0]));u.mem_write(0x5cae44,w(others[0]))
     for nodes,refs,offset,sentinel in [(entities,entity_refs,0x13d8,0x5cb060),(others,other_refs,0x2d0,0x5cabb8)]:
      for j,node in enumerate(nodes):u.mem_write(node+0x28c,w(nodes[j+1] if j+1<len(nodes) else sentinel));u.mem_write(node+offset,w(refs[j]))
     pool_nodes=[b,b+64,b+128,b+0x1000,b+0x1040,b+0x1080,b+0x10c0,b+0x1100]
     u.mem_write(0x62f768,w(123))
     wire_cases.append(snapshot(pool_nodes,entity_refs+other_refs)+w(pool_nodes.index(target)+1,mode,0))
     before=bytes(u.mem_read(target,64));trace=[];u.mem_write(stack,w(stop,target,mode));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x42ed20,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
     assert trace==calls(before),cases
     assert bytes(u.mem_read(target,56))==clean(before)[:56],cases
     if target in entity_refs:entity_refs[entity_refs.index(target)]=0
     elif target in other_refs:other_refs[other_refs.index(target)]=0
     for nodes,refs,offset in [(entities,entity_refs,0x13d8),(others,other_refs,0x2d0)]:
      for node,ref in zip(nodes,refs):assert bytes(u.mem_read(node+offset,4))==w(ref),cases
     wire_expected.append(result_wire(pool_nodes,entity_refs+other_refs))
     if mode&255:
      check_ring(active[0],active);check_ring(free[0] if free else 0,free)
      assert bytes(u.mem_read(target+56,8))==before[56:]
     else:
      active.remove(target);free.append(target)
      check_ring(struct.unpack('<I',u.mem_read(0x62f770,4))[0],active)
      check_ring(struct.unpack('<I',u.mem_read(0x62f76c,4))[0],free)
# Actual initialization, including unmodified release and timer clear4fa3e0.
slots=[0x62f778+i*64 for i in range(8)];u.mem_write(0x5cb2ec,w(0x5cb060));u.mem_write(0x5cae44,w(0x5cabb8));wanted=[];before=[]
for i,node in enumerate(slots):
 data=bytearray([0xa5]*64);data[:16]=w(i,0,0,0);data[36:40]=w(-1 if i%2 else i);u.mem_write(node,bytes(data));before.append(data);wanted+=calls(data)
u.mem_write(0x62f768,w(123));wire_cases.append(snapshot(slots,[0]*4)+w(1,1,1));trace=[];u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x42e8a0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
assert trace==wanted;assert bytes(u.mem_read(0x62f768,4))==w(-1);assert bytes(u.mem_read(0x62f770,4))==w(0)
check_ring(struct.unpack('<I',u.mem_read(0x62f76c,4))[0],slots)
for node,data in zip(slots,before):assert bytes(u.mem_read(node,56))==clean(data)[:56]
wire_expected.append(result_wire(slots,[0]*4))
report=dict(result='PASS',release_cases=cases,initialization_cases=1,pool_slots=8,pool_record_bytes=64,original_sha256=digest,scope='Complete42ed20 release and42e8a0 initialization; only emitter reset/free and sound stop intercepted. Actual ordered owner lists, circular free/active lists and timer clear. Duplicate-owner first-match behavior, low-byte reset mode, missing emitter/voice conventions and preserved source/padding verified. No actual particle/audio release implementation or per-frame burn update.')
(root/'artifacts/burn-release-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
