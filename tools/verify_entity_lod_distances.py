"""Installed LOD declarations and retained seed ownership versus PC/NXDK."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
inv=json.loads((root/'artifacts/inventory.json').read_text())['files']
a=next(x for x in inv if x['path']=='tables.vpp');e=next(x for x in a['vpp']['entries'] if x['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
clean=re.sub(rb'//[^\r\n]*',b'',raw);parts=re.split(rb'\$Name:\s*"([^"\r\n]*)"',clean,flags=re.I)
classes={};cases=[]
def output(values):
 values=values[:4];return struct.pack('<iI4f',0,len(values),*(values+[0.]*(4-len(values))))
for name,body in zip(parts[1::2],parts[2::2]):
 match=re.search(rb'\$LOD\s+Distances:\s*\{([^}]*)\}',body,re.I)
 values=[float(x) for x in match[1].split()] if match else []
 classes[name.decode().lower()]=values[:4];cases.append((raw,name.swapcase(),output(values)))
for count in range(9):
 values=[float(i*13-7) for i in range(count)];text=b'$Name: "fixture"\n$LOD Distances: {'+b' '.join(str(v).encode() for v in values)+b'}\n#End'
 cases.append((text,b'fixture',output(values)))
for text,code in [(b'$Name: "fixture"\n',0),(b'$Name: "other"\n',-3),(b'$Name: "fixture"\n$LOD Distances: { 1',-2),(b'$Name: "fixture"\n$LOD Distances: {1} $LOD Distances: {2}',-2),(b'$Name: "fixture"\n$LOD Distances: { "1" }',-2)]:
 cases.append((text,b'fixture',output([]) if code==0 else struct.pack('<i',code)+b'\xa5'*20))
wire=b''.join(struct.pack('<I64s',len(text),name)+text for text,name,_ in cases)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--lod-distances'],input=wire)
expected=b''.join(want for _,_,want in cases);assert pc==expected, next((k for k in range(len(cases)) if pc[k*24:k*24+24]!=expected[k*24:k*24+24]),None)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase;u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im)
base=0x30000000;u.mem_map(base,0x200000);name_ptr=base+0x110000;out=name_ptr+256;stack=base+0x180000;stop=base+0x1f0000
entry=int(re.search(r'\s_rf_entity_lod_distances_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for k,(text,name,want) in enumerate(cases):
 u.mem_write(base,text);u.mem_write(name_ptr,name+b'\0');u.mem_write(out,b'\xa5'*20);u.mem_write(stack,struct.pack('<5I',stop,base,len(text),name_ptr,out));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(entry,stop,count=50000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=struct.pack('<I',u.reg_read(UC_X86_REG_EAX))+bytes(u.mem_read(out,20));assert result==want,('NXDK',k,result.hex(),want.hex())
levels=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 text=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--seeds',str(root/'Installed_Game/levels1.vpp'),str(root/'Installed_Game/tables.vpp'),level],text=True)
 rows=[s.split('\t') for s in text.splitlines() if s.startswith('SEED_LOD\t')]
 for _,name,count,*values in rows:
  expected=classes[name.lower()];assert int(count)==len(expected) and list(map(float,values))==expected+[0.]*(4-len(expected))
 levels.append(dict(level=level,classes=len(rows),added_resident_bytes=len(rows)*20,summary=next(s for s in text.splitlines() if s.startswith('SEEDS '))))
report=dict(result='PASS',installed_classes=len(classes),reader_cases=len(cases),levels=levels,scope='Independent installed text inventory and malformed/surplus fixtures versus PC/NXDK reader. PC seed probe checks exact/short budgets and retained values after archive closure. Original class-loop count cap is source evidence; no full original parser equivalence or native XEMU execution claimed.')
(root/'artifacts/entity-lod-distances.json').write_text(json.dumps(report,indent=2));print(report)
