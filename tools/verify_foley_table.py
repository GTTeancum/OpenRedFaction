"""Independent Foley inventory versus shared C on PC and NXDK machine code."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
inv=json.loads((root/'artifacts/inventory.json').read_text())['files']
a=next(a for a in inv if a['path']=='tables.vpp');e=next(e for e in a['vpp']['entries'] if e['name']=='foley.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);data=f.read(e['size'])
clean=re.sub(rb'//[^\r\n]*',b'',data)
parts=re.split(rb'\$Name:\s*"([^"\r\n]*)"',clean)
materials=['default','rock','metal','flesh','water','laval','solid','sand','ice','glass','ladder','chain fence']
groups=[];samples=[];surplus=[]
for name,body in zip(parts[1::2],parts[2::2]):
 count=re.search(rb'\$Sounds:\s*(\d+)',body);count=int(count[1]) if count else 1
 mat=re.search(rb'\$Material:\s*"([^"]*)"',body);mat=materials.index(mat[1].decode().lower()) if mat else 0;mat={10:1,11:2}.get(mat,mat)
 rows=re.findall(rb'\$Sound:\s*"([^"\r\n]*)"\s*([^\s]+)\s+([^\s]+)',body)
 assert len(rows)>=count,(name,count,len(rows))
 groups.append(name.ljust(32,b'\0')+struct.pack('<III',mat,count,len(samples)))
 for sound,near,volume in rows[:count]:samples.append(struct.pack('<61s3x3f',sound,float(near),float(volume),1))
 if len(rows)>count:surplus.append(dict(name=name.decode(),declared=count,listed=len(rows)))
expected=struct.pack('<iII',0,len(groups),len(samples))+b''.join(groups+samples)
fixture=b'#Entity Sounds\r\n$Name: "odd"\r\n$Sounds: 3\r\n$Sound: "a.wav" 2 .5\r\n$Sound: ""\r\n$Sound: "c.wav" 3 1\r\n$Sound: surplus\r\n$Name: "zero"\r\n$Sounds: 0\r\n#End'
cases=[data,fixture,fixture.replace(b'"odd"',b'"'+b'x'*32+b'"'),fixture.replace(b'$Sounds: 3',b'$Sounds: -1'),fixture.replace(b'3 1',b'3 nan'),fixture.replace(b'#End',b''),fixture+b'\0',b'#Entity Sounds\r\n#End']
outputs=[]
for i,raw in enumerate(cases):
 path=root/'artifacts/foley-test.tbl';path.write_bytes(raw)
 out=subprocess.run([str(root/'build/pc/Release/rf_audio_probe.exe'),'--foley',str(path)],capture_output=True,check=True).stdout
 outputs.append(out)
 if i==0:assert out==expected,(len(out),len(expected),out[:12],expected[:12])
 elif i==1:
  want=struct.pack('<iII',0,2,3)+struct.pack('<32sIII',b'odd',0,3,0)+struct.pack('<32sIII',b'zero',0,0,3)
  want+=b''.join(struct.pack('<61s3x3f',*row) for row in [(b'a.wav',2,.5,1),(b'',0,0,1),(b'c.wav',3,1,1)])
  assert out==want
 elif i==7:assert out==struct.pack('<iII',0,0,0)
 else:assert struct.unpack_from('<i',out)[0]!=0
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase
u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im)
base=0x30000000;u.mem_map(base,0x200000);g=base+0x40000;s=base+0x50000;counts=base+0x100000;stack=base+0x180000;stop=base+0x1f0000
entry=int(re.search(r'_rf_foley_table_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for raw,out in zip(cases,outputs):
 u.mem_write(base,raw);u.mem_write(g,bytes([0xa5])*28160);u.mem_write(s,bytes([0xa5])*311296);u.mem_write(counts,bytes(8))
 u.mem_write(stack,struct.pack('<9I',stop,base,len(raw),g,640,s,4096,counts,counts+4));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(entry,stop,count=20000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 status=u.reg_read(UC_X86_REG_EAX);ng,ns=struct.unpack('<II',u.mem_read(counts,8));actual=struct.pack('<III',status,ng,ns)
 if not status:actual+=bytes(u.mem_read(g,ng*44))+bytes(u.mem_read(s,ns*76))
 else:assert bytes(u.mem_read(g,28160))==bytes([0xa5])*28160 and bytes(u.mem_read(s,311296))==bytes([0xa5])*311296
 assert actual==out,(actual[:12],out[:12])
for gc,sc in [(len(groups)-1,4096),(640,len(samples)-1)]:
 u.mem_write(base,data);u.mem_write(g,bytes([0xa5])*28160);u.mem_write(s,bytes([0xa5])*311296);u.mem_write(counts,bytes([0xa5])*8)
 u.mem_write(stack,struct.pack('<9I',stop,base,len(data),g,gc,s,sc,counts,counts+4));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(entry,stop,count=20000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 assert u.reg_read(UC_X86_REG_EAX)==0xfffffffc
 assert bytes(u.mem_read(counts,8))==bytes([0xa5])*8
 assert bytes(u.mem_read(g,28160))==bytes([0xa5])*28160 and bytes(u.mem_read(s,311296))==bytes([0xa5])*311296
report=dict(result='PASS',groups=len(groups),samples=len(samples),cases=len(cases),short_capacity_cases=2,table_sha256=hashlib.sha256(data).hexdigest(),surplus=surplus,scope='Independent installed entity-group inventory and malformed fixtures versus PC/NXDK reader. Error preservation checked on NXDK. No full original loader or registration execution; parser primitives verified separately.')
(root/'artifacts/foley-table.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
