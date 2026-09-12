"""Original54e200 prepared skeletal traversal versus PC/NXDK; only51ba00 supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
b=0x30000000;model=b+0x1000;count=model+0x48;query=b+0x2000;hit=b+0x3000;backend=b+0x4000;callback=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_collision_model_pose_trace\s+([0-9a-fA-F]+)',mapping)[1],16)
model=b+0x1000;pose=b+0x2000;part=b+0x3000;table=b+0x4000;lod=b+0x4100;batch=b+0x4200;query=b+0x6000
u.mem_write(model+0x90,w(part));u.mem_write(part+0x8c,w(table));u.mem_write(table,w(1,lod));u.mem_write(lod+8,w(batch));u.mem_write(lod+12,struct.pack('<H',1))
u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
def prepared(m,address,size,_):
 sp=m.reg_read(UC_X86_REG_ESP);assert m.reg_read(UC_X86_REG_ECX)==pose
 assert struct.unpack('<6I',m.mem_read(sp+4,24))==(0,model,0,0,0,1)
 m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0]);m.reg_write(UC_X86_REG_ESP,sp+28)
u.hook_add(UC_HOOK_CODE,prepared,None,0x51ba00,0x51ba00)
observed=[0]
def count_vertex(m,address,size,_):
 ret=struct.unpack('<I',m.mem_read(m.reg_read(UC_X86_REG_ESP),4))[0]
 if 0x54e200<=ret<0x54e530:observed[0]+=1
u.hook_add(UC_HOOK_CODE,count_vertex,None,0x539530,0x539530)
rng=random.Random(0x54e200);commands=[];answers=[];hits=0
coverage=dict(empty_batches=0,first_batch_early_exit=0,both_batches_processed=0)
for case in range(8192):
 start=[rng.uniform(-3,3),rng.uniform(-3,3),2.];delta=[0.,0.,-4.];radius=(0,.025,.25,1)[case%4];flags=case//4%2;limit=(.25,.5,1,2)[case//8%4]
 q=bytes(72)+f([radius])+w(flags)+f(start+delta);initial=f([limit,11,12,13,14,15,16])+w(0x12345678)
 matrices=[]
 for n in range(4):
  matrices.extend([1,0,0,0,1,0,0,0,1,0,0,n*.25] if case<2048 else [rng.randrange(-8,9)/8 for _ in range(12)])
 positions=[-2,-2,0,2,-2,0,0,2,0,-2,-2,1,2,-2,1,0,2,1]
 links=b''.join(bytes(([255,0,0,0] if case%3==0 else [128,128,0,0] if case%3==1 else [rng.randrange(256) for _ in range(4)])+[rng.randrange(4) for _ in range(4)]) for n in range(6))
 records=struct.pack('<8H',0,1,2,0x20,3,4,5,0);count=case//32%3;command=q+initial+f(matrices+positions)+links+records+w(count);assert len(command)==468
 second_positions=[value+(1 if i%3==2 else 0) for i,value in enumerate(positions)]
 second_links=b''.join(links[i:i+8] for i in range(40,-1,-8))
 second_records=records[8:]+records[:8];second_count=case//17%3;batch_count=case//7%3
 command+=f(second_positions)+second_links+second_records+w(second_count,batch_count)
 assert len(command)==612;commands.append(command)
 u.mem_write(lod+12,struct.pack('<H',batch_count));u.mem_write(0x1d0e9b8,b'\xa5'*72)
 u.mem_write(batch+0x38+4,w(b+468));u.mem_write(batch+0x38+0x14,w(b+588));u.mem_write(batch+0x38+0x1c,w(b+540));u.mem_write(batch+0x38+0x28,struct.pack('<HH',6,second_count))
 u.mem_write(b,command);u.mem_write(query,q);u.mem_write(pose+0x960,f(matrices));u.mem_write(batch+4,w(b+328));u.mem_write(batch+0x14,w(b+448));u.mem_write(batch+0x1c,w(b+400));u.mem_write(batch+0x28,struct.pack('<HH',6,count))
 observed[0]=0;u.mem_write(stack,w(stop,model,pose,query,b+104));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x54e200,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 accepted=u.reg_read(UC_X86_REG_EAX)&255;expected=w(accepted)+bytes(u.mem_read(b+104,32))+bytes(u.mem_read(0x1d0e9b8,72));answers.append(expected);hits+=accepted
 if batch_count==0:
  assert observed[0]==0 and expected[36:]==b'\xa5'*72
  coverage['empty_batches']+=1
 elif batch_count==2:
  assert observed[0] in (6,12)
  if observed[0]==6:
   assert accepted==1 and flags==1
   coverage['first_batch_early_exit']+=1
  else:coverage['both_batches_processed']+=1
 assert bytes(u.mem_read(query,104))==q and bytes(u.mem_read(b+136,476))==command[136:]
 x.mem_write(b,command);x.mem_write(b+0x6000,w(b+328,b+400,b+448)+struct.pack('<HH',6,count));x.mem_write(b+0x6010,w(b+468,b+540,b+588)+struct.pack('<HH',6,second_count));x.mem_write(b+0x5000,b'\xa5'*72);x.mem_write(stack,w(stop,b+0x6000,batch_count,b+136,4,b,b+104,b+0x5000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+104,32))+bytes(x.mem_read(b+0x5000,72));assert actual==expected,('NXDK',case,expected.hex(),actual.hex())
 assert bytes(x.mem_read(b,104))==q and bytes(x.mem_read(b+136,476))==command[136:]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-pose-trace-multi'],input=b''.join(commands));assert actual==b''.join(answers),'PC mismatch'
assert all(coverage.values()),coverage
report=dict(coverage=coverage,result='PASS',cases=len(commands),hits=hits,original_sha256=sha,scope='Original54e200 geometry traversal with51ba00 alone supplied via checked ABI and prepared matrices; actual vertex/triangle/math callees. PC/NXDK exact result and every posed vertex, preserved input; zero to two distinct batches/two triangles each, poisoned scratch preservation and reuse, mixed/zero-terminated weights, identity/arbitrary matrices and first-hit/time/radius cases. No full pose preparation/native scene claim.')
(root/'artifacts/model-pose-trace-multi.json').write_text(json.dumps(report,indent=2));print(report)
