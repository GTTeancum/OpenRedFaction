"""Execute original 4c0320 door dispatch; intercept downstream actions only.

Synthetic object registrations feed the original load-time UID conversion.
This verifies conversion and dispatch ordering, not action implementations.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EBX

exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,0x10000)
trigger=base;array=base+0x1000;stack=base+0xe000;stop=base+0xf000

def words(address,*values): u.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def read(address,n): return struct.unpack('<'+'I'*n,u.mem_read(address,n*4))
triggers=next(x for x in json.loads((root/'artifacts/triggers.json').read_text())['results'] if x['file']=='L1S1.rfl')['records']
events=next(x for x in json.loads((root/'artifacts/events.json').read_text())['results'] if x['file']=='L1S1.rfl')['records']
event_ids={x['uid'] for x in events}
trace=[]

def hook(cpu,address,size,context):
    if address not in (0x46aba0,0x4b6760): return
    sp=cpu.reg_read(UC_X86_REG_ESP)
    ret,target,source,actor=read(sp,4)
    trace.append(dict(action='mover' if address==0x46aba0 else 'event',target=target,source=source,actor=actor))
    cpu.reg_write(UC_X86_REG_EAX,0)
    cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)

u.hook_add(UC_HOOK_CODE,hook)
u.mem_write(0x64ecb9,b'\0\0') # Single-player.
results=[]
for uid in (8542,8522):
    record=next(x for x in triggers if x['uid']==uid)
    handles=[0x12340000+i for i in range(len(record['links']))]
    source=0x23450020;actor=0x34560021
    words(trigger+0x2c,source)
    words(trigger+0x2d4,len(handles),len(handles),array)
    words(array,*record['links'])
    objects=[];controllers=[]
    for i,(target,h) in enumerate(zip(record['links'],handles)):
        obj=base+0x2000+i*0x400
        words(obj+0x24,6 if target in event_ids else 8)
        words(obj+0x2c,h);words(0x7394cc+4*i,obj)
        if target in event_ids:
            words(obj+0x20,target);objects.append(obj)
        else:
            controllers.append(obj)
            key_array=base+0x5000+i*0x100;key=key_array+0x40
            words(obj+0x29c,1,1,key_array);words(key_array,key);words(key,target)
    words(0x73d890,objects[0] if objects else 0x73d880)
    for i,obj in enumerate(objects): words(obj+0x10,objects[i+1] if i+1<len(objects) else 0x73d880)
    words(0x64e63c,controllers[0] if controllers else 0x64e3b0)
    for i,obj in enumerate(controllers): words(obj+0x28c,controllers[i+1] if i+1<len(controllers) else 0x64e3b0)
    words(trigger+0x2b0,0) # No entity backlink branch in these door triggers.
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,trigger)
    u.emu_start(0x4611a1,0x461231,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==0x461231
    assert read(array,len(handles))==tuple(handles),(uid,'UID conversion')
    for suppress in (0,1):
        trace.clear();words(stack,stop,trigger,actor,suppress)
        u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4c0320,stop,count=10000)
        assert u.reg_read(UC_X86_REG_EIP)==stop
        expected=[dict(action='event' if target in event_ids else 'mover',target=h,source=source,actor=actor)
            for target,h in zip(record['links'],handles) if not suppress or target in event_ids]
        assert trace==expected,(uid,suppress,trace,expected)
        results.append(dict(trigger_uid=uid,suppress_movers=suppress,ordered_uid_actions=[
            dict(uid=record['links'][handles.index(x['target'])],action=x['action']) for x in trace]))
report=dict(result='PASS',cases=len(results),scope='Original post-load block 4611a1..461231, object UID lookup 48a4a0, key-owner lookup 46afc0, dispatch 4c0320, array helpers and handle lookup 40a0e0 execute unchanged. Synthetic object registrations; downstream 46aba0/4b6760 intercepted. Single-player door links only. No eligibility, action execution, entity backlinks or C/NXDK equivalence claimed.',results=results)
(root/'artifacts/trigger-dispatch-verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
