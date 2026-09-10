"""Campaign jump restrictions across crouch and climb transitions."""
import argparse,hashlib,json,os,struct,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--climb',action='store_true');p.add_argument('--simultaneous',action='store_true');args=p.parse_args();assert not(args.climb and args.simultaneous)
root=Path(__file__).resolve().parents[1];out=root/('artifacts/jump-climb' if args.climb else 'artifacts/jump-simultaneous' if args.simultaneous else 'artifacts/jump-crouch');out.mkdir(exist_ok=True)
frames=180 if args.climb else 128
if args.climb:
 commands=[(0,0,int(i>=80),1 if i<80 else -1 if i>=105 else 0,0,0,int(90<=i<100 or 120<=i<130 or i>=139)) for i in range(frames)]
elif args.simultaneous:
 commands=[(0,0,0,0,0,int(24<=i<40),int(24<=i<32 or i>=40)) for i in range(frames)]
else:
 commands=[(0,0,0,0,0,int(8<=i<40),int(24<=i<48 or i>=52)) for i in range(frames)]
source=out/'inputs.bin';source.write_bytes(b'RFI2'+struct.pack('<I',28)+b''.join(struct.pack('<5f2I',*c) for c in commands))
exe=root/'build/pc/Release/rf_pc_play.exe';env=dict(os.environ)
if args.climb:env.update(RF_REPLAY_LEVEL='L1S2.rfl',RF_REPLAY_REGION_START='1')
r=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(source),str(out/'final.ppm')],cwd=root,env=env,capture_output=True,text=True,check=True);(out/'pc.txt').write_text(r.stdout)
v={line.split()[0]:list(map(int,line.split()[1:])) for line in r.stdout.splitlines() if line.startswith(('PLAYER_JUMP','PLAYER_CLIMB','PLAYER_STANCE'))}
ring=v['PLAYER_JUMP_FRAMES'];rows={ring[i]:ring[i:i+8] for i in range(0,len(ring),8)}
if args.climb:
 assert v['PLAYER_JUMP']==[3,1,1,139],v['PLAYER_JUMP']
 assert rows[90][2:5]==[1,0,2] and rows[120][2:5]==[1,0,2]
 assert rows[139][2:5]==[1,1,3] and rows[139][7]&2
 assert v['PLAYER_CLIMB'][1:3]==[1,1]
 # The exit itself returns mode 1 before the same-frame action-3 request.
 cr=v['PLAYER_CLIMB_FRAMES'];exit_row=next(cr[i:i+9] for i in range(0,len(cr),9) if cr[i]==139)
 assert exit_row[1:3]==[0xffffffff,1]
elif args.simultaneous:
 assert v['PLAYER_JUMP']==[2,1,1,40],v['PLAYER_JUMP']
 assert rows[24][2:5]==[1,0,1] and rows[24][7]&0x400
 assert rows[40][2:5]==[1,1,3] and not(rows[40][7]&0x400)
 assert all(rows[i][4]==1 for i in range(110,128))
else:
 assert v['PLAYER_JUMP']==[2,1,1,52],v['PLAYER_JUMP']
 assert rows[24][2:5]==[1,0,1] and rows[24][7]&0x400
 assert all(rows[i][2:4]==[0,0] for i in range(25,48))
 assert not rows[41][7]&0x400 and rows[52][2:5]==[1,1,3]
 assert all(rows[i][4]==1 for i in range(120,128))
report=dict(result='PASS',climb=args.climb,simultaneous=args.simultaneous,frames=frames,jump=v['PLAYER_JUMP'],pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),scope='Staged campaign state transitions; crouched/climbing presses rejected, later rising edge accepted. Climb exit accepts in restored mode 1 before support; this does not prove a grounded upper-platform jump or authored-route fidelity.')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
