"""Independent authored doorway-plane checks for the staged L1S1 replay."""
import json,math,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def floats(words):return struct.unpack('<'+'f'*len(words),struct.pack('<'+'I'*len(words),*words))
def measure(spawn,body):
 groups=next(l for l in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] if l['file'].lower()=='l1s1.rfl')['records']
 a,b=[next(g for g in groups if g['keys'][0]['uid']==uid)['keys'][0]['position'] for uid in (8591,8593)]
 center=[(a[i]+b[i])*.5 for i in range(3)];dx=b[0]-a[0];dz=b[2]-a[2];length=math.hypot(dx,dz)
 normal=[dz/length,0,-dx/length]
 start=floats(spawn[1:4]);end=floats(body[22:25])
 depth=lambda p:sum((p[i]-center[i])*normal[i] for i in range(3))
 return dict(plane_center=center,plane_normal=normal,start_position=start,end_position=end,start_depth=depth(start),end_depth=depth(end),crossed=depth(start)<-1 and depth(end)>1)
