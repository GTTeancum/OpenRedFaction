"""Original 4d9130 release notifications versus PC and compiled NXDK.

Hooks observe/skip only the original visibility dispatcher and the port's
caller callback; release, reference counts and linked-list mutation execute.
Imports the existing pool oracle to run its unchanged operation/cache checks.
"""
import itertools
from verify_vfx_light_pool import *
CALLBACK=B+0xd000
x.mem_write(CALLBACK,b"\xc3")
observed=[]
def original_notify(u,address,size,user):
 if address!=0x4d8660:return
 sp=u.reg_read(UC_X86_REG_ESP);source=read(u,sp+4);update=read(u,sp+8)
 observed.append([1,(source-P)//268,update,read(u,0xc96878),read(u,0xc96874),
                  read(u,0xc96890),read(u,0xc9687c),read(u,source+8),read(u,source+0x58),
                  (read(u,0xc96768)-P)//268])
 u.reg_write(UC_X86_REG_EIP,read(u,sp));u.reg_write(UC_X86_REG_ESP,sp+4)
def port_notify(u,address,size,user):
 if address!=CALLBACK:return
 sp=u.reg_read(UC_X86_REG_ESP);context=read(u,sp+4);pool=read(u,sp+8);id=read(u,sp+12);update=read(u,sp+16)
 sources=read(u,pool+36);links=read(u,pool+40);link=links+id*16
 u.mem_write(context,w(read(u,context)+1,id,update,read(u,pool+4),read(u,pool+8),
                       read(u,pool+12),read(u,pool+16),read(u,sources+id*80),read(u,link),
                       read(u,pool+20+read(u,link+12)*4)))
o.hook_add(UC_HOOK_CODE,original_notify,begin=0x4d8660,end=0x4d8660)
# The imported baseline already translated this dispatcher without hooks.
o.ctl_remove_cache(0x400000,0x800000)
x.hook_add(UC_HOOK_CODE,port_notify,begin=CALLBACK,end=CALLBACK)
inputs=[];responses=[];cases=0;notifications=0
for cls,update,refs in itertools.product((0,1,255),(0,1,2,255,256,257,0xffffffff),(0,1,2,7,-3)):
 inputs.append(w(cls,update,refs))
 assert call('rf_vfx_light_pool_init',[OWNER,FACE,PTR,1])==0
 x.mem_write(OWNER+4,w(1,17,1,7,0,0xffffffff,0,0xffffffff))
 x.mem_write(FACE,w(2)+bytes(72)+bytes((0,cls,0,0)))
 x.mem_write(PTR,w(refs,0xffffffff,0xffffffff,0));x.mem_write(OUT,bytes(40))
 o.mem_write(P,bytes(268));o.mem_write(P,w(0xc96768,0xc96768,2))
 o.mem_write(P+0x4d,bytes((cls,)));o.mem_write(P+0x58,w(refs))
 o.mem_write(0xc96768,w(P,P));o.mem_write(0xc4e6b8,w(0xc4e6b8,0xc4e6b8))
 o.mem_write(0xc96878,w(1));o.mem_write(0xc96874,w(17));o.mem_write(0xc96890,w(1));o.mem_write(0xc9687c,w(7))
 observed.clear();original(0x4d9130,[0,update])
 status=call('rf_vfx_light_pool_release_update',[OWNER,0,update,CALLBACK,OUT]);assert status==0
 actual=list(struct.unpack('<10I',x.mem_read(OUT,40)))
 assert actual==(observed[0] if observed else [0]*10),(cls,update,refs,actual,observed)
 assert len(observed)<=1
 assert [read(x,OWNER+a) for a in (4,8,12,16)]==[read(o,a) for a in (0xc96878,0xc96874,0xc96890,0xc9687c)]
 assert read(x,FACE)==read(o,P+8) and read(x,PTR)==read(o,P+0x58)
 assert read(x,OWNER+20)==(0 if refs>1 else 0xffffffff)
 assert read(x,OWNER+28)==(0 if refs>1 else 0xffffffff)
 responses.append(w(status)+bytes(x.mem_read(OUT,40))+bytes(x.mem_read(OWNER,36))+bytes(x.mem_read(FACE,80))+bytes(x.mem_read(PTR,16)))
 cases+=1;notifications+=len(observed)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--light-release-notify'],input=b''.join(inputs))
expected=b''.join(responses)
assert pc==expected, next((i,pc[i:i+16].hex(),expected[i:i+16].hex()) for i in range(min(len(pc),len(expected))) if pc[i]!=expected[i])
# Corrupt refcount underflow is rejected before any mutation/callback.
assert call('rf_vfx_light_pool_init',[OWNER,FACE,PTR,1])==0
x.mem_write(FACE,w(2));x.mem_write(PTR,w(0x80000000));x.mem_write(OUT,bytes(40))
snapshot=bytes(x.mem_read(OWNER,44))+bytes(x.mem_read(FACE,80))+bytes(x.mem_read(PTR,16))
assert call('rf_vfx_light_pool_release_update',[OWNER,0,1,CALLBACK,OUT])!=0
assert snapshot==bytes(x.mem_read(OWNER,44))+bytes(x.mem_read(FACE,80))+bytes(x.mem_read(PTR,16))
assert bytes(x.mem_read(OUT,40))==bytes(40)
report=dict(result='PASS',original_pc_nxdk_release_cases=cases,notifications=notifications,underflow_guard=1,
 scope='Visibility dispatcher observed at original4d8660; reference decrement, generation, live source, active cache and list head captured before unlink. Full update word preserved. No scene visibility or rendered-lighting claim.')
(root/'artifacts/light-release-notify.json').write_text(json.dumps(report,indent=2));print(report)
