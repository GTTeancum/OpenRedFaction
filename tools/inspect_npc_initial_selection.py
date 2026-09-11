"""Opening-level NPC selector fixture with actual class modes and mappings.
Not complete actor construction: fixture field assumptions are recorded explicitly.
"""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
entity=0x30000000;info=entity+0x3000;mode=entity+0x5000;stack=entity+0xe000;stop=entity+0xf000
u.mem_map(entity,0x10000);u.mem_map(0,0x10000)
def put(address,fmt,*values):u.mem_write(address,struct.pack(fmt,*values))
def call(address,end=stop):
 u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(address,end,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==end
reads=[]
u.hook_add(UC_HOOK_MEM_READ,lambda uc,access,address,size,value,data:reads.append((address-entity,size)),begin=entity,end=entity+0x1493)
assets=str(root/'build/pc/Release/rf_entity_assets_probe.exe');motion=str(root/'build/pc/Release/rf_motion_probe.exe')
entity_probe=str(root/'build/pc/Release/rf_entity_probe.exe');game=root/'Installed_Game'
defaults={r['entity_class'].lower():r for r in json.loads((root/'artifacts/entity-default-weapons.json').read_text())['rows']}
initial=struct.pack('<iiffiI',0,-1,0,0,0,0);results=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 out=subprocess.check_output([assets,'--catalog',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'motions.vpp'),str(game/'meshes.vpp'),level],text=True)
 for line in out.splitlines():
  if not line.startswith('CATALOG_MAP\t'):continue
  fields=line.split('\t');cls,weapon=fields[1:3];skeleton=int(fields[3])
  if weapon or skeleton==0xffffffff:continue
  mapping=list(map(int,fields[4:27]));actions=list(map(int,fields[27:72]))
  config=bytes.fromhex(subprocess.check_output([assets,'--class-physics',str(game/'tables.vpp'),cls],text=True).strip())
  flags,flags2,mode_id=struct.unpack_from('<3I',config,68)
  physics=struct.unpack('<I',subprocess.check_output([entity_probe,'--physics-flags'],input=struct.pack('<5I',0,flags,flags2,0,0)))[0]
  primary,secondary=[defaults[cls.lower()][k] for k in ('primary','secondary')]
  for prior in (0,-1,123456):
   u.mem_write(entity,bytes(0xd000));reads.clear()
   put(entity+0x24,'<I',0);put(entity+0x2c,'<i',5);put(entity+0x200,'<i',-1)
   put(entity+0x294,'<I',info);put(entity+0x29c,'<I',info);put(info+0x94,'<I',2)
   put(info+0x724,'<II',flags,flags2);put(entity+0x858,'<I',mode);put(mode+4,'<i',mode_id)
   put(entity+0x1a8,'<I',physics);put(entity+0x2a0,'<I',entity);put(entity+0x2a4,'<ii',primary,secondary)
   # Execute the recovered scalar initializer span, not fabricated action/behavior values.
   put(entity+0x520,'<I',0xa5a5a5a5);put(entity+0x554,'<I',0xa5a5a5a5)
   u.reg_write(UC_X86_REG_ESI,entity+0x2a0);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_ESP,stack)
   call(0x402d68,0x402dad)
   put(entity+0x834,'<i',-1);put(entity+0x1380,'<i',prior);u.mem_write(entity+0x138c,initial[:16])
   for i,value in enumerate(mapping):put(entity+0x8e4+16*i,'<i',value)
   for i,value in enumerate(actions):put(entity+0xa54+16*i,'<i',value)
   put(0x7c75cc,'<I',0);u.mem_write(0x64ecb9,b'\0');u.mem_write(0x6fc4d8,b'\0')
   put(stack,'<II',stop,entity);u.reg_write(UC_X86_REG_ESP,stack);call(0x41f400)
   actual=bytes(u.mem_read(entity+0x138c,16))+initial[16:]
   priority=struct.pack('<10i3fI',-1,0,physics if physics<0x80000000 else physics-0x100000000,mode_id,0,prior,0,0,0,5,0,0,0,0)
   selected=subprocess.check_output([motion,'--priority'],input=initial+struct.pack('<23i',*mapping)+priority)
   status,handled=struct.unpack_from('<2i',selected);assert status==0
   expected=selected[8:]
   if not handled:
    movement=struct.pack('<3f5i',0,0,0,mode_id,0,0,2,4)
    selected=subprocess.check_output([motion,'--movement'],input=expected+struct.pack('<23i',*mapping)+movement)
    assert selected[:4]==bytes(4);expected=selected[4:]
   assert actual==expected,(level,cls,prior,actual.hex(),expected.hex())
   results.append(dict(level=level,entity_class=cls,mode=mode_id,primary=primary,secondary=secondary,prior_action=prior,
    prior_action_reads=sum(a<=0x1380<a+n for a,n in reads),controller=list(struct.unpack('<iiff',actual[:16])),read_fields=sorted({hex(a) for a,n in reads})))
report=dict(result='PASS',cases=len(results),original_sha256=digest,scope='Full original41f400 selector versus PC priority/movement composition using installed base maps, class flags/movement modes and default weapon IDs. Original402d68 scalar span executes. Fixture zeroes unspecified actor state, uses kind0/no links/nonplayer, zero velocity and no external events; not full factory or first pose/weight update. Field1380 sensitivity is tested, not assumed.',results=results)
(root/'artifacts/npc-initial-selection.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
