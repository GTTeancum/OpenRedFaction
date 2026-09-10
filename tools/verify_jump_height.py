"""Check bounded game.tbl jump-height parsing on PC and compiled NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
inventory=json.loads((root/'artifacts/inventory.json').read_text());a=next(a for a in inventory['files'] if a['path']=='tables.vpp');e=next(e for e in a['vpp']['entries'] if e['name']=='game.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as file:file.seek(e['offset']);installed=file.read(e['size'])
key=b'$Max Entity Jump Height: ';cases=[('installed',installed,0,1.33)]
for token,value in [('0',0),('1.33',1.33),('2.5',2.5),('+4',4),('.5',.5),('1.25e1',12.5),('1e-2',.01)]:cases.append((token,key+token.encode(),0,value))
for token in ('-1','nan','inf','1e999','1x','"1.33"','1e','+',''):cases.append(('invalid '+token,key+token.encode(),-2,123))
cases += [('missing',b'$Other: 4',-3,123),('empty',b'',-4,123),('duplicate',key+b'1\n'+key+b'2',-2,123),('comment',b'// '+key+b'99\n'+key+b'2.5 // end',0,2.5),('quoted',b'"$Max Entity Jump Height:" 4',-3,123),('case',b'$max entity jump height: 3',0,3),('nul',key+b'1\x00',-2,123)]
probe=root/'build/pc/Release/rf_entity_assets_probe.exe';pack=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);flt=lambda x:struct.pack('<f',x)
assert subprocess.check_output([str(probe),'--jump-height',str(root/'Installed_Game/tables.vpp')])==flt(1.33)
pe=pefile.PE(str(root/'build/xbox/main.exe'));image=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase;pe.close();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(image)+4095)//4096*4096);u.mem_write(origin,image)
base=0x30000000;u.mem_map(base,0x20000);out=base+0x18000;stack=base+0x1e000;stop=base+0x1f000
entry=int(re.search(r'_rf_game_jump_height_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for name,data,status,value in cases:
 expected=pack(status)+flt(value)
 assert subprocess.check_output([str(probe),'--jump-height-text'],input=data)==expected,name
 u.mem_write(base,data or b'\x00');u.mem_write(out,flt(123));u.mem_write(stack,pack(stop,base,len(data),out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(entry,stop,count=3000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4,name
 assert pack(u.reg_read(UC_X86_REG_EAX))+bytes(u.mem_read(out,4))==expected,name
 assert bytes(u.mem_read(base,len(data)))==data,name
report=dict(result='PASS',cases=len(cases),installed_height=1.33,installed_bytes=len(installed),pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Narrow game.tbl height reader, PC exact-budget/archive-close checks, NXDK parser execution. Does not reconstruct every original table-parser rule or the full game configuration lifecycle.')
(root/'artifacts/jump-height-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
