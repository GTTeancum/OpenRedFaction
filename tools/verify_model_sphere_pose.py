"""Original animated sphere query versus shared placement and real miner poses."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;desc=base+0x4000;handle=base+0x5000;container=base+0x6000;source=base+0x9000;output=base+0xa000;stack=base+0xe000;stop=base+0xf000
u.mem_map(base,0x10000);rng=random.Random(0x503270);commands=[];expected=[];real=[]
for i in range(300):
 count=20;parent=(-1,0,8,15,19)[i%5]
 sphere=b'csphere_test'+bytes(12)+struct.pack('<i4f',parent,*[rng.uniform(-10,10) for _ in range(3)],rng.uniform(0,5))
 matrices=floats(*[rng.uniform(-2,2) for _ in range(count*12)])
 commands.append(sphere+pack(count)+matrices)
miner=next(r for r in json.loads((root/'artifacts/model-spheres-verification.json').read_text())['records'] if r['model'].lower()=='miner.v3c')
for motion in ('ult2_stand.rfa','ult2_crouch.rfa'):
 ticks=(0,200,4000)
 poses=subprocess.check_output([str(root/'build/pc/Release/rf_skeleton_probe.exe'),str(root/'Installed_Game/meshes.vpp'),str(root/'Installed_Game/motions.vpp'),'miner.v3c',motion],input=struct.pack('<3i',*ticks))
 stride=len(poses)//len(ticks);count=stride//48-1;assert stride==(count+1)*48
 for frame,tick in enumerate(ticks):
  matrices=poses[frame*stride:frame*stride+count*48]
  for s in miner['spheres']:
   sphere=s['name'].encode().ljust(24,bytes(1))+struct.pack('<i4f',s['parent'],*s['center'],s['radius'])
   real.append(dict(case=len(commands),motion=motion,tick=tick,name=s['name']));commands.append(sphere+pack(count)+matrices)
for i,command in enumerate(commands):
 count=struct.unpack_from('<I',command,44)[0];u.mem_write(base,bytes(0x3000));u.mem_write(base,command[48:]);u.mem_write(base+0x1d50,pack(desc));u.mem_write(desc+0x48,pack(count))
 u.mem_write(handle,pack(2,base,container));u.mem_write(container+0x19c0+0x60,pack(1,source));u.mem_write(source,command[:44]);u.mem_write(output,bytes([0xa5])*16)
 u.mem_write(stack,pack(stop,handle,0,output,output+12));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x503270,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)&255==1
 expected.append(bytes(u.mem_read(output,16))+pack(0))
for r in real:r['center_radius']=struct.unpack('<4f',expected[r['case']][:16])
original_cases=len(commands);guards=[]
valid=bytearray(commands[1])
for parent in (-2,20):
 changed=bytearray(valid);changed[24:28]=struct.pack('<i',parent);guards.append(changed)
changed=bytearray(valid);changed[40:44]=floats(-1);guards.append(changed)
for offset in list(range(28,44,4))+list(range(48,96,4)):
 for bad in (float('inf'),float('nan')):
  changed=bytearray(valid);changed[offset:offset+4]=floats(bad);guards.append(changed)
for command in guards:commands.append(bytes(command));expected.append(bytes([0xa5])*16+pack(0xfffffffc))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--sphere-pose'],input=b''.join(commands));assert actual==b''.join(expected),'PC sphere pose mismatch'
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_model_collision_sphere_pose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,(command,want) in enumerate(zip(commands,expected)):
 count=struct.unpack_from('<I',command,44)[0];x.mem_write(source,command[:24]+bytes(4)+command[24:44]);x.mem_write(base,command[48:]);x.mem_write(output,bytes([0xa5])*16)
 x.mem_write(stack,pack(stop,source,base,count,output));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert bytes(x.mem_read(output,16))+pack(x.reg_read(UC_X86_REG_EAX))==want,('NXDK',i)
report=dict(result='PASS',original_cases=original_cases,guard_cases=len(guards),real_miner_cases=real,scope='Complete original 503270 animated-kind query with valid cached bone matrices and identity parent -1, no hooks. Shared PC/NXDK bytes match; 18 real miner sphere queries use shared standing/crouching sampled bone poses. Port guards preserve output. Does not verify uncached evaluation, virtual/attachment parents, class overrides or live Xbox entity integration.')
(root/'artifacts/model-sphere-pose-verification.json').write_text(json.dumps(report,indent=2));print('PASS',len(commands),'cases;',len(real),'real miner sphere queries')
