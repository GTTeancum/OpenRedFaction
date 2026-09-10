"""Rendered PC checks for authored low-gravity startup events; reports load failures."""
import hashlib,json,os,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'artifacts/gravity-campaign-live';out.mkdir(exist_ok=True)
inputs=out/'inputs.bin';inputs.write_bytes(b'RFI2'+struct.pack('<I',28)+bytes(28*16));probe=root/'build/pc/Release/rf_pc_play.exe';rows=[]
for level,value in [('L17S1',4),('L17S2',3),('L17S3',4),('L18S1',9.8)]:
 r=subprocess.run([str(probe),'--spawn-replay',str(root/'Installed_Game'),str(inputs),str(out/(level+'.ppm'))],env=dict(os.environ,RF_REPLAY_ARCHIVE='levels3.vpp',RF_REPLAY_LEVEL=level+'.rfl'),capture_output=True,text=True)
 (out/(level+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:rows.append(dict(level=level,result='FAIL',exit_code=r.returncode,error=r.stderr));continue
 data=list(map(int,next(l for l in r.stdout.splitlines() if l.startswith('CAMPAIGN_STARTUP')).split()[1:]))
 valid=data[2]==1 and data[9]==struct.unpack('<I',struct.pack('<f',value))[0]
 rows.append(dict(level=level,result='PASS' if valid else 'FAIL',frames=16,startup=data))
report=dict(result='PASS' if all(r['result']=='PASS' for r in rows) else 'INCOMPLETE',pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),scope='Rendered PC campaign spawn with startup gravity; 16 idle ticks per map. Other startup actions pending; no full campaign or native Xbox late-level proof.',results=rows)
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(0 if report['result']=='PASS' else 1)
