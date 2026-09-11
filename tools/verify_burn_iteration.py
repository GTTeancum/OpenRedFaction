"""Bounded original42ee80 iteration audit, including missing-owner release."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536);visits=[];lookups=[];body=[];releases=[]
corrected=globals().get('corrected',False);trace=[]
def hook(m,address,size,context):
    if corrected and address==0x42ef39:m.reg_write(UC_X86_REG_EIP,0x42f2a2);return
    if address==0x42ee13:trace.append((0x42ee13,slot_map[m.reg_read(UC_X86_REG_ESI)]));return
    if address==0x42eee1:
        if len(visits)<16:visits.append(m.reg_read(UC_X86_REG_ESI))
        return
    if address==0x42ed20:
        sp=m.reg_read(UC_X86_REG_ESP);releases.append(struct.unpack('<I',m.mem_read(sp+4,4))[0]);return
    if address==0x42ef3e:
        trace.append((0x42ef3e,slot_map[m.reg_read(UC_X86_REG_ESI)]));body.append(m.reg_read(UC_X86_REG_ESI));m.reg_write(UC_X86_REG_EIP,0x42f2a2);return
    if address not in (0x40a0e0,0x4973d0,0x497d80,0x505a40):return
    sp=m.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',m.mem_read(sp,8));result=0
    trace.append((address,m.reg_read(UC_X86_REG_ECX) if address==0x4973d0 else arg))
    if address==0x40a0e0:lookups.append(arg);result=0 if arg==missing_handle else b+0x7000
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
def ring(nodes):
    for i,node in enumerate(nodes):u.mem_write(node+56,w(nodes[(i+1)%len(nodes)],nodes[i-1]))
results=[];wire_cases=[];wire_expected=[]
def snapshot():
    data=bytearray()
    for ptr in slots:
        record=bytearray(u.mem_read(ptr,64))
        for off in (56,60):
            link=struct.unpack('<I',record[off:off+4])[0];record[off:off+4]=w(slot_map.get(link,link))
        data+=record
    heads=struct.unpack('<2I',u.mem_read(0x62f76c,8))
    return bytes(data)+w(*[slot_map.get(ptr,ptr) for ptr in heads])+bytes(u.mem_read(0x62f768,4))
for active_count in (1,2,3):
 for missing_index in range(active_count):
  for free_count in (0,1,2):
   for missing_emitter in (-1,0,1,2,3):
    active=[b+i*64 for i in range(active_count)];free=[b+0x1000+i*64 for i in range(free_count)];selected=active[missing_index];missing_handle=0x12340000+missing_index
    slots=(active+free+[b+0x1800+j*64 for j in range(8)])[:8];slot_map={ptr:j+1 for j,ptr in enumerate(slots)}
    u.mem_write(b,bytes(0x2000));ring(active);ring(free)
    for i,node in enumerate(active):
        u.mem_write(node,w(1,2,3,4,0x12340000+i));u.mem_write(node+36,w(-1))
    if missing_emitter>=0:u.mem_write(selected+missing_emitter*4,w(0))
    u.mem_write(0x62f770,w(active[0]));u.mem_write(0x62f76c,w(free[0] if free else 0));u.mem_write(0x62f768,w(-1));u.mem_write(0x5a3ed8,w(1000));u.mem_write(0x5cb2ec,w(0x5cb060));u.mem_write(0x5cae44,w(0x5cabb8))
    wire_cases.append(snapshot()+w(missing_handle));trace=[]
    visits=[];lookups=[];body=[];releases=[];u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x42ee80,stop,count=10000);completed=u.reg_read(UC_X86_REG_EIP)==stop
    wire_expected.append(w(0)+snapshot()+w(len(trace))+b''.join(w(*row) for row in trace)+bytes((64-len(trace))*8))
    if corrected:
        assert completed and visits==active
        assert releases==([selected] if missing_emitter<0 else [])
        assert body==[ptr for ptr in active if ptr!=selected]
        assert bytes(u.mem_read(0x62f768,4))==w(1225)
    elif missing_emitter<0:
        assert not completed and releases==[selected],(active_count,missing_index,free_count,visits)
        assert lookups==[0x12340000+i for i in range(missing_index+1)]
        assert body==active[:missing_index]
        # After release, the repeated path only visits reset/free records.
        repeated=visits[missing_index+1:];assert len(repeated)>4 and selected in repeated
        for ptr in set(repeated):assert bytes(u.mem_read(ptr,16))==bytes(16)
        assert bytes(u.mem_read(0x62f768,4))==w(-1) # timer epilogue not reached
    else:
        assert completed and not releases
        assert body==[ptr for ptr in active if ptr!=selected]
        assert missing_handle not in lookups and visits==active
        assert bytes(u.mem_read(0x62f768,4))==w(1225)
    results.append(dict(active=active_count,missing_index=missing_index,free=free_count,missing_emitter=missing_emitter,completed=completed,visits=[hex(ptr) for ptr in visits],release_count=len(releases)))
report=dict(corrected=corrected,result='PASS',cases=len(results),nonterminating_missing_owner_cases=sum(not r['completed'] for r in results),instruction_bound=10000,original_sha256=digest,scope='Original42ee80 traversal with real42ed20 list release, constructors and timer epilogue. Live-owner body skipped at42ef3e to isolate iteration; object lookup and emitter/audio release supplied. Missing emitter paths terminate, missing owner with all emitters enters free-ring cycle. Bounded instruction exhaustion plus repeated zero-emitter free records and unchanged deadline confirm cycle; no unbounded process run.',results=results)
if corrected:report['scope']='Original42ee80 traversal with42ef39 redirected to42f2a2 to resume saved next active record; real42ed20 and timer epilogue. Live-owner body skipped and external resource callbacks supplied. All90 cases complete, including18 formerly cycling missing-owner paths. No installed binary modification.'
(root/('artifacts/burn-iteration-corrected.json' if corrected else 'artifacts/burn-iteration.json')).write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})
