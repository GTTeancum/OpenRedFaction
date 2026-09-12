"""Archive-backed USERBMAP policy, budgets, precedence and cleanup."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'artifacts/user-bitmap-materials';out.mkdir(exist_ok=True)
def archive(name,entries):
 size=2048+(2048 if entries else 0)+sum((len(data)+2047)//2048*2048 for _,data in entries);raw=bytearray(size);struct.pack_into('<4I',raw,0,0x51890ace,1,len(entries),size);cursor=4096
 for index,(key,data) in enumerate(entries):
  at=2048+index*64;raw[at:at+len(key)]=key.encode();struct.pack_into('<I',raw,at+60,len(data));raw[cursor:cursor+len(data)]=data;cursor+=(len(data)+2047)//2048*2048
 path=out/name;path.write_bytes(raw);return path
empty=archive('empty.vpp',[]);bad=archive('corrupt.vpp',[('USERBMAP',b'bad')])
probe=root/'build/pc/Release/rf_material_probe.exe'
def check(names,budget,archives):
 p=subprocess.run([str(probe),'--named',str(budget),*map(str,archives)],input='\n'.join(names)+'\n',capture_output=True,text=True)
 assert p.returncode in (0,1),(p.returncode,p.stdout,p.stderr);return p.returncode,p.stdout.strip().splitlines()
names=['USERBMAP','userbmap','unresolved.tga'];budget=3*28+2*4096
code,lines=check(names,budget,[empty]);assert code==0 and lines[0]==f'3 2 1 {budget}'
expected_hash=2166136261
for y in range(32):
 for x in range(32):
  for channel in ((121,121,142,255) if x%8 and y%8 else (64,64,64,255)):expected_hash=((expected_hash^channel)*16777619)&0xffffffff
for line in lines[1:3]:assert list(map(int,line.split()[1:]))==[0,4294967295,32,32,expected_hash,6,0]
assert list(map(int,lines[3].split()[1:]))==[-3,4294967295,0,0,2166136261,0,0]
assert check(names,budget-1,[empty])==(1,['-4']) # Includes failure after first successful fallback.
assert check(['USERBMAP'],4096+28,[bad,empty])==(1,['-2'])
assert check(['USERBMAP'],4096+28,[empty,bad])==(1,['-2']) # Search all archives before generating.
report=dict(result='PASS',cases=4,generated=2,bytes=budget,scope='Named loader actual VPP files: exact budget, late allocation-budget failure cleanup, case folding, missing ordinary name, corrupt found image rejection even after earlier archive miss. Generated image hash matches recovered pattern; native scene residency checked separately.')
(root/'artifacts/user-bitmap-materials.json').write_text(json.dumps(report,indent=2));print(report)
