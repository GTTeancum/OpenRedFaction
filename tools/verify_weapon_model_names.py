"""Independent authored third-person filename audit versus PC/NXDK reader."""
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
 m=re.search(rb'\$3rd\s+Person\s+V3D:\s*"([^"\r\n]*)"',block);files.append(m[1] if m else b'')
def out(names):return b''.join(n.ljust(64,b'\0') for n in names).ljust(4096,b'\0')+w(len(names))
def table(body):return b'#Primary Weapons\n$Name: "Test"\n'+body+b'\n#End\n#Secondary Weapons\n#End\n'
cases=[(authored,0,out(files)),(table(b''),0,out([b''])),(table(b'$3rd Person V3D: ""'),0,out([b''])),(table(b'$3rd Person V3D: "Mixed.V3D"\n$3rd Person Muzzle Flash Glare: "x"'),0,out([b'Mixed.V3D'])),(table(b'// $3rd Person V3D: "wrong"\n$3rd Person V3D: "right.v3d"'),0,out([b'right.v3d'])),(table(b'$3rd Person V3D: "'+b'x'*63+b'"'),0,out([b'x'*63]))]
for body in [b'$3rd Person V3D: "x"\n$3rd Person V3D: "y"',b'$3rd Person V3D: nope',b'$3rd Person V3D: "'+b'x'*64+b'"',b'$3rd Person V3D: "unterminated']:
 cases.append((table(body),None,b'\xa5'*4100))
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase;u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im)
B=0x30000000;S=B+0xff000;STOP=B+0xfff00;OUT=B+0x40000;u.mem_map(B,0x100000)
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def chk(c,a,size,data):
 sp=c.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',c.mem_read(sp,4))[0];c.reg_write(UC_X86_REG_ESP,sp+4-c.reg_read(UC_X86_REG_EAX));c.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,chk,begin=sym('_chkstk'),end=sym('_chkstk'))
expected=[]
for text,status,result in cases:
 u.mem_write(B,text);u.mem_write(OUT,b'\xa5'*4100);u.mem_write(S,w(STOP,B,len(text),OUT));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(sym('rf_weapon_model_names_read'),STOP,count=100000000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 actual=u.reg_read(UC_X86_REG_EAX);assert actual==0 if status==0 else actual!=0
 assert bytes(u.mem_read(OUT,4100))==result
 expected.append(w(actual)+result)
probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe');assert subprocess.check_output([probe,'--weapon-model-names'],input=b''.join(w(len(t))+t for t,_,_ in cases))==b''.join(expected)
for budget in [e['size']-1,e['size'],131072]:
 actual=subprocess.check_output([probe,'--weapon-model-load',str(root/'Installed_Game/tables.vpp'),str(budget)])
 assert actual==(w(-4)+b'\xa5'*4100 if budget<e['size'] else expected[0])
assets={e['name'].lower() for a in inv['files'] for e in a.get('vpp',{}).get('entries',[])}
missing=[s.decode() for s in files if s and str(Path(s.decode()).with_suffix('.v3m')).lower() not in assets];assert not missing,missing
report=dict(result='PASS',cases=len(cases),archive_budgets=3,weapons=len(files),nonempty=sum(bool(s) for s in files),unique_models=len(set(s.lower() for s in files if s)),resident_bytes=4100,scratch_bytes=e['size'],scope='Independent comment-aware installed-table audit, exact PC/NXDK parser outputs and failure preservation, PC archive budget boundaries and installed compiled .v3m asset existence. Original4c3007..4c3024 field/store semantics traced; full original parser and model loading/rendering not executed.')
(root/'artifacts/weapon-model-names.json').write_text(json.dumps(report,indent=2));print(report)
