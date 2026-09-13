"""Audit authored rotation scalars and retained class values on PC/NXDK."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
B=0x30000000;STACK=B+0x1e0000;STOP=STACK+0x1000;OUT=STOP+0x100;NAME=OUT+0x100
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
bits=lambda v:struct.unpack('<I',struct.pack('<f',float(v)))[0]
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image()
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im);x.mem_map(B,0x200000)
entry=int(re.search(r'\s_rf_entity_rotation_values_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
path=root/'artifacts/rotation-reader.tbl';cases=0
def check(raw,name,status,values=None):
 global cases
 expected=w(status)+ (w(*(bits(v) for v in values)) if values is not None else b'\xa5'*8)
 path.write_bytes(raw)
 actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--rotation-values',str(path),name])
 assert actual==expected,('PC',raw,actual.hex(),expected.hex())
 x.mem_write(B,raw);x.mem_write(NAME,name.encode()+b'\0');x.mem_write(OUT,b'\xa5'*8);x.mem_write(STACK,w(STOP,B,len(raw),NAME,OUT));x.reg_write(UC_X86_REG_ESP,STACK)
 x.emu_start(entry,STOP,count=10000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,8))==expected,('NXDK',raw)
 cases+=1
for a,b in [('0','0'),('-1','2'),('0.125','1.2345'),('2e-3','-0.0009')]:
 check(f'$Name: "test"\n$Max Rot Vel: {a}\n$Rot Acceleration: {b}\n$Name: "other"\n$Max Rot Vel: 99\n'.encode(),'test',0,(a,b))
check(b'$name: "TEST"\n// $Max Rot Vel: 999\n"$Rot Acceleration:"\n$max rot vel: 2\n$rot acceleration: 3\n','test',0,(2,3))
check(b'$Name: "other"\n','absent',-3)
for body in (b'',b'$Max Rot Vel: 1',b'$Rot Acceleration: 1',b'$Max Rot Vel: 1\n$Max Rot Vel: 2\n$Rot Acceleration: 3',b'$Max Rot Vel: 1\n$Rot Acceleration: 2\n$Rot Acceleration: 3'):
 check(b'$Name: "test"\n'+body+b'\n','test',-2)
for tag,other in [(b'$Max Rot Vel:',b'$Rot Acceleration:'),(b'$Rot Acceleration:',b'$Max Rot Vel:')]:
 for value in (b'bad',b'',b'1e99',b'nan',b'inf'):
  check(b'$Name: "test"\n'+other+b' 1\n'+tag+b' '+value+b'\n','test',-2)
files=json.loads((root/'artifacts/inventory.json').read_text())['files']
table=next(e for a in files if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(table['offset']);raw=f.read(table['size'])
blocks=re.split(rb'(?m)^\s*\$Name:\s*"([^"]+)"',raw);authored={}
for i in range(1,len(blocks),2):
 name=blocks[i].decode('cp1252');values=[]
 for tag in (rb'Max\s+Rot\s+Vel:',rb'Rot\s+Acceleration:'):
  matches=re.findall(rb'(?m)^\s*\$'+tag+rb'\s*([^\s/]+)',blocks[i+1]);assert len(matches)==1
  values.append(float(matches[0]))
 check(b'$Name: "'+blocks[i]+b'"\n'+blocks[i+1],name,0,values);authored[name.lower()]=values
levels=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 output=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--seeds',str(root/'Installed_Game/levels1.vpp'),str(root/'Installed_Game/tables.vpp'),level],text=True)
 rows=[line.split('\t') for line in output.splitlines() if line.startswith('SEED_ROTATION\t')];assert rows
 for _,name,a,b in rows:assert [int(a),int(b)]==[bits(v) for v in authored[name.lower()]],(level,name,a,b)
 levels.append(dict(level=level,classes=len(rows),added_bytes=8*len(rows),summary=next(line for line in output.splitlines() if line.startswith('SEEDS '))))
report=dict(result='PASS',pc_nxdk_cases=cases,installed_classes=len(authored),authored=authored,levels=levels,scope='Independent installed declaration audit and shared compiled PC/NXDK reader; three PC levels retain values after archive closure with exact/undersized budget checks. No live steering or native XEMU claim.')
(root/'artifacts/rotation-reader.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!='authored'}))
