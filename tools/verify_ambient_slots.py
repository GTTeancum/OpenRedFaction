"""Original 25-slot ambient table operations versus PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;position=base+0x1000
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,b)
 m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entries=[int(re.search('_rf_ambient_slot_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ('start','volume','position')]
def call(m,address,args):
 m.mem_write(stack,w(stop)+args);m.reg_write(UC_X86_REG_ESP,stack)
 m.emu_start(address,stop,count=10000);assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x505ac0);commands=bytearray();expected=bytearray();counts=[0,0,0];changed=[0,0,0]
for case in range(3072):
 op=case%3;enabled=(0,1,2,255,256,257)[case//3%6]
 handle=((-2147483648,-2,-1,0,1,2599,2600,2147483647) if op==0 else (-2147483648,-1,0,1,12,24,25,2147483647))[case//18%8]
 pool=bytearray(rng.randbytes(600));free=(0,12,24,25,26)[case//144%5]
 for i in range(25):
  # Include entirely full tables, first/last free slots and arbitrary negative sentinels.
  sample=(-1 if i==free else -2147483648 if free==26 and i%2 else rng.randrange(3000))
  pool[i*24:i*24+4]=w(sample)
 pos=f(rng.uniform(-1000,1000),rng.uniform(-1000,1000),rng.uniform(-1000,1000));volume=f((-2.,-0.,0.,.5,1.,4.)[case//18%6])
 u.mem_write(0x17543d8,bytes([enabled&255]));u.mem_write(0x1754170,bytes(pool));u.mem_write(position,pos)
 args=w(handle,position)+volume if op==0 else w(handle)+volume if op==1 else w(handle,position)
 result=call(u,(0x505ac0,0x505b50,0x505b80)[op],args)
 if op:result=0
 after=bytes(u.mem_read(0x1754170,600));expected.extend(w(result)+after)
 x.mem_write(base,bytes(pool));x.mem_write(position,pos)
 args=w(base,enabled,handle,position)+volume if op==0 else w(base,enabled,handle)+volume if op==1 else w(base,enabled,handle,position)
 native=call(x,entries[op],args)
 assert bytes(x.mem_read(base,600))==after and (op!=0 or native==result),(case,op)
 commands.extend(w(op,enabled,handle)+pos+volume+pool);counts[op]+=1;changed[op]+=after!=pool
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--ambient-slots'],input=commands)
assert actual==expected
report=dict(result='PASS',cases=len(commands)//628,operations=counts,changed=changed,original_sha256=digest,
 scope='Unmodified505ac0 allocation,505b50 volume and505b80 position, including vector callee. Full600-byte table comparison with PC and NXDK; first-free/full table, signed sentinels, sample/slot bounds, low-byte audio gate and unchanged neighbors. Finite positions/volume include negative and out-of-unit-range volume. No mixer, PCM loading, scheduling or playback.')
(root/'artifacts/ambient-slots.json').write_text(json.dumps(report,indent=2));print(report)
