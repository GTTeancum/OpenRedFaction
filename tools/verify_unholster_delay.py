"""Authored Unholster Delay reader on installed and malformed class blocks."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
B=0x30000000;STACK=B+0x1e0000;STOP=STACK+0x1000;OUT=STOP+0x100;NAME=OUT+0x100
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);c.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);c.mem_write(p.OPTIONAL_HEADER.ImageBase,im);c.mem_map(B,0x200000);return c
x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_unholster_delay_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
path=root/'artifacts/unholster-reader.tbl';cases=0

def check(raw,name,status,bits):
 global cases
 path.write_bytes(raw);expected=w(status,bits)
 actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--unholster-delay',str(path),name])
 assert actual==expected,('PC',name,actual.hex(),expected.hex())
 x.mem_write(B,raw);x.mem_write(NAME,name.encode()+b'\0');x.mem_write(OUT,b'\xa5'*4);x.mem_write(STACK,w(STOP,B,len(raw),NAME,OUT));x.reg_write(UC_X86_REG_ESP,STACK)
 x.emu_start(entry,STOP,count=10000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,4))==expected,('NXDK',name)
 cases+=1
for value in ('0','1','-1','0.125','1.2345','2e-3','-0.0009'):
 raw=('$Name: "test"\n$Unholster Delay: '+value+'\n$Name: "other"\n$Unholster Delay: 9\n').encode()
 check(raw,'test',0,struct.unpack('<I',struct.pack('<f',float(value)))[0])
check(b'$Name: "test"\n//$Unholster Delay: 3\n','test',0,0)
check(b'$Name: "other"\n$Unholster Delay: 3\n$Name: "test"\n','test',0,0)
check(b'$Name: "test"\n','absent',-3,0xa5a5a5a5)
for suffix in (b'bad',b'',b'1\n$Unholster Delay: 2',b'1e99'):
 check(b'$Name: "test"\n$Unholster Delay: '+suffix+b'\n','test',-2,0xa5a5a5a5)
files=json.loads((root/'artifacts/inventory.json').read_text())['files'];e=next(e for a in files if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
# Audit installed scalar declarations independently of the shared lexer.
blocks=re.split(rb'(?m)^\s*\$Name:\s*"([^"]+)"',raw);authored=[]
for i in range(1,len(blocks),2):
 name=blocks[i].decode('cp1252');values=re.findall(rb'(?m)^\s*\$Unholster\s+Delay:\s*([^\s/]+)',blocks[i+1]);assert len(values)<=1
 value=float(values[0]) if values else 0;bits=struct.unpack('<I',struct.pack('<f',value))[0]
 check(b'$Name: "'+blocks[i]+b'"\n'+blocks[i+1],name,0,bits)
 if values:authored.append(dict(name=name,seconds=value))
report=dict(result='PASS',pc_nxdk_cases=cases,installed_classes=(len(blocks)-1)//2,authored=authored,scope='Shared scalar reader PC/NXDK, independent installed declaration audit; comments, neighboring classes, default0, malformed/duplicate/missing cases. Original41c48c stores parsed float atf78 or defaults0; source address/string audit recorded separately. No live class retention.')
(root/'artifacts/unholster-reader.json').write_text(json.dumps(report,indent=2));print(report)
