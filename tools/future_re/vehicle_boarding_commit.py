"""Original4a1970 post-admission commit with external services recorded."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
from unicorn.x86_const import UC_X86_REG_ECX
actor=base+0x2000;host=base+0x4000;player=base+0x6000;info=base+0x7000
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
events=[];attach=0;gunner=0;endpred=0
predicates=[0x42a130,0x4a7690,0x425250,0x4a9dc0,0x4a7420,0x40a0d0,0x4adb60]
services=[0x4a8670,0x4ad8a0,0x41ae70,0x427240,0x42acd0,0x407e20,0x407e80,0x489f70,0x42d7b0,0x40a420]
def cb(cpu,a,size,_):
 if a not in predicates+services+[0x4897d0,0x426fc0]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4);pop=0
 if a==0x4897d0:word(arg(2),22);value=host
 elif a==0x426fc0:value=host
 elif a in predicates:value=0
 else:
  counts={0x4a8670:1,0x4ad8a0:1,0x41ae70:2,0x427240:2,0x42acd0:1,0x407e20:4,0x407e80:2,0x489f70:2,0x42d7b0:1,0x40a420:1}
  events.append(dict(address=hex(a),args=[arg(i+1) for i in range(counts[a])]))
  if a in [0x4a8670,0x4ad8a0,0x42d7b0,0x40a420]:return
  value=attach if a==0x427240 else gunner if a==0x42acd0 else endpred if a==0x42d7b0 else 0
  if a==0x427240:assert cpu.reg_read(UC_X86_REG_ECX)==host;pop=8
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for kind,attach,gunner,endpred in itertools.product([1,4],[0,1],[0,1],[0,1]):
 events.clear()
 for addr,n in [(actor,0x2000),(host,0x2000),(player,0x1000),(info,0x1000)]:u.mem_write(addr,bytes(n))
 word(actor+0x200,0xffffffff);word(actor+0x2c,55);word(actor+0x2a4,8);word(0x85cce4,9);word(host+0x294,info);word(host+0x2c,66);word(info+0x1b4,kind);word(host+0x810,0x20025);word(player+0x10,0x82);u.mem_write(0x64ecb9,b'\x00');word(0x7c75d4,player)
 word(info+0x724,0x200 if endpred else 0);word(player+0x10c4,71);word(player+0x10c8,72);word(player+0x10cc,73);u.mem_write(player+0xf94,b'\x01\x01');word(player+0xf98,0x3f800000);word(host+0x1a8,0x12);word(host+0x7c,0x34)
 u.mem_write(actor+0x3c,f(12,23,34));u.mem_write(host+0x3c,f(10,20,30));u.mem_write(host+0x48,f(1,0,0,0,1,0,0,0,1));u.mem_write(info+0x6c,f(1,2,3));u.mem_write(info+0x78,f(4,5,6));u.mem_write(0x7c7618,f(7,8,9));u.mem_write(0x7c75f0,f(10,11,12))
 u.mem_write(stack,struct.pack('<3I',stop,player,actor));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4a1970,stop,count=10000)
 expected=([0x4a8670] if kind==1 else [])+[0x4ad8a0,0x41ae70,0x427240,0x42acd0]+([] if gunner else [0x407e20])+[0x407e80,0x489f70,0x42acd0,0x42d7b0]+([0x40a420] if endpred else [])
 assert [int(e['address'],16) for e in events]==expected
 assert read(player+0x10)==(0x482 if kind==1 else 0x82)
 assert read(host+0x810)==0x10025
 assert bytes(u.mem_read(player+0x10c4,12))==(bytes(12) if kind==1 else struct.pack('<3I',71,72,73))
 assert bytes(u.mem_read(player+0xf94,2))==bytes(2) and read(player+0xf98)==0
 assert read(host+0x1a8)==(0x80000012 if endpred else 0x12) and read(host+0x7c)==(0x6000034 if endpred else 0x34)
 assert bytes(u.mem_read(actor+0x13e0,12))==f(2,3,4)
 assert bytes(u.mem_read(host+0x1434,24))==f(1,2,3,4,5,6)
 assert bytes(u.mem_read(actor+0x1434,24))==(f(7,8,9,10,11,12) if gunner else bytes(24))
 rows.append(dict(kind=kind,attach_return=attach,gunner=gunner,final_predicate=endpred,events=events[:],player_flags=read(player+0x10),host_flags=read(host+0x810),board_local_position=list(struct.unpack('<3f',u.mem_read(actor+0x13e0,12)))))
(R/'artifacts/future-vehicles-re/boarding-commit.json').write_text(json.dumps(dict(scope="Original4a1970 post-admission orchestration; service bodies supplied; real vector transforms and state stores",exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original post-admission cases')
