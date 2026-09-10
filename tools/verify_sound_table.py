"""Check port table adapter against installed inventory and malformed fixtures."""
import json,runpy,struct,subprocess,sys,re
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
inventory=runpy.run_path(str(root/'tools/inspect_sound_table.py'));data=inventory['data'];rows=inventory['rows']
run=root/'artifacts/sound-table-check';run.mkdir(exist_ok=True)
p=pefile.PE(str(root/'build/xbox/main.exe'));image=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(image)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,image)
base=0x30000000;m.mem_map(base,0x200000);stack=base+0x1e0000;stop=base+0x1f0000;output=base+0x100000;counter=base+0x180000
entry=int(re.search(r'_rf_sound_table_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def check(text,capacity,success):
 path=run/'input.tbl';path.write_bytes(text)
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--sound-table',str(path),str(capacity)])
 status,count=struct.unpack_from('<iI',pc);assert (status==0)==success,(text[:80],status)
 sentinel=bytes([165])*((capacity+1)*76);m.mem_write(base,text or b' ');m.mem_write(output,sentinel);m.mem_write(counter,struct.pack('<I',123))
 m.mem_write(stack,struct.pack('<6I',stop,base,len(text),output,capacity,counter));m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_FPCW,0x27f)
 m.emu_start(entry,stop,count=10000000);assert m.reg_read(UC_X86_REG_EIP)==stop
 result=m.reg_read(UC_X86_REG_EAX);n=struct.unpack('<I',m.mem_read(counter,4))[0]
 assert result==(status&0xffffffff) and n==count
 actual=bytes(m.mem_read(output,len(sentinel)))
 if status:assert actual==sentinel and count==123
 else:assert actual[:count*76]==pc[8:] and actual[count*76:]==sentinel[count*76:]
 return pc[8:]
actual=check(data,len(rows),True);expected=b''.join(r['name'].encode().ljust(64,b'\0')+struct.pack('<3f',r['near'],r['volume'],r['rolloff']) for r in rows)
assert actual==expected
check(data,len(rows)-1,False)
valid=b'#Sounds Start\n"a.wav" 1 .5 1\n#Sounds End'
check(valid,1,True);check(b'//comment\n'+valid+b' // trailing\n',1,True)
invalid=[b'',valid[:-1],valid.replace(b'"a.wav"',b'a.wav'),valid.replace(b'.5',b'NaN'),valid.replace(b'.5',b'-1'),valid.replace(b'1\n#',b'0\n#'),valid+b' junk',valid.replace(b'a.wav',b'a'*61),valid.replace(b'.5',b'1e999')]
for text in invalid:check(text,1,False)
check(b'#Sounds Start\n'+b'"a.wav" 1 .5 1\n'*2048+b'#Sounds End',2048,True)
check(b'#Sounds Start\n'+b'"a.wav" 1 .5 1\n'*2049+b'#Sounds End',2048,False)
def archive_check(path,capacity,budget,success):
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--sound-table-archive',str(path),str(capacity),str(budget)])
 status,count=struct.unpack_from('<iI',raw);assert (status==0)==success
 if success:assert count==len(rows) and raw[8:]==expected
 else:assert count==123 and len(raw)==8
archive_check(root/'Installed_Game/tables.vpp',len(rows),len(data),True)
archive_check(root/'Installed_Game/tables.vpp',len(rows),len(data)-1,False)
archive_check(root/'Installed_Game/tables.vpp',len(rows)-1,len(data),False)
archive_check(root/'Installed_Game/audio.vpp',len(rows),len(data),False)
fixture=bytearray(6144);struct.pack_into('<4I',fixture,0,0x51890ace,1,1,len(fixture))
fixture[2048:2058]=b'sounds.tbl';struct.pack_into('<I',fixture,2108,3);fixture[4096:4099]=b'bad'
malformed=run/'malformed.vpp';malformed.write_bytes(fixture);archive_check(malformed,len(rows),3,False)
report=dict(result='PASS',installed_rows=len(rows),cases=11+len(invalid),scope='PC and compiled NXDK reader exact records against independent installed inventory, capacity/count query, output preservation and malformed fixtures. NXDK reader executed in Unicorn; PC archive loader verifies exact/short scratch budgets, row capacity, missing/malformed tables and owned rows after close. Not original parser equivalence, XEMU file loading or campaign registration.')
(run/'report.json').write_text(json.dumps(report,indent=2));print(report)
