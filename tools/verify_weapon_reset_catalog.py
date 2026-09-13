"""Reset descriptor composition against original flag parse/store and sound lookup."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
B=0x30000000;T=B+0x10000;P=B+0x30000;F=B+0x40000;G=B+0x41000;I=B+0x50000;STACK=B+0xff000;STOP=B+0xfff00
inv=json.loads((root/'artifacts/inventory.json').read_text())
def table(name):
 e=next(e for a in inv['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==name)
 with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
def clean(data):return re.sub(rb'"[^"\r\n]*"|//[^\r\n]*',lambda m:b'' if m[0].startswith(b'//') else m[0],data)
weapons=table('weapons.tbl');names=re.findall(rb'\$Name:\s*"([^"\r\n]*)"',clean(table('foley.tbl')))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,0x100000);return u
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_weapon_reset_catalog_read\s+([0-9a-fA-F]+)',mapping)[1],16)
def stack_probe(c,at,size,data):
 sp=c.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',c.mem_read(sp,4))[0];c.reg_write(UC_X86_REG_ESP,sp+4-c.reg_read(UC_X86_REG_EAX));c.reg_write(UC_X86_REG_EIP,ret)
probe=int(re.search(r'\s__chkstk\s+([0-9a-fA-F]+)',mapping)[1],16);x.hook_add(UC_HOOK_CODE,stack_probe,begin=probe,end=probe)
cases=[];outputs=[]
def original_flags(body,seed):
 first=re.search(rb'\$Flags:\s*(\([^)]*\))',body);second=re.search(rb'\$Flags2:\s*(\([^)]*\))',body)
 data=b'$Flags: '+first[1]+(b'\n$Flags2: '+second[1] if second else b'')+b'\n$Next: 0'
 u.mem_write(T,data+b'\0'*16);u.mem_write(P,bytes(272));u.mem_write(P,w(T,T));u.mem_write(P+0x10c,w(len(data)))
 u.mem_write(B+0x264,w(seed,0xa5a5a5a5));u.reg_write(UC_X86_REG_EDI,P);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_ESP,STACK)
 u.emu_start(0x4c2dc2,0x4c2e1a,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==0x4c2e1a
 return bytes(u.mem_read(B+0x264,8))
def original_sound(name,sounds):
 for i,s in enumerate(sounds):u.mem_write(0x6300f8+i*44,s.ljust(32,b'\0')+bytes(12))
 u.mem_write(0x636ef8,w(len(sounds)));u.mem_write(T,name+b'\0');u.mem_write(STACK,w(STOP,T));u.reg_write(UC_X86_REG_ESP,STACK)
 u.emu_start(0x434cb0,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return w(u.reg_read(UC_X86_REG_EAX))
def check(raw,seed,sounds,error=None):
 initial=w(*seed);x.mem_write(T,raw+b'\0');x.mem_write(I,initial);x.mem_write(F,w(G,0,len(sounds),0,0,0))
 for i,name in enumerate(sounds):x.mem_write(G+i*44,name.ljust(32,b'\0')+bytes(12))
 x.mem_write(B,b'\xa5'*4872);x.mem_write(STACK,w(STOP,T,len(raw),I,F,B));x.reg_write(UC_X86_REG_ESP,STACK)
 x.emu_start(entry,STOP,count=20000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,4872))
 if error is not None:expected=w(error)+b'\xa5'*4872
 else:
  out=bytearray(4872);parts=re.split(rb'\$Name:\s*"([^"\r\n]*)"',clean(raw));rows=list(zip(parts[1::2],parts[2::2]))
  primary=len(re.findall(rb'\$Name:',clean(raw).split(b'#End')[0]));out[4096:4104]=w(len(rows),primary)
  for i,(name,body) in enumerate(rows):
   out[i*64:i*64+len(name)]=name;sound=re.search(rb'\$Stop Sound:\s*"([^"\r\n]*)"',body)
   out[4104+i*12:4116+i*12]=original_flags(body,seed[i])+original_sound(sound[1] if sound else b'',sounds)
  expected=w(0)+out
 assert actual==expected,(len(cases),actual[:4],expected[:4])
 cases.append(w(len(raw),len(sounds))+initial+b''.join(n.ljust(32,b'\0') for n in sounds)+raw);outputs.append(actual)
check(weapons,[0]*64,names);check(weapons,[0x80000000|i for i in range(64)],names)
wrap=lambda b:b'#Primary Weapons\n'+b+b'#End\n#Secondary Weapons\n#End\n'
block=b'$Name: "test"\n$Flags: ("continuous_fire")\n$Flags2: ("flame")\n$Stop Sound: "release"\n'
for text,sounds in ((block,[b'release',b'RELEASE']),(block.replace(b'release',b'unknown'),[b'release']),(block.replace(b'$Stop Sound: "release"\n',b''),[]),(block.replace(b'$Flags2: ("flame")\n',b''),[b'RELEASE'])):check(wrap(text),[0x100]*64,sounds)
for bad in (block.replace(b'$Flags: ("continuous_fire")\n',b''),block+b'$Flags: ()\n',block+b'$Flags2: ()\n',block+b'$Stop Sound: "x"\n',block.replace(b'continuous_fire',b'unknown'),block.replace(b'"release"',b'release')):check(wrap(bad),[0]*64,[],error=-2)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--weapon-reset-catalog'],input=b''.join(cases));assert pc==b''.join(outputs)
for budget in (len(weapons),len(weapons)-1,0):
 got=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--weapon-reset-load',str(root/'Installed_Game/tables.vpp'),str(budget)],input=cases[0]);assert got==(outputs[0] if budget==len(weapons) else w(-4)+b'\xa5'*4872)
report=dict(result='PASS',cases=len(cases),authored_weapons=44,foley_groups=len(names),archive_budgets=3,bytes=4872,scratch=len(weapons),original_sha256=digest,scope='Original4c2dc2..4c2e1a parser/store including required/optional tags and actual513020; original434cb0 lookup with497 retained names. PC/NXDK complete reset catalogs, primary OR, optional fields, unknown/duplicate sound names and malformed guards. Stop Sound token extraction independently supplied to original lookup; full weapon parser and native scene binding excluded.')
(root/'artifacts/weapon-reset-catalog.json').write_text(json.dumps(report,indent=2));print(report)
