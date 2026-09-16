"""Original projectile liquid-effect bindings and owned asset definitions; no visual acceptance."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,65536)
j=json.loads((root/'artifacts/inventory.json').read_text());entry=next(e for a in j['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='vclip.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);table=f.read(entry['size'])
names=re.findall(rb'(?im)^\s*\$Name:\s*"([^"]*)"',table);assert len(names)==63
for i,name in enumerate(names+[b'']):
 u.mem_write(B+i*128,name+b'\0');u.mem_write(0x858cb8+i*224,struct.pack('<II',len(name),B+i*128))
u.reg_write(UC_X86_REG_ESP,B+0xe000);u.emu_start(0x4c1323,0x4c1369,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x4c1369
ids=[struct.unpack('<I',u.mem_read(a,4))[0] for a in (0x8568a8,0x8568b0)];assert ids==[45,42];assert [names[i] for i in ids]==[b'bullet_splash',b'water_splash_medium']
definitions=[];assets=[]
for name in [names[i].decode() for i in ids]:
 b=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--vclip-load',str(root/'Installed_Game/tables.vpp'),name,'65536']);assert len(b)==504 and b[:4]==bytes(4);b=b[4:];definitions.append(b)
 assets.append(dict(name=name,vfx=b[128:192].split(b'\0')[0].decode(),foley=b[224:288].split(b'\0')[0].decode(),radius=struct.unpack_from('<f',b,300)[0],flags=struct.unpack_from('<I',b,288)[0]))
h=2166136261
for v in b''.join(definitions):h=((h^v)*16777619)&0xffffffff
report=dict(result='PASS',ids=ids,definition_hash=h,assets=assets,scope='Original4c1323..4c1369 with actual4c1d00 and authored names; PC owned definitions. No splash rendering/playback validation.')
(root/'artifacts/projectile-liquid-assets-verification.json').write_text(json.dumps(report,indent=2));print(report)
