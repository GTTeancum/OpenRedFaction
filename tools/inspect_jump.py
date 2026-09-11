"""Execute original 4288b0 jump and 4281a0 fall transition; isolate parent/audio queries."""
import hashlib,itertools,json,math,random,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
binary=root/'Installed_Game/RF.exe';sha=hashlib.sha256(binary.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(binary)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);cls=base+0x2000;descriptor=base+0x3000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);f=lambda v:struct.pack('<f',v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
real=lambda a:struct.unpack('<f',u.mem_read(a,4))[0]
vehicle=sub=0;events=[]
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x4290d0:value=vehicle
 elif address==0x40a270:value=sub
 elif address==0x434d00:events.append(['sound_lookup',list(struct.unpack('<5I',cpu.mem_read(sp+4,20)))]);value=123
 else:events.append(['sound_play',read(sp+4)]);value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for address in [0x4290d0,0x40a270,0x434d00,0x505560]:u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
# Resolve the shipped game.tbl height and execute the original fsqrt initializer.
inventory=json.loads((root/'artifacts/inventory.json').read_text())
archive=next(a for a in inventory['files'] if a['path']=='tables.vpp');entry=next(e for e in archive['vpp']['entries'] if e['name']=='game.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as stream:stream.seek(entry['offset']);table=stream.read(entry['size']).decode('cp1252')
height=float(re.search(r'^\$Max Entity Jump Height:[ \t]*([0-9.]+)',table,re.M)[1]);u.mem_write(0x5cb050,f(height))
initializer=image.index(bytes.fromhex('d905dc005a00'),0x33e80,0x33eb4)+0x400000
u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(initializer,0x433eb4,count=100)
shipped_strength=real(0x62f2c8);assert f(shipped_strength)==f(math.sqrt(2*real(0x5a00dc)*real(0x5cb050)))
rng=random.Random(0x4288b0);results=[]
for mode,crouch,waterflag,vehicle,sub,velocity,strength,dt,enabled in itertools.product(range(16),(0,1),(0,1),(0,1),(0,1),(-2.,0.,3.),(6.,shipped_strength),(1/60,1/30),(0,1)):
 seed=bytearray(rng.randbytes(0x1500));flags=(rng.getrandbits(32)&~0x2400)|crouch*0x400|waterflag*0x2000
 seed[0x294:0x298]=w(cls);seed[0x858:0x85c]=w(descriptor);seed[0x810:0x814]=w(flags);seed[0x148:0x14c]=f(velocity)
 u.mem_write(base,bytes(seed));u.mem_write(cls,bytes(0x1000));u.mem_write(cls+0x120,w(456));u.mem_write(descriptor,w(1,mode,0,0,0,0,0,0))
 for i in (3,8):u.mem_write(0x62fe50+i*32,w(enabled,i,0,0,0,0,0,0))
 u.mem_write(0x630050,w(77));u.mem_write(0x6460f0,w(777));u.mem_write(0x62f2c8,f(strength));u.mem_write(0x5a4014,f(dt))
 u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);events.clear();u.emu_start(0x4288b0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 accepted=(mode==1 or (mode==4 and not waterflag)) and not crouch and not vehicle
 wanted=bytearray(seed);selected=(8 if sub else 3) if enabled else 0
 if accepted:
  impulse=strength
  # 428935..428977 keeps the scaled impulse on x87 until after adding
  # negative velocity; there is no intermediate binary32 store.
  if mode==4:impulse=(real(0x58950c)-(real(0x5893c4)-real(0x5a4014))*real(0x5895a0))*strength
  if velocity<0:impulse+=velocity
  wanted[0x148:0x14c]=f(impulse);wanted[0x810:0x814]=w(flags|2);wanted[0x1a8:0x1ac]=w(struct.unpack_from('<I',seed,0x1a8)[0]|1)
  wanted[0x858:0x85c]=w(0x62fe50+selected*32);wanted[0x85c:0x860]=w(0x73a858);wanted[0x7b4:0x7b8]=w(777)
  assert events==[['sound_lookup',[456,0,0,0,0x3f800000]],['sound_play',123]]
 else:assert events==[]
 assert bytes(u.mem_read(base+0x8a0,16))==seed[0x8a0:0x8b0], "Jump changed support velocity/handle"
 actual=bytes(u.mem_read(base,len(seed)));assert actual==wanted,(mode,crouch,waterflag,vehicle,sub,velocity,[(i,actual[i],wanted[i]) for i in range(len(seed)) if actual[i]!=wanted[i]])
 assert read(0x630050)==(selected if accepted else 77)
 results.append(dict(mode=mode,crouch=crouch,waterflag=waterflag,parent_block=vehicle,alternate_fall=sub,frame_dt=real(0x5a4014),descriptor_enabled=enabled,velocity=velocity,accepted=accepted,jump_strength=strength,output_velocity=real(base+0x148),actor_flags=flags,physics_flags=struct.unpack_from("<I",seed,0x1a8)[0],old_time=struct.unpack_from("<I",seed,0x7b4)[0],final_actor_flags=read(base+0x810),final_physics_flags=read(base+0x1a8),final_time=read(base+0x7b4),events=list(events)))
# A null entity must return without querying parent/fall/audio or touching selection.
u.mem_write(0x630050,w(77));u.mem_write(stack,w(stop,0));u.reg_write(UC_X86_REG_ESP,stack);events.clear()
u.emu_start(0x4288b0,stop,count=100)
assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4 and not events and read(0x630050)==77
report=dict(result='PASS',null_entity_preserved=True,support_velocity_and_handle_preserved=True,original_sha256=sha,shipped_height=height,shipped_strength=shipped_strength,cases=len(results),accepted=sum(r['accepted'] for r in results),climb_rejected=all(not r['accepted'] for r in results if r['mode']==2),scope='Original jump and fall transition. Parent-kind predicate, alternate-fall predicate and audio boundaries supplied. No input dispatch, shared-C jump or live jumping claim.',results=results)
out=root/'artifacts/jump-original';out.mkdir(exist_ok=True);(out/'report.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
