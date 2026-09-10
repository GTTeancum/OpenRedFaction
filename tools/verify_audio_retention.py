"""Execute original pre-level sound counter update over the complete registry."""
import json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
env=runpy.run_path(str(root/'tools/verify_audio_registration.py'));m,call,u=env['m'],env['call'],env['u']
results=[]
for enabled in (0,1):
 bank=bytearray(2600*64);expected=bytearray(len(bank));changed=0
 for slot in range(2600):
  count=(-1,0,1,2,100)[slot%5];flag=(slot//5)%2;start=slot*64
  record=bytearray((i+slot)%256 for i in range(64));record[56:60]=u(count&0xffffffff);record[60]=flag
  bank[start:start+64]=record
  if enabled and count>0 and not flag:record[56:60]=u(count+1);changed+=1
  expected[start:start+64]=record
 m.mem_write(0x1cd3ba8,bytes(bank));m.mem_write(0x17543d8,u(enabled));m.mem_write(0x1cfc5cc,u(88))
 call(0x506080,b'')
 assert bytes(m.mem_read(0x1cd3ba8,len(bank)))==bytes(expected)
 assert bytes(m.mem_read(0x1cfc5cc,4))==u(88)
 results.append(dict(enabled=enabled,slots=2600,changed=changed))
report=dict(result='PASS',original_sha256=env['digest'],results=results,scope='Unchanged original506080 pre-level counter update, full registry bytes checked with negative/zero/positive counters and both metadata flag states. Registration count remains88. No counter decrement, whole transition, C/NXDK implementation or PCM ownership claim.')
(root/'artifacts/audio-retention-verification.json').write_text(json.dumps(report,indent=2));print(report)
