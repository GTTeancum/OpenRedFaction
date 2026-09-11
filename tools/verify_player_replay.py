"""Process-local input sweeps exercising first-person projection capacity."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];folder=root/'artifacts/input-replay';folder.mkdir(exist_ok=True)
cases={'yaw-sweep':(0,0,0,0,1,0),'look-up':(0,0,0,.5,.25,0),'forward':(0,0,1,0,0,0),'diagonal-turn':(.70710677,0,.70710677,0,.25,0)}
report=[]
for profile,name,command in [(p,n,c) for p in ('diagnostic','campaign') for n,c in cases.items()]:
 source=folder/(name+'.bin');source.write_bytes(struct.pack('<5fI',*command)*480)
 output=name if profile=='diagnostic' else 'campaign-'+name
 run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--replay' if profile=='diagnostic' else '--spawn-replay',str(root/'Installed_Game'),str(source),str(folder/(output+'-verified.ppm'))],capture_output=True,text=True)
 (folder/(output+'-verified.txt')).write_text(run.stdout+'\n'+run.stderr)
 assert run.returncode==0,(name,run.stderr)
 values=list(map(int,next(x for x in run.stdout.splitlines() if x.startswith('ACTOR_FOLLOW_SUMMARY ')).split()[1:]))
 assert values[0]==480 and values[4]==3145728 and values[2]<=3145728
 # Static backface rejection reduces demand (some old routes below 1 MiB). The campaign
 # yaw case still exercises the retained full-capacity fallback.
 if profile=='campaign' and name=='yaw-sweep':assert values[2]>1048576
 report.append({'profile':profile,'case':name,'result':'PASS','frames':480,'world_hash':values[1],'peak_world_bytes':values[2],'camera_hash':values[3],'cpu_capacity':values[4]})
 print(profile,name,'PASS',values[2],'peak world bytes',flush=True)
(folder/'capacity-verification.json').write_text(json.dumps(report,indent=2))
