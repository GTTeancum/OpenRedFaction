"""Check authored/default eye limits on PC/NXDK with original numeric stores."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_FPCW,UC_X86_REG_EAX
b=0x30000000;stack=b+0x1f000;stop=b+0x1ff00
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,0x20000);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);binary=root/'build/xbox/main.exe';x=machine(binary)
entry=int(re.search(r'\s_rf_entity_eye_limits_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
table=(root/'artifacts/entity.tbl').read_bytes();clean=re.sub(rb'//[^\r\n]*',b'',table);names=list(re.finditer(rb'\$Name:\s*"([^"\r\n]+)"',clean,re.I));assert len(names)>50
outdir=root/'artifacts/eye-limits';outdir.mkdir(exist_ok=True);fixture=outdir/'input.tbl';count=0;authored=0;bits_set=0
cases=[]
for i,m in enumerate(names):
 body=clean[m.start():names[i+1].start() if i+1<len(names) else len(clean)]
 pair=re.search(rb'\$Min Relative Eye PHB:\s*<([^>]+)>\s*\$Max Relative Eye PHB:\s*<([^>]+)>',body,re.I)
 values=[float(v) for group in pair.groups() for v in group.split(b',')] if pair else None
 cases.append((body,m[1],values,0));authored+=bool(pair)
for body,values,status in [(b'$Name: "test"',None,0),(b'$Name: "test" $Min Relative Eye PHB: <1,2,11.0487375> $Max Relative Eye PHB: <4,5,6>',[1,2,11.0487375,4,5,6],0),(b'$Name: "test" $Min Relative Eye PHB: <1,2,3>',None,3),(b'$Name: "test" $Max Relative Eye PHB: <1,2,3>',None,3),(b'$Name: "test" $Min Relative Eye PHB: <1,nan,3> $Max Relative Eye PHB: <1,2,3>',None,3)]:cases.append((body,b'test',values,status))
for body,name,values,status in cases:
 fixture.write_bytes(body);pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--eye-limits',str(fixture),name.decode()]);actual=struct.unpack('<I',pc[:4])[0]
 x.mem_write(b,body);x.mem_write(b+0x10000,name+b'\0');x.mem_write(b+0x11000,b'\xa5'*24);x.mem_write(stack,w(stop,b,len(body),b+0x10000,b+0x11000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=5000000);assert x.reg_read(UC_X86_REG_EIP)==stop,(name,hex(x.reg_read(UC_X86_REG_EIP)))
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x11000,24))==pc,(name,'NXDK')
 if status:assert actual!=0 and pc[4:]==b'\xa5'*24
 else:
  assert actual==0,(name,actual,body)
  u.mem_write(b,bytes(256));u.reg_write(UC_X86_REG_EBP,b);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
  if values is None:u.emu_start(0x41be17,0x41be39,count=100)
  else:
   u.mem_write(b+0x6c,f(*values));u.reg_write(UC_X86_REG_EDI,b+0x6c);u.emu_start(0x41bd9b,0x41bdc4,count=100)
   u.reg_write(UC_X86_REG_EDI,b+0x78);u.emu_start(0x41bdd4,0x41be39,count=100)
  assert pc[4:]==bytes(u.mem_read(b+0x6c,24)),(name,'original stores')
  if count<len(names):bits_set+=bool(struct.unpack('<I',pc[12:16])[0]&4)
 count+=1
retained=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 output=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--seeds',str(root/'Installed_Game/levels1.vpp'),str(root/'Installed_Game/tables.vpp'),level],text=True)
 rows=[line.split('\t') for line in output.splitlines() if line.startswith('SEED_EYE\t')]
 fixture.write_bytes(table)
 for row in rows:
  direct=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--eye-limits',str(fixture),row[1]])
  assert direct==w(0,*map(int,row[2:])),(level,row)
 assert rows
 retained.append(dict(level=level,classes=len(rows),added_bytes=24*len(rows),summary=next(line for line in output.splitlines() if line.startswith('SEEDS '))))
report=dict(retained_levels=retained,result='PASS',classes=len(names),authored_pairs=authored,installed_clearance_bit_set=bits_set,cases=count,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='PC/NXDK metadata reader exact agreement, original numeric conversion/default instructions. Original parser itself not executed. Malformed input preserves output. Retained PC class values checked after archive closure on L1S1/L1S2/L1S3, including existing exact-budget and undersized-budget checks. Live death actor binding remains open.')
(outdir/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
