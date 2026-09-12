"""Verify original declaration presence independently of motion filename content."""
import json,runpy
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"local/python"))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
v=runpy.run_path(str(Path(__file__).with_name('verify_action_name.py')))
u=v['original'];native=v['native'];b=v['b'];w=v['w'];call=v['call'];root=v['root'];table=v['declarations'];strings=v['strings'];query=v['query'];out=b+0x2000
import re
entry=int(re.search(r'\s_rf_entity_declared_action_lookup\s+([0-9a-fA-F]+)',v['mapping'])[1],16)
names=[x['name'].encode() for x in json.loads((root/'artifacts/animation-names.json').read_text())['actions']['names']]
assigned=[]
def hook(cpu,address,size,data):
 if address!=0x4ffa20:return
 sp=cpu.reg_read(UC_X86_REG_ESP);src=int.from_bytes(cpu.mem_read(sp+4,4),'little')
 assert cpu.reg_read(UC_X86_REG_ECX)==out
 assigned.append(src);cpu.reg_write(UC_X86_REG_EAX,out);cpu.reg_write(UC_X86_REG_ESP,sp+8);cpu.reg_write(UC_X86_REG_EIP,int.from_bytes(cpu.mem_read(sp,4),'little'))
u.hook_add(UC_HOOK_CODE,hook);cases=0
for empty in (False,True):
 u.mem_write(table,bytes(45*52));u.mem_write(0x5ccc58,w(45));u.mem_write(0x5ccc60,w(table))
 for i,name in enumerate(names):
  u.mem_write(strings+i*64,name+b'\0');u.mem_write(table+(44-i)*52,w(len(name),strings+i*64,0 if empty else 6,0 if empty else query+100))
 u.mem_write(query+100,b'a.rfa\0')
 for i,name in enumerate(names):
  for cpu in (u,native):cpu.mem_write(query+16,name.upper()+b'\0')
  u.mem_write(query,w(len(name),query+16));assigned.clear()
  assert call(u,0x419a00,out,0,-1,query,0)==44-i
  assert assigned==[table+(44-i)*52+8]
  native.mem_write(out,w(0xffffffff,0x1fff));assert call(native,entry,out,1,2,query+16)==i;cases+=1
u.mem_write(0x5ccc58,w(0));assigned.clear();assert call(u,0x419a00,out,0,-1,query,0)==0xffffffff and not assigned
report=dict(result='PASS',cases=cases,scope='Original419a00 declaration selection executes with actual500150/string comparison; only4ffa20 string assignment is observed at its boundary. Reversed declaration order and empty/nonempty motion filenames select identical declarations. NXDK canonical mask lookup returns actor action slot. Startup base mappings only; weapon switching excluded.')
(root/'artifacts/action-declaration-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
