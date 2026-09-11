"""Independent authored class footstep inventory versus PC/NXDK reader."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];archive=next(a for a in inv if a['path']=='tables.vpp')
def table(name):
 e=next(e for e in archive['vpp']['entries'] if e['name']==name)
 with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
foley=table('foley.tbl');entity=table('entity.tbl')
def sections(raw):
 clean=re.sub(rb'//[^\r\n]*',b'',raw);parts=re.split(rb'\$Name:\s*"([^"\r\n]*)"',clean,flags=re.I)
 return list(zip(parts[1::2],parts[2::2]))
# Foley's forward search is case-sensitive; stop before lowercase collision groups.
clean=re.sub(rb'//[^\r\n]*',b'',foley);parts=re.split(rb'\$Name:\s*"([^"\r\n]*)"',clean)
materials=['default','rock','metal','flesh','water','laval','solid','sand','ice','glass','ladder','chain fence'];records=[];lookup={}
for index,(name,body) in enumerate(zip(parts[1::2],parts[2::2])):
 mat=re.search(rb'\$Material:\s*"([^"]*)"',body);mat=materials.index(mat[1].decode().lower()) if mat else 0;mat={10:1,11:2}.get(mat,mat)
 records.append(struct.pack('<32sIII',name,mat,0,0));lookup.setdefault(name.lower(),(index,mat))
classes=[];cases=[]
for name,body in sections(entity):
 names=re.findall(rb'\$Footstep\s+Sound:\s*"([^"\r\n]*)"',body,re.I);slots=[-1]*10
 for group in names:
  index,mat=lookup[group.lower()];slots[mat]=index
 classes.append(dict(name=name.decode(),declarations=len(names),slots=slots))
 cases.append((entity,name.swapcase(),struct.pack('<i10i',0,*slots)))
cases.append((entity,b'missing class',struct.pack('<i',-3)+bytes([0xa5])*40))
fixture=b'$Name: "test"\r\n// $Footstep Sound: "bad"\r\n$Footstep Sound: "Rock Footstep"\r\n$Footstep Sound: "Ladder Climb"\r\n$Name: "empty"\r\n#End'
slots=[-1]*10;slots[1]=lookup[b'ladder climb'][0]
cases.extend([(fixture,b'TEST',struct.pack('<i10i',0,*slots)),(fixture,b'empty',struct.pack('<i10i',0,*([-1]*10))),
 (fixture.replace(b'Ladder Climb',b'missing'),b'test',struct.pack('<i',-3)+bytes([0xa5])*40),
 (fixture.replace(b'"Rock Footstep"',b'"'+b'x'*32+b'"'),b'test',struct.pack('<i',-2)+bytes([0xa5])*40)])
fpath=root/'artifacts/foley-classes-foley.tbl';fpath.write_bytes(foley);epath=root/'artifacts/foley-classes-entity.tbl'
# Batch classes sharing source text for the PC probe.
for raw in dict.fromkeys(row[0] for row in cases):
 selected=[row for row in cases if row[0]==raw];epath.write_bytes(raw)
 output=subprocess.run([str(root/'build/pc/Release/rf_audio_probe.exe'),'--foley-classes',str(epath),str(fpath)],input=b''.join(name.ljust(64,b'\0') for _,name,_ in selected),capture_output=True,check=True).stdout
 assert output==b''.join(want for _,_,want in selected)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase;u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im)
base=0x30000000;u.mem_map(base,0x200000);owner=base+0x100000;groups=owner+0x1000;name_ptr=owner+0x10000;result=name_ptr+256;stack=base+0x180000;stop=base+0x1f0000
u.mem_write(groups,b''.join(records));u.mem_write(owner,struct.pack('<6I',groups,0,len(records),0,0,0))
entry=int(re.search(r'\s_rf_entity_footstep_groups_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for raw,name,want in cases:
 u.mem_write(base,raw);u.mem_write(name_ptr,name+b'\0');u.mem_write(result,bytes([0xa5])*40)
 u.mem_write(stack,struct.pack('<6I',stop,base,len(raw),name_ptr,owner,result));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(entry,stop,count=30000000);assert u.reg_read(UC_X86_REG_EIP)==stop,name
 actual=struct.pack('<I',u.reg_read(UC_X86_REG_EAX))+bytes(u.mem_read(result,40));assert actual==want,(name,actual.hex(),want.hex())
report=dict(result='PASS',authored_classes=len(classes),cases=len(cases),entity_sha256=hashlib.sha256(entity).hexdigest(),classes=classes,scope='Independent inventory of every installed entity class versus PC/NXDK adapter, with comments, case, material replacement, empty classes and output-preserving errors. Uses independently inventoried group IDs/materials. Not full original entity parser or live class ownership.')
(root/'artifacts/foley-classes.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='classes'})
