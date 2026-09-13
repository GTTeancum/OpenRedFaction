"""Independent authored projectile filename/type audit versus PC/NXDK reader."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
inv=json.loads((root/'artifacts/inventory.json').read_text());e=next(e for a in inv['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='weapons.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as stream:stream.seek(e['offset']);authored=stream.read(e['size'])
clean=re.sub(rb'"[^"\r\n]*"|//[^\r\n]*',lambda m:b'' if m[0].startswith(b'//') else m[0],authored)
blocks=re.split(rb'\$Name:\s*"[^"\r\n]*"',clean)[1:];files=[]
for block in blocks:
 m=re.search(rb'\$V3D\s+Filename:\s*"([^"\r\n]*)"',block);files.append(m[1] if m else b'')
def kind(n):return 0 if len(n)<5 else 3 if n.lower().endswith(b'.vfx') else 1
def out(names):return b''.join(n.ljust(64,b'\0') for n in names).ljust(4096,b'\0')+w(*[kind(n) for n in names]).ljust(256,b'\0')+w(len(names))
def table(body):return b'#Primary Weapons\n$Name: "Test"\n'+body+b'\n#End\n#Secondary Weapons\n#End\n'
cases=[(authored,0,out(files))]
for name in [b'',b'x',b'.vfx',b'a.vfx',b'Rocket.VFX',b'model.vcm',b'a.vfx.v3d',b'noextension',b'x'*63]:
 cases.append((table(b'$V3D Filename: "'+name+b'"'),0,out([name])))
cases.append((table(b'// $V3D Filename: "wrong"\n$V3D Filename: "right.v3d"\n$3rd Person V3D: "ignored"'),0,out([b'right.v3d'])))
for body in [b'',b'$V3D Filename: "x"\n$V3D Filename: "y"',b'$V3D Filename: nope',b'$V3D Filename: "'+b'x'*64+b'"',b'$V3D Filename: "unterminated']:
 cases.append((table(body),None,b'\xa5'*4356))
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase;u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im)
B=0x30000000;S=B+0xff000;STOP=B+0xfff00;OUT=B+0x40000;u.mem_map(B,0x100000)
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def chk(c,a,size,data):
 sp=c.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',c.mem_read(sp,4))[0];c.reg_write(UC_X86_REG_ESP,sp+4-c.reg_read(UC_X86_REG_EAX));c.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,chk,begin=sym('_chkstk'),end=sym('_chkstk'))
expected=[]
for text,status,result in cases:
 u.mem_write(B,text);u.mem_write(OUT,b'\xa5'*4356);u.mem_write(S,w(STOP,B,len(text),OUT));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(sym('rf_projectile_model_catalog_read'),STOP,count=100000000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 actual=u.reg_read(UC_X86_REG_EAX);assert actual==0 if status==0 else actual!=0
 assert bytes(u.mem_read(OUT,4356))==result
 expected.append(w(actual)+result)
probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe');assert subprocess.check_output([probe,'--projectile-models'],input=b''.join(w(len(t))+t for t,_,_ in cases))==b''.join(expected)
for budget in [e['size']-1,e['size'],131072]:
 actual=subprocess.check_output([probe,'--projectile-model-load',str(root/'Installed_Game/tables.vpp'),str(budget)])
 assert actual==(w(-4)+b'\xa5'*4356 if budget<e['size'] else expected[0])
assets={e['name'].lower() for a in inv['files'] for e in a.get('vpp',{}).get('entries',[])}
missing=[n.decode() for n in files if kind(n) and (n.decode().lower() if kind(n)==3 else str(Path(n.decode()).with_suffix('.v3m')).lower()) not in assets];assert not missing,missing
# Execute original extension search and case-insensitive comparison without
# hooks; the surrounding4c2c80 length gate is read directly from its branch.
import hashlib
original=root/'Installed_Game/RF.exe'
assert hashlib.sha256(original.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
op=pefile.PE(str(original));oi=op.get_memory_mapped_image();o=Uc(UC_ARCH_X86,UC_MODE_32)
o.mem_map(0x400000,(len(oi)+4095)//4096*4096);o.mem_write(0x400000,oi);o.mem_map(B,0x100000)
def original_call(address,args):
 o.mem_write(S,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,S);o.emu_start(address,STOP,count=1000000)
 assert o.reg_read(UC_X86_REG_EIP)==STOP;return o.reg_read(UC_X86_REG_EAX)
original_names=files+[b'Rocket.VFX',b'model.vcm',b'a.vfx.v3d',b'noextension',b'.vfx',b'A.VfX',b'dir.vfx/name']
for name in original_names:
 o.mem_write(B,name+b'\0')
 if len(name)<5:actual_kind=0
 else:
  extension=original_call(0x5143f0,[B]);equal=original_call(0x57c130,[extension,0x5a28a8])==0
  actual_kind=3 if equal else 1
 assert actual_kind==kind(name),(name,actual_kind,kind(name))
report=dict(result='PASS',original_extension_cases=len(original_names),cases=len(cases),archive_budgets=3,weapons=len(files),static_models=sum(kind(n)==1 for n in files),effect_models=sum(kind(n)==3 for n in files),model_less=sum(kind(n)==0 for n in files),resident_bytes=4356,scratch_bytes=e['size'],files=[dict(name=n.decode(),kind=kind(n)) for n in files],scope='Independent comment-aware installed-table audit and exact PC/NXDK parser/failure outputs; PC archive budget boundaries; installed static/effect asset existence. Original4c2c80 stores filename+10 and kind+18, length<5 kind0,5143f0 final extension and5001d0 comparison to .vfx select3 else1. Original extension/comparison helpers executed unhooked; full original parser, resource loading, rendering and native XEMU not executed.')
(root/'artifacts/projectile-model-catalog.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='files'})
