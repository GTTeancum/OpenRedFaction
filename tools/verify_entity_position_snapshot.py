"""Original frame position snapshots and complete list traversal."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);nx=machine(root/'build/xbox/main.exe');rng=random.Random(0x487b11)
entry=int(re.search(r'\s_rf_entity_position_snapshot\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=[];expected=[];objects=0
# Execute the complete original snapshot-list traversal, excluding type8's
# separate update469770. Every object's physics position deliberately differs.
for trial in range(128):
 count=trial%17;addresses=[base+0x100+i*0x300 for i in range(count)]
 before=[]
 for i,a in enumerate(addresses):
  raw=pack(*(rng.getrandbits(32) for _ in range(7)));cases.append(raw)
  obj=bytearray(rng.randbytes(0x280));struct.pack_into('<I',obj,0x10,addresses[i+1] if i+1<count else 0x73d880)
  struct.pack_into('<I',obj,0x24,i%8);obj[0x7c:0x80]=raw[:4];obj[0x6c:0x78]=raw[4:16];obj[0x3c:0x48]=raw[16:]
  u.mem_write(a,bytes(obj));before.append(bytes(obj))
  nx.mem_write(base,raw);nx.mem_write(stack,pack(stop,base,base+4,base+16));nx.reg_write(UC_X86_REG_ESP,stack);nx.emu_start(entry,stop,count=1000)
  assert nx.reg_read(UC_X86_REG_EIP)==stop
  want=pack(struct.unpack('<I',raw[:4])[0]&~0x01000000)+raw[16:]+raw[16:];expected.append(want)
  assert bytes(nx.mem_read(base,28))==want
 u.reg_write(UC_X86_REG_ESI,addresses[0] if count else 0x73d880);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x487aff,0x487b45,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==0x487b45 and u.reg_read(UC_X86_REG_ESP)==stack
 assert u.reg_read(UC_X86_REG_ESI)==0x73d880
 for a,old in zip(addresses,before):
  want=bytearray(old);want[0x6c:0x78]=old[0x3c:0x48]
  struct.pack_into('<I',want,0x7c,struct.unpack_from('<I',old,0x7c)[0]&~0x01000000)
  assert bytes(u.mem_read(a,len(old)))==bytes(want)
 objects+=count
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--position-snapshot'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',lists=128,objects=objects,original_sha256=sha,scope='Unmodified487aff..487b45 list traversal and409f40/409f70 copies; types0..7, empty lists, arbitrary position bits. Every object byte checked; only previous position and flag01000000 change. PC/NXDK snapshot exact. Type8 update469770 and remaining frame phases excluded; no live NPC integration claimed.')
(root/'artifacts/entity-position-snapshot.json').write_text(json.dumps(report,indent=2));print(report)
