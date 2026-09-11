"""Original pain timers/RNG for the registered guard's two-hit fixture."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
folder=root/'artifacts/npc-damage-binding';lines=(folder/'pc.txt').read_text().splitlines()
observed=next(list(map(int,l.split()[1:])) for l in lines if l.startswith('NPC_PAIN_TEST '))
sound_observed=next(list(map(int,l.split()[1:])) for l in lines if l.startswith('NPC_PAIN_SOUND_TEST '))
audio=next(list(map(int,l.split()[1:])) for l in lines if l.startswith('NPC_PAIN_AUDIO '))
def table(name):
 with (root/'Installed_Game/tables.vpp').open('rb') as f:
  count=struct.unpack('<4I',f.read(16))[2];at=2048+(count*64+2047)//2048*2048
  for i in range(count):
   f.seek(2048+i*64);row=f.read(64);size=struct.unpack_from('<I',row,60)[0]
   if row[:60].split(b'\0')[0].decode()==name:f.seek(at);return f.read(size)
   at+=(size+2047)//2048*2048
 raise AssertionError(name)
parts=re.split(rb'\$Name:\s*"([^"\r\n]*)"',re.sub(rb'//[^\r\n]*',b'',table('foley.tbl')))
group=next(body for label,body in zip(parts[1::2],parts[2::2]) if label==b'Grd Small Pain')
sample_names=re.findall(rb'\$Sound:\s*"([^"\r\n]*)"',group)
assert len(sample_names)==int(re.search(rb'\$Sounds:\s*(\d+)',group)[1])==5
decl=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--action',str(root/'Installed_Game/tables.vpp'),'env_guard','','flinch_stand'])
assert decl[:4]==bytes(4) and not decl[68:132].split(b'\0')[0]
name=decl[4:68].split(b'\0')[0].decode().split('.')[0]+'.rfa'
with (root/'Installed_Game/motions.vpp').open('rb') as f:
 count=struct.unpack('<4I',f.read(16))[2];at=2048+((count*64+2047)//2048)*2048
 for i in range(count):
  f.seek(2048+i*64);row=f.read(64);size=struct.unpack_from('<I',row,60)[0]
  if row[:60].split(b'\0')[0].decode().lower()==name.lower():f.seek(at);header=f.read(80);break
  at+=(size+2047)//2048*2048
 else:raise AssertionError(name)
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def put(a,*v):u.mem_write(a,w(*v))
put(b+0x2a0,b,-1,-1);put(b+0x828,-1);put(0x5a3ed8,1000);put(b+0xd014,1)
put(b+0x294,b+0x4000) # Class owner is valid; player association remains NULL.
put(b+0x29c,b+0x4000);put(b+0x4124,-1);put(b+0x416c,0,0);put(b+0x808,-1)
u.mem_write(b+0x34,struct.pack('<f',100));put(0x636ef8,1)
put(0x63011c,len(sample_names),b+0x5000);put(b+0x5000,*range(len(sample_names)))
for i in range(45):put(b+0xa54+i*16,-1)
put(b+0xa54+22*16,0);put(b+0xa54+23*16,0)
put(b+0x80,b+0x6000);put(b+0x6000,2,b+0x6100);put(b+0x6100+0x1d50,b+0x9000)
put(b+0x9000+0xf5c,b+0xb000);put(b+0xb078,b+0xc000);u.mem_write(b+0xc000,header)
starts=[];draws=0;plays=[]
def boundary(m,address,size,ctx):
 global draws
 if address not in (0x428d10,0x428c90,0x577eef,0x505c00,0x48acf0,0x5056a0):return
 sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<6I',m.mem_read(sp,24));value=0
 if address==0x428c90:starts.append(a[2:6]);assert a[2:6]==(22,0x3f800000,0,1)
 if address==0x577eef:value=b+0xd000;draws+=1
 if address==0x505c00:assert a[1]==0xffffffff
 if address==0x5056a0:
  assert a[3]==0x3f800000 and a[5]==0;plays.append(a[1])
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,boundary);u.reg_write(UC_X86_REG_FPCW,0x27f)
expected=[]
for hit in range(2):
 put(stack,stop,b);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x428740,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 put(stack,stop,b,0x3dcccccd);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4196f0,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 expected.extend(struct.unpack('<I',u.mem_read(b+o,4))[0] for o in (0x514,0x744,0x830,0x828,0xd014))
 row=sound_observed[hit*5:hit*5+5]
 assert row[:2]==list(struct.unpack('<I',u.mem_read(b+o,4))[0] for o in (0x1458,0x808))
 assert row[3:]==[expected[-1],len(plays)] and row[2]==audio[5]
assert observed==expected,(observed,expected)
assert len(starts)==len(plays)==1 and draws==2
sample_name=sample_names[plays[0]];name_hash=2166136261
for byte in sample_name.lower():name_hash=((name_hash^byte)*16777619)&0xffffffff
assert audio[:4]==[2,1,1,1] and audio[7:]==[0,name_hash],(audio,sample_name)
report=dict(result='PASS',pain_words=expected,motion=name,tick_span=struct.unpack_from('<2i',header,16),starts=len(starts),rng_draws=draws,
 sample=sample_name.decode(),sound_words=sound_observed,
 scope='Original428740 followed by4196f0/434da0/48a9c0, real timers/RNG and loaded motion duration. Installed five-sample Foley group with ordinal IDs. Active-fire/action-start, nonplayer routing and terminal voice device boundaries supplied; CRT TLS supplied. Both hits match PC deadlines/action/RNG/voice retention and selected asset name hash. Second hit suppressed by both cooldowns. Does not prove original loader IDs, device waveform fidelity, AI or armed behavior.')
(folder/'pain-report.json').write_text(json.dumps(report,indent=2));print(report)
