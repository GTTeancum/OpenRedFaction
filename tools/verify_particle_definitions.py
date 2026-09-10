"""Validate bounded particle metadata against authored fields, on PC and NXDK.
This is an independent table-value oracle, not original-parser equivalence.
"""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
j=json.loads((root/'artifacts/inventory.json').read_text());archive=(root/'Installed_Game/tables.vpp').read_bytes()
fixtures=[];counts={}
for name in ('vclip.tbl','emitters.tbl'):
 e=next(e for a in j['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==name)
 text=archive[e['offset']:e['offset']+e['size']].decode('cp1252')
 # Preserve original comments in submitted blocks.
 blocks=re.findall(r'(?ims)^\s*\$pos:.*?(?=^\s*\$name:|^\s*#end|\Z)',text);counts[name]=len(blocks)
 for block in blocks:
  clean=re.sub(r'//[^\n]*','',block)
  fields={re.sub(r'[ _\t]','',k.lower()):v.strip() for k,v in re.findall(r'\$([^:\r\n]+):([^\r\n]*)',clean)}
  def num(k,default=0):return float(fields.get(k,default))
  def vector(k):return [float(v) for v in fields[k].strip('<>').split(',')]
  f=vector('pos')+vector('dir')+[num(k) for k in ('dirrand','minvel','maxvel','spawnradius','minspawndelay','maxspawndelay','minlifesecs','maxlifesecs','minpradius','maxpradius','growthrate','acceleration','gravityscale')]
  alternate=fields['alternatestates'].lower() in ('yes','true','1')
  f+=([num(k) for k in ('ontime','ontimevariance','offtime','offtimevariance')] if alternate else [1,0,1,0])
  emit=fields['emitterflags'].lower();part=fields['particleflags'].lower();flags=[0,0,0]
  for k,b in [('immediate',2),('continuous',4),('dirdepend',8),('dont_move_with_parent',64),('accel_with_parent',128)]:
   if k in emit:flags[0]|=b
  if fields['initiallyon'].lower() in ('yes','true','1'):flags[0]|=16
  if alternate:flags[0]|=32
  for k,b in [('glow',2),('clr_change',4),('gravity',8),('collide',16),('collide_liquid',1024),('collide_and_die',2048),('wind',0xf0000000),('accelerate',64),('loop',256),('explode',128),('random_orient',512),('vel_stretch',16384),('no_z_check',8192)]:
   if k in part:flags[1]|=b
  for k,b in [('damages',1),('hold_last_frame',4),('fire_damage',8)]:
   if k in part:flags[2]|=b
  for k,shift in [('bounciness',16),('stickiness',20),('swirliness',24)]:flags[1]|=(int(fields.get(k,0))&15)<<shift
  flags[2]|=(int(fields.get('damagefactor',0))&15)<<12
  color=bytes(int(v) for v in fields['particlecolor'].strip('{}').split(','))
  dest=bytes(int(v) for v in fields.get('particlecolordest',fields['particlecolor']).strip('{}').split(','))
  expected=struct.pack('<23f3I',*f,*flags)+fields['bitmap'].strip('"').encode().ljust(64,b'\0')+color+dest+struct.pack('<fI',num('agepcttofinishvbm'),'agepcttofinishvbm' in fields)
  assert len(expected)==184
  fixtures.append((block.encode('cp1252'),0,expected))
valid_count=len(fixtures);sample=fixtures[0][0]
# Safety boundaries: each required field omitted, duplicate, unknown, nonfinite,
# overflowing signed nibble, unterminated string and embedded NUL.
required=('pos','dir','dir_rand','min_vel','max_vel','spawn_radius','emitter_flags','initially_on','alternate_states','min_life_secs','max_life_secs','min_pradius','max_pradius','growth_rate','acceleration','gravity_scale','bitmap','particle color','particle flags')
bad=[re.sub(rb'(?im)^\s*\$'+k.encode()+rb':[^\n]*\n',b'',sample) for k in required]
bad += [sample+b'\n$pos: <0,0,0>',sample+b'\n$unknown: 1',sample+b'\n$age_pct_to_finish_vbm: nan',sample+b'\n$damage_factor: 2147483648',sample+b'\0',sample.replace(b'"brwnglass01_A.tga"',b'"unterminated')]
fixtures += [(b,-2,bytes([0xa5])*184) for b in bad]
p=pefile.PE(str(root/'build/xbox/main.exe'));image=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(image)+4095)//4096*4096);u.mem_write(origin,image)
base=0x30000000;u.mem_map(base,0x20000);stack=base+0x1e000;stop=base+0x1f000
entry=int(re.search(r'_rf_particle_definition_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
commands=bytearray();expected_all=bytearray()
for raw,status,expected in fixtures:
 commands.extend(struct.pack('<I',len(raw))+raw);expected_all.extend(struct.pack('<i',status)+expected)
 u.mem_write(base,bytes([0xa5])*184);u.mem_write(base+0x1000,raw);u.mem_write(stack,struct.pack('<4I',stop,base+0x1000,len(raw),base));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(entry,stop,count=2000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==status&0xffffffff
 assert bytes(u.mem_read(base,184))==expected,(raw[:100],bytes(u.mem_read(base,184)).hex(),expected.hex())
probe=root/'build/pc/Release/rf_effect_probe.exe';got=subprocess.check_output([str(probe),'--particle-definition'],input=commands)
assert got==expected_all
report=dict(result='PASS',authored=counts,valid=valid_count,invalid=len(bad),pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Authored metadata compared with independent Python field decoding, exact PC/NXDK bytes; invalid blocks preserve output. No original full-parser or resource/runtime equivalence claimed.')
(root/'artifacts/particle-definitions-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
