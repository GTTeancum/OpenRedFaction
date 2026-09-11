"""Foley lifetime budgets, registration order and allocation-failure cleanup."""
import contextlib,io,json,re,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with contextlib.redirect_stdout(io.StringIO()):env=runpy.run_path(str(root/'tools/verify_foley_table.py'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
u=env['u'];base=env['base'];stack=env['stack'];stop=env['stop'];owner=base+0x110000;callback=base+0x1e0000
symbols=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
alloc,release,opened,closed=map(symbol,['malloc','free','rf_foley_open','rf_foley_close'])
heap=base+0x40000;blocks={};attempts=0;fail=0;calls=[];expected=[]
def hook(uc,address,size,context):
 global heap,attempts
 if address not in (alloc,release,callback):return
 sp=uc.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<II',uc.mem_read(sp,8));result=0
 if address==alloc:
  attempts+=1
  if attempts!=fail:
   result=heap;heap+=(arg+15)&~15;blocks[result]=arg;uc.mem_write(result,bytes([0xa5])*arg)
 elif address==release:
  if arg:
   size=blocks.pop(arg);uc.mem_write(arg,bytes([0xdd])*size)
 else:
  _,name=struct.unpack('<II',uc.mem_read(sp+4,8));params=bytes(uc.mem_read(sp+12,12))
  string=bytes(uc.mem_read(name,61)).split(b'\0',1)[0]
  index,row=expected[len(calls)];assert string==row[:61].split(b'\0',1)[0] and params==row[64:76]
  calls.append(index);result=(index+101 if (index+1)%7 else -3)&0xffffffff
 uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
def invoke(entry,args):
 u.mem_write(stack,struct.pack('<I',stop)+struct.pack('<'+'I'*len(args),*args));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(entry,stop,count=30000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 return u.reg_read(UC_X86_REG_EAX)
reports=[]
for number in (0,1,7):
 raw=env['cases'][number];out=env['outputs'][number];_,ng,ns=struct.unpack_from('<iII',out)
 group_data=out[12:12+ng*44];rows=[out[12+ng*44+i*76:12+ng*44+(i+1)*76] for i in range(ns)]
 expected=[(i,row) for i,row in enumerate(rows) if row[0]]
 path=root/'artifacts/foley-owner-test.tbl';path.write_bytes(raw)
 pc=subprocess.run([str(root/'build/pc/Release/rf_audio_probe.exe'),'--foley-owner',str(path)],capture_output=True,text=True,check=True).stdout.strip()
 resident=24+ng*44+ns*4;peak=resident+ns*76
 for failure in ([0,1,2] if ns else [0]):
  fail=failure;attempts=0;calls=[];blocks={};heap=base+0x40000
  u.mem_write(base,raw);u.mem_write(owner,bytes(24))
  result=invoke(opened,[base,len(raw),peak,callback,0,owner])
  if failure:
   assert result==0xfffffffc and not blocks and not calls and bytes(u.mem_read(owner,24))==bytes(24)
   continue
  assert result==0 and len(calls)==len(expected)
  gp,sp,native_ng,native_ns,r,p=struct.unpack('<6I',u.mem_read(owner,24))
  assert (native_ng,native_ns,r,p)==(ng,ns,resident,peak)
  u.mem_write(base,bytes(len(raw))) # Discard the source; free hook already poisoned declarations.
  if ng:assert bytes(u.mem_read(gp,ng*44))==group_data
  ids=[i+101 if (i+1)%7 else -3 for i in range(ns)]
  for i,row in enumerate(rows):
   if not row[0]:ids[i]=-1
  if ns:assert bytes(u.mem_read(sp,ns*4))==struct.pack('<'+'i'*ns,*ids)
  invoke(closed,[owner]);invoke(closed,[owner]);assert not blocks and bytes(u.mem_read(owner,24))==bytes(24)
 # Short budget must not allocate or register.
 fail=0;attempts=0;calls=[];blocks={};u.mem_write(base,raw);u.mem_write(owner,bytes(24))
 assert invoke(opened,[base,len(raw),peak-1,callback,0,owner])==0xfffffffc
 assert not attempts and not calls and not blocks and bytes(u.mem_read(owner,24))==bytes(24)
 reports.append(dict(fixture=number,groups=ng,samples=ns,resident=resident,peak=peak,pc=pc))
for raw in (env['cases'][2],env['cases'][4]):
 fail=0;attempts=0;calls=[];blocks={};u.mem_write(base,raw);u.mem_write(owner,bytes(24))
 assert invoke(opened,[base,len(raw),200000,callback,0,owner])!=0
 assert not attempts and not calls and not blocks and bytes(u.mem_read(owner,24))==bytes(24)
report=dict(result='PASS',fixtures=reports,invalid_preflight_cases=2,scope='PC and NXDK owner validation, exact/short peak budgets, ordered registration arguments, empty names, negative IDs, discarded source/temporary storage and repeated close. NXDK allocator hooks inject each allocation failure and check cleanup. No actual audio registration/device or original global initialization-order proof.')
(root/'artifacts/foley-owner.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
