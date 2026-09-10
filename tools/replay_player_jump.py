"""Campaign jump replay: airborne rejection, takeoff, held landing, repress."""
import hashlib,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'artifacts/player-jump-live';out.mkdir(exist_ok=True);source=out/'inputs.bin'
source.write_bytes(b'RFI2'+struct.pack('<I',28)+b''.join(struct.pack('<5f2I',0,0,0,0,0,0,int(8<=i<20 or 24<=i<96 or i>=100)) for i in range(128)))
exe=root/'build/pc/Release/rf_pc_play.exe'
r=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(source),str(out/'final.ppm')],cwd=root,capture_output=True,text=True,check=True);(out/'pc.txt').write_text(r.stdout)
values={line.split()[0]:list(map(int,line.split()[1:])) for line in r.stdout.splitlines() if line.startswith('PLAYER_JUMP')}
v=values['PLAYER_JUMP_FRAMES'];rows=[v[i:i+8] for i in range(0,len(v),8)];f=lambda v:struct.unpack('<f',struct.pack('<I',v))[0]
assert values['PLAYER_JUMP']==[3,2,2,100]
assert rows[8][2:5]==[1,0,3], 'Initial airborne press should be rejected'
assert rows[24][2:5]==[1,1,3] and rows[24][7]&2 and f(rows[24][6])>5
assert all(not row[2] and not row[3] for row in rows[25:96]), 'Held input must not repeat even after landing'
assert all(row[4]==1 for row in rows[90:100]), 'First jump must land'
assert rows[100][2:5]==[1,1,3], 'Released/repressed input must jump again'
height=max(f(row[5]) for row in rows[24:90])-f(rows[24][5]);assert 1.2<height<1.4
assert all(not(row[7]&2) for row in rows[25:100]), 'Jump flag must be consumed after takeoff support decision'
report=dict(result='PASS',frames=128,requests=3,accepted=2,height=height,pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),scope='Campaign fixture process-local input, grounded takeoff, held-button landing, repress and initial airborne rejection. No physical input or original full-arc parity proof; audio/config lifecycle remains incomplete.')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
