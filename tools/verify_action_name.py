"""Compare428fe0 and its real C-locale callees against PC/NXDK name lookup."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,data);u.mem_map(0x30000000,0x10000);return u
original=load(exe);native=load(root/'build/xbox/main.exe');original.mem_write(0x20852f4,w(0))
b=0x30000000;actor=b+0x1000;cls=b+0x3000;declarations=b+0x4000;pointers=b+0x6000;strings=b+0x8000;query=b+0xb000;stack=b+0xe000;stop=b+0xf000
mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_entity_action_name_lookup\s+([0-9a-fA-F]+)',mapping)[1],16)
def call(u,entry,*args):
 u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
cases=[]
for slot in range(45):
 for text in (b'death_front',b'DEATH_FRONT',b'DeAtH_FrOnT',b'',b'\xc0x',b'A'*63):
  names=[None]*45;names[slot]=text;names[44]=text
  for model,kind in ((1,2),(0,2),(1,3)):
   cases.append((model,kind,names,text.lower()))
cases.extend([(1,2,[None]*45,b''),(1,2,[b'corpse_drop']*45,None),(1,2,[b'corpse_drop']*45,b'corpse_carry'),(1,2,[b'\xc0']*45,b'\xe0')])
inputs=bytearray();expected=[]
for model,kind,names,q in cases:
 original.mem_write(actor,bytes(0x1800));original.mem_write(cls,bytes(0x800));original.mem_write(declarations,bytes(45*52))
 original.mem_write(actor+0x80,w(model));original.mem_write(actor+0x294,w(cls));original.mem_write(cls+0x94,w(kind));original.mem_write(cls+0x760,w(declarations))
 mask=[0,0];records=[]
 for i,name in enumerate(names):
  index=44-i # reverse declaration storage: result is actor slot, not declaration index
  original.mem_write(actor+0xa58+i*16,w(index if name is not None else -1))
  native.mem_write(pointers+i*4,w(strings+i*64 if name is not None else 0))
  raw=(name or b'').ljust(64,b'\0');records.append(raw)
  for u in (original,native):u.mem_write(strings+i*64,raw)
  if name is not None:
   mask[i//32]|=1<<(i%32);original.mem_write(declarations+index*52,w(len(name),strings+i*64 if name else 0))
 for u in (original,native):u.mem_write(query,(q or b'').ljust(64,b'\0'))
 want=call(original,0x428fe0,actor,query if q is not None else 0);got=call(native,entry,model,kind,pointers,query if q is not None else 0)
 assert got==want,(len(expected),want,got)
 expected.append(want);inputs+=w(model,kind,*mask,int(q is not None))+b''.join(records)+(q or b'').ljust(64,b'\0')
path=root/'artifacts/action-name-input.bin';path.write_bytes(inputs)
output=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--action-name',str(path)])
assert output==w(*expected)
assert call(original,0x428fe0,0,query)==0xffffffff
report=dict(result='PASS',cases=len(cases),original_sha256=sha,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Full428fe0,40a1e0,5001d0 and57c130 execute unchanged under explicit original C locale. PC/NXDK first action slot, reversed declaration indices, duplicates, unavailable/empty declarations, NULL query/model, model kinds, ASCII case and non-ASCII bytes. No live declaration-table or corpse callback integration.')
(root/'artifacts/action-name-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
