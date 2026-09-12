"""Clutter skin +Glare metadata: independent table extraction, PC and NXDK."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x200000);C=B+0x100000;N=C+0x100;A=C+0x1000;G=A+0x2000;P=G+0x100;S=C+0xe000;STOP=S+0x1000
entry=int(re.search(r'\s_rf_clutter_skin_assets_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
probe=int(re.search(r'\s__chkstk\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def stack_probe(cpu,address,length,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',cpu.mem_read(sp,4))[0]
 cpu.reg_write(UC_X86_REG_ESP,sp+4-cpu.reg_read(UC_X86_REG_EAX));cpu.reg_write(UC_X86_REG_EIP,ret)
# Supply only stack growth on an already mapped harness stack.
x.hook_add(UC_HOOK_CODE,stack_probe,begin=probe,end=probe)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v));cases=overrides=0
path=root/'artifacts/clutter-skin-glare-input.tbl';probe=root/'build/pc/Release/rf_entity_assets_probe.exe'
def check(raw,cls,skin,want):
 global cases,overrides
 path.write_bytes(raw);pc=subprocess.check_output([str(probe),'--clutter-skin-assets',str(path),cls,skin]);assert len(pc)==4236
 x.mem_write(B,raw);x.mem_write(C,cls.encode('cp1252')+b'\0');x.mem_write(N,skin.encode('cp1252')+b'\0')
 x.mem_write(A,b'\xa5'*4164);x.mem_write(G,b'\xa5'*64);x.mem_write(P,w(0xa5a5a5a5));x.mem_write(S,w(STOP,B,len(raw),C,N,A,G,P));x.reg_write(UC_X86_REG_ESP,S)
 x.emu_start(entry,STOP,count=100000000);assert x.reg_read(UC_X86_REG_EIP)==STOP,(cls,skin,"instruction budget")
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(A,4164))+bytes(x.mem_read(G,64))+bytes(x.mem_read(P,4));assert actual==pc,(cls,skin,'NXDK')
 if want is None:assert pc[:4]!=w(0) and pc[4:]==b'\xa5'*4232
 else:
  assert pc==w(0)+want,(cls,skin,'metadata');overrides+=struct.unpack_from('<I',want,4228)[0]
 cases+=1

def expected(model,textures,glare=None):
 assert len(textures)<=64
 return model.encode().ljust(64,b'\0')+b''.join(t.encode().ljust(64,b'\0') for t in textures).ljust(4096,b'\0')+w(len(textures))+(glare or '').encode().ljust(64,b'\0')+w(glare is not None)
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];e=next(e for a in inv if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='clutter.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'))
classes=list(re.finditer(r'(?i)\$Class\s+Name:\s*"([^"]*)"',clean));seen=set();authored_overrides=[]
for i,m in enumerate(classes):
 cls=m[1]
 if cls.lower() in seen:continue
 seen.add(cls.lower());block=clean[m.end():classes[i+1].start() if i+1<len(classes) else len(clean)]
 model=re.search(r'(?i)\$V3D\s+Filename:\s*"([^"]*)"',block)[1]
 check(raw,cls,'',expected(model,[]));skins=set()
 for skin in re.finditer(r'(?i)\$Skin:\s*"([^"]*)"\s*\(([^)]*)\)\s*(?:\+Glare:\s*"([^"]*)")?',block):
  name=skin[1]
  if name.lower() in skins:continue
  skins.add(name.lower());textures=re.findall(r'"([^"]*)"',skin[2]);glare=skin[3]
  check(raw,cls.swapcase(),name.swapcase(),expected(model,textures,glare))
  if glare is not None:authored_overrides.append(dict(class_name=cls,skin=name,glare=glare))
base=b'$Class Name: "test"\n$V3D Filename: "x.v3d"\n$Skin: "skin" ("one")\n'
check(base,'test','skin',expected('x.v3d',['one']))
check(base+b'+Glare: "good"\n','test','skin',expected('x.v3d',['one'],'good'))
check(base+b'// comment\n+Glare: ""\n','test','skin',expected('x.v3d',['one'],''))
check(base+b'+Glare: "first"\n$Skin: "SKIN" ("two") +Glare: "second"\n','test','skin',expected('x.v3d',['one'],'first'))
check(base+b'$Skin: "other" () +Glare: "other"\n','test','skin',expected('x.v3d',['one']))
for bad in (base+b'+Glare: unquoted\n',base+b'+Glare: "'+b'x'*64+b'"\n',base.replace(b'("one")',b'("one"')):check(bad,'test','skin',None)
check(base,'test','missing',None);check(base,'missing','skin',None)
report=dict(result='PASS',cases=cases,classes=len(seen),authored_overrides=authored_overrides,scope='Independent first class/skin table extraction versus PC and actual compiled NXDK parser with supplied stack growth, all model/replacement/glare bytes, absent/empty/duplicate/comment/malformed cases and failure output preservation. No original full parser or live skin mutation claim.')
(root/'artifacts/clutter-skin-glare.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='authored_overrides'});print('Authored overrides:',len(authored_overrides))
