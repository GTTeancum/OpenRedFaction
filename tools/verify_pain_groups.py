"""Installed pain group bindings, with original434cb0 name resolution."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
impact='--impact' in sys.argv;death='--death' in sys.argv;count=1 if impact else 3 if death else 2;sentinels=(123,456,789)[:count]
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
def archive_entry(name):
 with (root/'Installed_Game/tables.vpp').open('rb') as f:
  count=struct.unpack('<4I',f.read(16))[2];at=2048+(count*64+2047)//2048*2048
  for i in range(count):
   f.seek(2048+i*64);r=f.read(64);size=struct.unpack_from('<I',r,60)[0]
   if r[:60].split(b'\0')[0].decode()==name:f.seek(at);return f.read(size)
   at+=(size+2047)//2048*2048
 raise AssertionError(name)
entity=archive_entry('entity.tbl');foley=archive_entry('foley.tbl')
names=re.findall(rb'^\s*\$Name:\s*"([^"\r\n]*)"',foley,re.M)
assert 0<len(names)<=640 and all(len(n)<32 for n in names)
classes=list(re.finditer(rb'^\s*\$Name:\s*"([^"\r\n]*)"',entity,re.M))
b=0x30000000;stack=b+0x1e0000;stop=stack+0x1000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,2*1024*1024);return m
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_'+('impact_sound_group_read' if impact else ('damage_sound' if death else 'pain')+'_groups_read')+r'\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,name in enumerate(names):
 row=name.ljust(32,b'\0')+bytes(12);u.mem_write(0x6300f8+i*44,row);x.mem_write(b+0x1000+i*44,row)
u.mem_write(0x636ef8,w(len(names)));x.mem_write(b,w(b+0x1000,0,len(names),0,0,0))
wire=w(len(names))+b''.join(n.ljust(32,b'\0') for n in names)
folder=root/('artifacts/impact-sound-group' if impact else 'artifacts/damage-sound-groups' if death else 'artifacts/pain-groups');folder.mkdir(exist_ok=True);path=folder/'entity.tbl';path.write_bytes(entity)
def original(name):
 u.mem_write(b+0x1000,name+b'\0');u.mem_write(stack,w(stop,b+0x1000));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x434cb0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
def check(text,name,expected):
 path.write_bytes(text)
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--impact-sound-group' if impact else '--damage-sound-groups' if death else '--pain-groups',str(path),name.decode()],input=wire)
 x.mem_write(b+0x10000,text);x.mem_write(b+0xe000,name+b'\0');x.mem_write(b+0xf000,w(*sentinels))
 x.mem_write(stack,w(stop,b+0x10000,len(text),b+0xe000,b,b+0xf000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=20000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 result=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0xf000,count*4));assert result==pc
 if expected is None:assert pc[:4]!=bytes(4) and pc[4:]==w(*sentinels)
 else:assert pc==w(0,*expected),(name,pc,expected)
rows=[]
for i,match in enumerate(classes):
 block=entity[match.end():classes[i+1].start() if i+1<len(classes) else len(entity)]
 labels=[]
 for kind in (b'Low',b'Med'):
  found=re.findall(rb'^\s*\$'+kind+rb'_Pain\s+Sounds:\s*"([^"\r\n]*)"',block,re.M|re.I)
  assert len(found)<=1;labels.append(found[0] if found else b'')
 if death:
  found=re.findall(rb'^\s*\$DeathSnd:\s*"([^"\r\n]*)"',block,re.M|re.I)
  assert len(found)<=1;labels.append(found[0] if found else b'')
 expected=list(map(original,labels))
 if impact:
  found=re.findall(rb'^\s*\$Impact Death Sound:\s*"([^"\r\n]*)"',block,re.M|re.I)
  assert len(found)<=1;expected=[original(found[0])] if found else [123]
 check(entity,match[1],expected)
 rows.append(dict(name=match[1].decode(),groups=expected))
if impact:
 check(b'$Name: "fixture"\n#End',b'fixture',[123])
 for label in (b'',b'unknown'):
  check(b'$Name: "fixture"\n$Impact Death Sound: "'+label+b'"\n#End',b'fixture',[0xffffffff])
 for text in (b'$Name: "fixture"\n$Impact Death Sound: 3',b'$Name: "fixture"\n$Impact Death Sound: "a"\n$Impact Death Sound: "b"',b'$Name: "fixture"\n$Impact Death bad "a"'):
  check(text,b'fixture',None)
else:
 for text in (b'$Name: "fixture"\n#End',b'$Name: "fixture"\n$Low_Pain Sounds: "unknown"\n$Med_Pain Sounds: ""\n#End'):
  check(text,b'fixture',[0xffffffff]*count)
 for text in (b'$Name: "fixture"\n$Low_Pain Sounds: 3',b'$Name: "fixture"\n$Low_Pain Sounds: "a"\n$Low_Pain Sounds: "b"',b'$Name: "fixture"\n$Med_Pain bad "a"'):
  check(text,b'fixture',None)
 if death:
  for text in (b'$Name: "fixture"\n$DeathSnd: 3',b'$Name: "fixture"\n$DeathSnd: "a"\n$DeathSnd: "b"'):
   check(text,b'fixture',None)
report=dict(result='PASS',installed_classes=len(rows),foley_groups=len(names),synthetic_cases=6 if impact else 7 if death else 5,rows=rows,
 scope='PC and linked NXDK metadata reader agree with independent installed label extraction and actual original434cb0/57c130 resolution. Full original class parser not executed. Missing impact fields preserve input; missing pain/death and explicit empty/unknown resolve-1; malformed/duplicate guards preserve output. Playback/voice ownership remain separate.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='rows'})
