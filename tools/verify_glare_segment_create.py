"""Compare shared two-tag construction with audited original write footprints."""
import runpy,struct,re,subprocess,json,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
c=runpy.run_path(str(Path(__file__).with_name('verify_glare_segment_original.py')))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=c['w'];B=c['B'];S=c['STACK'];STOP=c['STOP'];f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im);x.mem_map(B,0x10000);x.reg_write(UC_X86_REG_FPCW,0x27f)
entry=int(re.search(r'_rf_glare_segment_create\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
CLASS=B+0x1000;LIST=B+0x2000;BE=B+0x3000;OUT=B+0x4000;OBJ=B+0x5000;CB=STOP+16
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
calls=0;descriptor=bytes(56);case=None
def hook(m,a,size,ctx):
 global calls,descriptor
 if a not in (CB,CB+16):return
 sp=m.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);assert arg(0)==77;calls+=1
 if a==CB:
  assert arg(1)==123 and arg(2)==(7 if calls==1 else 9)
  pose=c['matrix']+c['position'] if calls==1 else c['second_matrix']+struct.pack('<3f',5.25+case[3],1.5,-.25)
  m.mem_write(arg(3),pose)
 else:
  descriptor=bytes(m.mem_read(arg(1),56));m.mem_write(arg(2),w(0 if case[1] else OBJ))
 m.reg_write(UC_X86_REG_EAX,0xffffffff if calls==case[2] else 0);m.reg_write(UC_X86_REG_EIP,r(sp));m.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook)
records=c['records'];cases=[(v['index'],v['fail'],0,v['flag']) for v in records]+[(0,0,i,1) for i in (1,2,3)]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--glare-segment-create'],input=b''.join(w(*v) for v in cases));assert len(actual)==180*len(cases)
for n,case in enumerate(cases):
 calls=0;descriptor=bytes(56);index,null,error,offset=case;valid=0<=index<3;created=valid and not null and not error
 x.mem_write(CLASS,struct.pack('<ffIffIffI',.5,1,1,1,.5,1,1,1,1));x.mem_write(LIST,w(LIST,LIST,0,0));x.mem_write(BE,w(CB,CB+16,77));x.mem_write(OUT,w(0));x.mem_write(OBJ,bytes([0xa5])*528)
 x.mem_write(S,w(STOP,CLASS,3,index,123,7,9,LIST,BE,OUT));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 if n<len(records):raw=bytes.fromhex(records[n]['state'])
 else:raw=bytes([0xa5])*104
 expected_calls=error if error else 3 if valid else 0
 expected_descriptor=w(-1,f(1),f(3.25+offset*.5),f(-.5),f(1.75))+c['second_matrix'] if valid and (not error or error==3) else bytes(56)
 expected=w(-1 if error else 0,created,expected_calls,created,123 if created else 0xa5a5a5a5)+raw[:8]+w(raw[8],raw[9])+raw[12:40]+w(created)+raw[44:52]+raw[60:76]+w(raw[76])+raw[80:104]+w(created)+expected_descriptor
 assert len(expected)==180
 s=bytes(x.mem_read(OBJ,528));linked=r(OUT)==OBJ and r(LIST)==OBJ+52 and r(LIST+4)==OBJ+52 and s[52:60]==w(LIST,LIST)
 got=w(x.reg_read(UC_X86_REG_EAX),r(OUT)!=0,calls,r(LIST+8))+s[128:132]+s[:8]+w(s[8],s[9])+s[12:40]+w(r(OBJ+40)==1)+s[44:52]+s[60:76]+w(s[76])+s[80:104]+w(linked)+descriptor
 assert got==expected and actual[n*180:(n+1)*180]==expected,(n,case,got.hex(),expected.hex(),actual[n*180:(n+1)*180].hex())
 # Verify every compiled-owner byte, not just the canonical output fields.
 footprint=bytearray([0xa5]*528)
 if created:
  footprint[128:132]=w(123);footprint[4:8]=w(-1);footprint[8]=1;footprint[12:40]=w(-1,0,0,0,0,0,0)
  footprint[40:60]=w(1,index,0,LIST,LIST);footprint[60:72]=struct.pack('<3f',-1000,-1000,-1000);footprint[76]=1
  footprint[80:104]=c['position']+struct.pack('<3f',5.25+offset,1.5,-.25)
 assert s==footprint
report=dict(result='PASS',original_cases=len(records),callback_failures=3,scope='Shared PC/NXDK two-tag constructor versus full original413f20 canonical state/descriptor audit. Entire NXDK528-byte write footprint checked, ordered tag services, second matrix, midpoint, allocation parent-1, preserved allocator fields, list links and failures. Generic type10 allocation/tag geometry remain supplied.')
(root/'artifacts/glare-segment-create.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
