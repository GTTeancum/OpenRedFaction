"""Check owned corpse metadata on PC/NXDK and retained level class storage."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im)
b=0x30000000;x.mem_map(b,0x20000);stack=b+0x1f000;stop=b+0x1ff00
entry=int(re.search(r'\s_rf_entity_corpse_config_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
pack=lambda model=b'',emitter=b'',life=-1:struct.pack('<64s64sf',model,emitter,life)
table=(root/'artifacts/entity.tbl').read_bytes();clean=re.sub(rb'//[^\r\n]*',b'',table)
names=list(re.finditer(rb'\$Name:\s*"([^"\r\n]+)"',clean,re.I));assert len(names)>50
cases=[];expected={};replacements=[]
for i,m in enumerate(names):
 body=clean[m.start():names[i+1].start() if i+1<len(names) else len(clean)]
 model=re.search(rb'\$Corpse V3D Filename:\s*"([^"\r\n]*)"',body,re.I)
 emitter=re.search(rb'\$Corpse Emitter:\s*"([^"\r\n]*)"',body,re.I)
 lifetime=re.search(rb'\$Corpse Emitter Lifetime:\s*([^\s]+)',body,re.I)
 value=pack(model[1] if model else b'',emitter[1] if emitter else b'',float(lifetime[1]) if lifetime else -1)
 expected[m[1].decode()]=value;cases.append((body,m[1],value))
 if model:replacements.append(dict(name=m[1].decode(),model=model[1].decode()))
for body,want in [
 (b'',pack()),
 (b'$Corpse V3D Filename: ""',pack()),
 (b'$corpse v3d filename: "a.v3d" $Corpse Emitter: "fire" $Corpse Emitter Lifetime: 2.5',pack(b'a.v3d',b'fire',2.5)),
 (b'$Corpse Emitter: "fire"',pack(b'',b'fire')),
 (b'$Corpse Emitter: "" $Corpse Emitter Lifetime: -1',pack()),
 (b'$Corpse Emitter Lifetime: 3',None),
 (b'$Corpse V3D Filename: "a" $Corpse V3D Filename: "b"',None),
 (b'$Corpse Emitter: "a" $Corpse Emitter: "b"',None),
 (b'$Corpse Emitter: "a" $Corpse V3D Filename: "b"',None),
 (b'$Corpse Emitter: "a" $Corpse Emitter Lifetime: nan',None),
 (b'$Corpse Emitter: "a" $Corpse Emitter Lifetime: 1e50',None),
 (b'$Corpse Emitter: "a" $Corpse Emitter Lifetime: 1 $Corpse Emitter Lifetime: 2',None),
 (b'$Corpse V3D Filename: "'+b'x'*64+b'"',None),
 (b'$Corpse V3D Filename: "'+b'x'*63+b'"',pack(b'x'*63)),
 (b'$Corpse V3D Filename: unquoted',None),
 (b'$Corpse Wrong: "x"',None),
 (b'// $Corpse V3D Filename: "ignored"\n$Name: "next" $Corpse V3D Filename: "next.v3d"',pack()),
]:cases.append((b'$Name: "test" '+body,b'TEST',want))
cases.append((b'$Name: "other"',b'test',None))
outdir=root/'artifacts/corpse-config';outdir.mkdir(exist_ok=True);fixture=outdir/'input.tbl'
for body,name,want in cases:
 fixture.write_bytes(body);pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--corpse-config',str(fixture),name.decode()]);status=struct.unpack('<i',pc[:4])[0]
 assert len(pc)==136
 x.mem_write(b,body);x.mem_write(b+0x10000,name+b'\0');x.mem_write(b+0x11000,b'\xa5'*132)
 x.mem_write(stack,w(stop,b,len(body),b+0x10000,b+0x11000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=5000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x11000,132))==pc,(name,'NXDK mismatch')
 if want is None:assert status!=0 and pc[4:]==b'\xa5'*132,(name,body)
 else:assert status==0 and pc[4:]==want,(name,body,status)
retained=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 output=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--seeds',str(root/'Installed_Game/levels1.vpp'),str(root/'Installed_Game/tables.vpp'),level],text=True)
 rows=[line.split('\t') for line in output.splitlines() if line.startswith('SEED_CORPSE\t')];assert rows
 for _,name,model,emitter,life in rows:assert pack(model.encode(),emitter.encode(),float(life))==expected[name],(level,name)
 retained.append(dict(level=level,classes=len(rows),added_bytes=132*len(rows)))
report=dict(result='PASS',classes=len(names),cases=len(cases),replacements=replacements,retained_levels=retained,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='PC/NXDK metadata parsing agrees with independent installed-table inventory. Defaults and tags traced statically to41bb32..41bbb9; original parser not executed. PC class values survive archive closure and exact/short-budget checks. No live corpse model loading or emitter creation claimed.')
(outdir/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
