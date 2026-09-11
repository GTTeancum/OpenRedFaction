"""Installed opening-level support diagnostics; not original-runtime ground validation."""
import json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/npc-support-probe';folder.mkdir(exist_ok=True)
fixtures=[('L1S1.rfl','door-audio-reference/inputs.bin','RF_REPLAY_DOOR_START',[78,78,191,35988,410628,1546073344]),
 ('L1S2.rfl','lift-cycle/inputs.bin','RF_REPLAY_LIFT_START',[39,38,114,18552,392544,1626689487]),
 ('L1S3.rfl','npc-bodies-start.bin',None,[28,25,48,13384,387088,3245065254])]
reports=[]
for level,inputs,staged,bodies in fixtures:
 env=os.environ.copy()
 for key in ['RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START']:env.pop(key,None)
 env.update(RF_REPLAY_LEVEL=level,RF_REPLAY_ARCHIVE='levels1.vpp')
 if staged:env[staged]='1'
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),
  str(root/'artifacts'/inputs),str(folder/(level+'.ppm'))],cwd=root,env=env,capture_output=True,text=True,check=True)
 (folder/(level+'.txt')).write_text(run.stdout)
 rows={line.split()[0]:list(map(int,line.split()[1:])) for line in run.stdout.splitlines() if line.startswith('NPC_')}
 # c560fea owner hashes, before adding these probes: guards against live-body,
 # sphere, publication, movement or cached-class mutation by the diagnostic.
 assert rows['NPC_BODIES']==bodies,(level,rows['NPC_BODIES'])
 summary=rows['NPC_SUPPORT_PROBE'];miss=rows['NPC_SUPPORT_FIRST_MISS']
 assert summary[0]==bodies[1] and summary[1]+summary[2]==summary[0]
 assert summary[3]+summary[4]+summary[5]+summary[6]==summary[1] and summary[7]==0,(level,summary)
 first={}
 if summary[3]:
  assert miss[0]==summary[10] and miss[3]==0
  data=subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(root/'Installed_Game/levels1.vpp'),level])
  record=next(data[i:i+1084] for i in range(0,len(data),1084) if struct.unpack_from('<I',data,i)[0]==miss[0])
  first=dict(uid=miss[0],class_name=record[52:308].split(b'\0')[0].decode('cp1252'),position=struct.unpack_from('<3f',record,4),
   probe_start_y=struct.unpack('<f',struct.pack('<I',miss[12]))[0],probe_end_y=struct.unpack('<f',struct.pack('<I',miss[13]))[0])
 reports.append(dict(level=level,summary=summary,first_miss=first))
report=dict(result='PASS',scope='Startup fixture only, cleared actor intent and unlinked parents. Actual shared world/mover queries; numeric support proposal on private state. Owner hashes match c560fea. Does not prove original first-use pose, live scheduling, moving-object acceptance, AI, fall or landing.',levels=reports)
(folder/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
