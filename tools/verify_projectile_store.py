"""Exercise compiled NXDK projectile owner lifecycle with checked resource services."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
B=0x30000000;REG=B+0x10000;LIST=B+0x20000;UID=B+0x21000;DESC=B+0x22000;OPS=B+0x23000;OUT=B+0x24000;INIT=B+0xf0000;CLEAN=INIT+16;STACK=B+0xff000;STOP=B+0xfff00;POOL=39400
p=pefile.PE(str(root/'build/xbox/main.exe'));data=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase;x.mem_map(base,(len(data)+4095)//4096*4096);x.mem_write(base,data);x.mem_map(B,0x100000)
mapping=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
def word(a):return struct.unpack('<I',x.mem_read(a,4))[0]
state=dict(initialized=0,cleaned=0,resources=0,fail=False)
def callback(c,a,size,data):
 sp=c.reg_read(UC_X86_REG_ESP);owner=word(sp+8);record=word(sp+12);handle=word(owner+4);slot=word(owner+12)
 assert word(REG+(handle&65535)*8)==owner and word(REG+(handle&65535)*8+4)==handle
 assert record==B+slot*788
 if a==INIT:
  state['initialized']+=1;assert word(owner+16)==0x06100000 and word(owner+20) and word(LIST+8)==word(B+POOL+8)
  assert word(sp+16)==DESC;state['resources']+=1;x.mem_write(record+400,w(state['resources']));result=-1 if state['fail'] else 0
 else:
  state['cleaned']+=1;assert word(owner+20)==word(owner+24)==0 and word(LIST+8)+1==word(B+POOL+8)
  assert word(record+400) and state['resources'];state['resources']-=1;x.mem_write(record+400,w(0));result=0
 c.reg_write(UC_X86_REG_EAX,result&0xffffffff);c.reg_write(UC_X86_REG_EIP,word(sp));c.reg_write(UC_X86_REG_ESP,sp+4)
for a in (INIT,CLEAN):x.hook_add(UC_HOOK_CODE,callback,begin=a,end=a)
def run(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(symbol(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP,(name,hex(x.reg_read(UC_X86_REG_EIP)));return x.reg_read(UC_X86_REG_EAX)
run('rf_projectile_store_init',[B]);run('rf_object_registry_init',[REG]);run('rf_object_list_init',[LIST]);x.mem_write(UID,w(1000));x.mem_write(OPS,w(INIT,CLEAN));handles=[]
def acquire():
 x.mem_write(OUT,w(0));return run('rf_projectile_store_open',[B,REG,LIST,UID,DESC,OPS,0,OUT])
def close(handle):return run('rf_projectile_store_close',[B,REG,LIST,handle,OPS,0])
for i in range(50):
 assert acquire()==0;owner=word(OUT);assert word(owner+12)==i and word(owner+8)==1000-i and word(owner+16)==0x06500000
 handles.append(word(owner+4));x.mem_write(owner+16,w(word(owner+16)|2));assert word(REG+(handles[-1]&65535)*8)==owner
assert acquire()==0xfffffffd and word(OUT)==0 and word(UID)==950
for handle in handles:assert close(handle)==0
assert acquire()==0 and word(word(OUT)+12)==49;handle=word(word(OUT)+4);assert close(handles[49])==0xfffffffd;assert close(handle)==0
state['fail']=True;assert acquire()==0xffffffff and word(OUT)==0 and word(UID)==948
assert word(B+POOL+8)==word(LIST+8)==state['resources']==0 and word(REG+12292)==1024
x.mem_write(REG+12292,w(0));assert acquire()==0xfffffffd and word(UID)==948;x.mem_write(REG+12292,w(1024))
assert state['initialized']==state['cleaned']==52 and word(REG+12296)==53 and word(B+POOL+12)==50
expected='PROJECTILE_STORE 52 52 53 50 40824'
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--projectile-store'],text=True).strip()==expected
report=dict(result='PASS',initialized=52,cleaned=52,retained_bytes=40824,scope='PC and compiled NXDK ownership adapter, checked init/cleanup callbacks, full pool and registry exhaustion, marked objects remain registered, stale handles cannot close reused slots, failed initialization releases partial resource/list/registry/pool ownership. Original pool and registry components independently verified; not a complete486da0 factory reconstruction or native XEMU scene test.')
(root/'artifacts/projectile-store.json').write_text(json.dumps(report,indent=2));print(report)

