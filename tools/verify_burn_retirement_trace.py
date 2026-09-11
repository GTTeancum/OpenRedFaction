"""Original pool traversal through owner fade and actual record release."""
import json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ev=runpy.run_path(str(root/'tools/verify_burn_iteration.py'),init_globals={'corrected':True});g=ev['hook'].__globals__;u=g['u'];b=g['b'];stack=g['stack'];stop=g['stop'];w=g['w'];g['corrected']=False;g['body_entry']=0x42f1dc
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
f=lambda value:struct.pack('<f',value)
emitters=[b+0x4000+i*0x200 for i in range(4)];owner=b+0x7000

def hook(m,address,size,context):
    if address not in (0x5058c0,0x426fc0,0x42f2f0):return
    sp=m.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',m.mem_read(sp,8))
    g['trace'].append((address,g['slot_map'][arg] if address==0x42f2f0 else arg))
    if address==0x42f2f0:return
    m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
wire_cases=[];wire_expected=[];cases=retired=0
for count in (1,2,3,8):
 for mask in range(1<<count):
  for deadline in (-1,1000):
    slots=[b+j*64 for j in range(8)];active=slots[:count];free=slots[count:];g.update(slots=slots,slot_map={ptr:j+1 for j,ptr in enumerate(slots)},missing_handle=0xffffffff)
    u.mem_write(b,bytes(0x2000));g['ring'](active);g['ring'](free)
    for j,node in enumerate(active):
        u.mem_write(node,w(*emitters,0x12340000+j));u.mem_write(node+36,w(-1)+f(1)+w(int(bool(mask&(1<<j))))+f(17.5 if mask&(1<<j) else 0))
    for ptr in emitters:u.mem_write(ptr,bytes(0x180))
    u.mem_write(owner+0x810,w(1));u.mem_write(0x5a4014,f(.125));u.mem_write(0x62f770,w(active[0]));u.mem_write(0x62f76c,w(free[0] if free else 0));u.mem_write(0x62f768,w(deadline));u.mem_write(0x5a3ed8,w(1000));u.mem_write(0x5cb2ec,w(0x5cb060));u.mem_write(0x5cae44,w(0x5cabb8))
    wire_cases.append(g['snapshot']());g.update(trace=[],visits=[],lookups=[],body=[],releases=[])
    u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x42ee80,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop,(count,mask,deadline)
    assert g['visits']==active and g['body']==active,(count,mask,g['visits'])
    expected_releases=[node for j,node in enumerate(active) if mask&(1<<j)];assert g['releases']==expected_releases
    snapshot=g['snapshot']();remaining=[j+1 for j in range(count) if not mask&(1<<j)];free_order=list(range(count+1,9))+[j+1 for j in range(count) if mask&(1<<j)]
    assert struct.unpack('<3I',snapshot[512:])==(free_order[0] if free_order else 0,remaining[0] if remaining else 0,1225)
    for ring in (remaining,free_order):
        for j,token in enumerate(ring):assert struct.unpack('<2I',snapshot[(token-1)*64+56:token*64])==(ring[(j+1)%len(ring)],ring[j-1])
    rows=g['trace'];assert len(rows)<=128
    wire_expected.append(w(0)+snapshot+w(len(rows))+b''.join(w(*row) for row in rows)+bytes((128-len(rows))*8))
    retired+=len(expected_releases);cases+=1
report=dict(result='PASS',cases=cases,retired_records=retired,original_sha256=ev['digest'],scope='Unmodified42ee80 traversal,42f1dc owner tail,42f2f0 fade and42ed20 release, with real timer/constructor/flag predicate. Attachment/spread portion skipped. Every retirement subset for active counts1,2,3,8, including all-active release; exact ring/head ordering and timer1225. Resource/audio/lookup supplied, absent entity reaction/owner links, zero inactive emitter fields. No live adapters.')
(root/'artifacts/burn-retirement-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
