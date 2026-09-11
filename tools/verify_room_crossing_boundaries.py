"""On-face, edge, corner, next-float and angled-segment crossing comparisons."""
import itertools,random,runpy,struct
from pathlib import Path
f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
near=lambda bits:struct.unpack('<f',struct.pack('<I',bits))[0]
values=(-3.,-2.,near(0xc0000001),near(0xbfffffff),0.,near(0x3fffffff),2.,near(0x40000001),3.)
queries=[]
for axis,a,z,offset,tree in itertools.product(range(3),values,values,(-2.,0.,2.),(0,1)):
 start=[offset]*3;end=[offset]*3;start[axis]=a;end[axis]=z
 queries.append(dict(start=start,end=end,flags=0,cached=len(queries)%2,tree=tree))
rng=random.Random(0x4cd9e0)
for i in range(2048):
 start=[f32(rng.uniform(-3,3)) for _ in range(3)];end=[f32(rng.uniform(-3,3)) for _ in range(3)]
 if i%4==0:
  axis=i%3;start[axis]=rng.choice(values);end=[f32(a+rng.uniform(-.0002,.0002)) for a in start]
 queries.append(dict(start=start,end=end,flags=(0,4,8,16)[i%4],cached=i%2,tree=(i//2)%2))
runpy.run_path(str(Path(__file__).with_name('verify_room_crossing.py')),init_globals={'query_specs':queries,'suite':'room-crossing-boundaries'})
