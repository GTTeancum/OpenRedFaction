"""Compare retained campaign NPC backlinks to original load-time x86 writes.
Run verify_npc_support_probe.py first to produce the three current PC replays.
"""
import hashlib,json,os,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EBX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image()
triggers=json.loads((root/'artifacts/triggers.json').read_text())['results']
events=json.loads((root/'artifacts/events.json').read_text())['results'];results=[]
# Authored positive case outside the three ordinary opening replays.
env=dict(os.environ)
for key in ['RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START','RF_REPLAY_REGION_START','RF_REPLAY_FORCE_UID']:env.pop(key,None)
env.update(RF_REPLAY_LEVEL='L4S2.rfl',RF_REPLAY_ARCHIVE='levels1.vpp')
folder=root/'artifacts/npc-support-probe'
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),
 str(root/'artifacts/npc-bodies-start.bin'),str(folder/'L4S2.rfl.ppm')],env=env,cwd=root,capture_output=True,text=True,check=True)
(folder/'L4S2.rfl.txt').write_text(run.stdout)
for level in ['L1S1.rfl','L1S2.rfl','L1S3.rfl','L4S2.rfl']:
 lines=(root/'artifacts/npc-support-probe'/(level+'.txt')).read_text().splitlines()
 rows=[list(map(int,l.split()[1:])) for l in lines if l.startswith('NPC_BACKLINK_ROW ')]
 summary=next(list(map(int,l.split()[1:])) for l in lines if l.startswith('NPC_BACKLINKS '))
 actors=next(int(l.split()[1]) for l in lines if l.startswith('NPC_BODIES '))
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
 base=0x30000000;u.mem_map(base,0x400000);trigger=base+0x300000;array=trigger+0x1000;stack=trigger+0x20000
 def words(a,*values):u.mem_write(a,struct.pack('<'+'I'*len(values),*values))
 assert rows,(level,'missing live owner records')
 addresses=[base+i*0x1800 for i in range(len(rows))]
 words(0x73d890,addresses[0]);words(0x64e63c,0x64e3b0)
 for i,((uid,handle,_),a) in enumerate(zip(rows,addresses)):
  words(a+0x10,addresses[i+1] if i+1<len(rows) else 0x73d880)
  words(a+0x20,uid,0);words(a+0x2c,handle);words(a+0x838,0xffffffff)
  words(0x7394cc+4*(handle&0xffff),a)
 writes=[]
 def observe(cpu,address,size,context):
  if address==0x4611fa:writes.append(address)
 u.hook_add(UC_HOOK_CODE,observe)
 records=next(l for l in triggers if l['file']==level and l['archive']=='levels1.vpp')['records']
 count=len(next(l for l in events if l['file']==level and l['archive']=='levels1.vpp')['records'])
 for i,r in enumerate(records):
  slot=count+i;source=((slot+1)<<16)|slot
  words(trigger+0x2c,source);words(trigger+0x2b0,4 if r['flags'][2]==1 else 0)
  words(trigger+0x2d4,len(r['links']),len(r['links']),array);words(array,*r['links'])
  u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,trigger)
  u.emu_start(0x4611a1,0x461231,count=1000000)
  assert u.reg_read(UC_X86_REG_EIP)==0x461231
 expected=[];digest=2166136261
 for (uid,handle,actual),a in zip(rows,addresses):
  original=struct.unpack('<I',u.mem_read(a+0x838,4))[0]
  assert actual==original,(level,uid,actual,original)
  expected.append([uid,handle,original])
  for byte in struct.pack('<III',uid,handle,original):digest=((digest^byte)*16777619)&0xffffffff
 assert summary==[len(writes),sum(r[2]!=0xffffffff for r in expected),digest,4*actors],(level,summary)
 results.append(dict(level=level,actors=len(rows),writes=len(writes),linked=summary[1],digest=digest))
assert sum(r['writes'] for r in results)>0,'No positive backlink coverage'
report=dict(result='PASS',levels=results,scope='Original4611a1..461231 and real UID/array/flag/typed-handle callees; no substituted calls. Prepared NPC registry uses observed port handles, authored trigger order and flag mapping. Compare every retained field and summary against PC. Non-NPC objects/key owners omitted; general lookup behavior separately verified. No original global factory order, eligibility or AI claim.')
(root/'artifacts/npc-backlinks-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
