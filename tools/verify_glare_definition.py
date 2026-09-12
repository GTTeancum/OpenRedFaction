"""Glare authored metadata on PC/NXDK versus independent table extraction."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x200000);N=B+0x100000;O=N+0x1000;S=N+0x2000;STOP=N+0x3000
entry=int(re.search(r'\s_rf_glare_definition_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v));cases=0
path=root/'artifacts/glare-definition-input.tbl'
def check(raw,name,want):
 global cases
 path.write_bytes(raw);pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--glare-definition',str(path),name]);assert len(pc)==304
 x.mem_write(B,raw);x.mem_write(N,name.encode()+b'\0');x.mem_write(O,b'\xa5'*300);x.mem_write(S,w(STOP,B,len(raw),N,O));x.reg_write(UC_X86_REG_ESP,S)
 x.emu_start(entry,STOP,count=10000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(O,300));assert actual==pc,(name,'NXDK')
 if want is None:assert actual[:4]!=w(0) and actual[4:]==b'\xa5'*300
 else:assert actual==w(0)+want,(name,actual[:4].hex())
 cases+=1
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];e=next(e for a in inv if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='effects.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'))
section=clean.split('#Glares',1)[1].split('#End',1)[0];matches=list(re.finditer(r'\$Name:\s*"([^"]*)"',section));seen=set()
for i,m in enumerate(matches):
 name=m[1]
 if name in seen:continue
 seen.add(name);block=section[m.end():matches[i+1].start() if i+1<len(matches) else len(section)]
 def text(key):
  v=re.search(r'\$'+re.escape(key)+r':\s*"([^"]*)"',block);return v[1] if v else ''
 def number(key):
  v=re.search(r'\$'+re.escape(key)+r':\s*([^\s]+)',block);return float(v[1]) if v else 0
 names=[name]+[text(k) for k in ('Corona Bitmap','Volumetric Bitmap','Reflection Bitmap')]
 colors=list(map(int,re.search(r'\$Light Color:\s*\{([^}]+)',block)[1].split(',')))
 floats=[number(k) for k in ('Cone Angle','Intensity','Radius Distance Factor','Radius Scale Factor','Diminish Distance','Volumetric Height','Volumetric Length')]
 fields=sum(1<<j for j,k in enumerate(('Corona Bitmap','Volumetric Bitmap','Reflection Bitmap')) if '$'+k+':' in block)
 want=b''.join(n.encode('cp1252').ljust(64,b'\0') for n in names)+w(*colors)+struct.pack('<7f',*floats)+w(fields)
 check(raw,name,want)
base=b'#Glares\n$Name: "test"\n$Light Color: {1,2,3}\n#End\n';want=b'test'.ljust(256,b'\0')+w(1,2,3)+bytes(32)
check(base,'test',want);check(base,'TEST',None)
for replacement in (b'{256,2,3}',b'{-1,2,3}',b'{1,2}',b'{1,2,3,4}'):
 check(base.replace(b'{1,2,3}',replacement),'test',None)
check(base.replace(b'#End',b'$Corona Bitmap: "x"\n#End'),'test',None)
check(base.replace(b'#End',b'$Volumetric Bitmap: "x"\n#End'),'test',None)
check(base.replace(b'#End',b'$Light Color: {1,2,3}\n#End'),'test',None)
report=dict(result='PASS',classes=len(seen),cases=cases,scope='Owned factory/render-facing metadata versus independent installed #Glares declarations, PC and actual compiled NXDK. Exact names, fields and finite authored values; absent optional blocks and malformed color/missing conditional fields/duplicate guards preserve output. No original full parser, bitmap ownership, degree conversion or native XEMU claim.')
(root/'artifacts/glare-definition.json').write_text(json.dumps(report,indent=2));print(report)
