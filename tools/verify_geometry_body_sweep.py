"""Owned mover/world adapter: identity, pose selection and surface propagation."""
import json,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
original=runpy.run_path(str(root/'tools/inspect_body_sweep_order.py'))
commands=bytearray();expected=[]
for fixture,record in zip(original['fixtures'],original['results']):
 name,heights,flags,winner=fixture;n=2 if name=='two spheres' else 1
 commands.extend(struct.pack('<3f5I',*(0 if z is None else z for z in heights),*(z is not None for z in heights),flags,n))
 raw=bytearray([0xa5]*92);calls=0
 for query in record['queries']:
  z=heights[query['solid']]
  if z is not None and (7.5-z)/16<=query['limit']:calls+=1
 if winner is not None:
  raw[:68]=bytes.fromhex(record['output']);world=winner==2;face=37 if world else 0
  struct.pack_into('<I',raw,28,3000+face)
  struct.pack_into('<3f',raw,36,*(0 if world else j+1+winner for j in range(3)))
  struct.pack_into('<I',raw,52,(2000 if world else 1000+winner)+face)
  struct.pack_into('<I',raw,60,face)
  struct.pack_into('<6I',raw,68,0xffffffff if world else winner,n-1,0 if world else 0xffffffff,face,1,0)
 expected.append(struct.pack('<2I',0,winner is not None)+raw+struct.pack('<I',calls))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--geometry-body-sweep'],input=commands)
assert len(actual)==len(expected)*104
for i,want in enumerate(expected):
 got=actual[i*104:(i+1)*104]
 assert got==want,(original['fixtures'][i][0],got.hex(),want.hex())
report=dict(result='PASS',cases=len(expected),scope='PC owned-geometry adapter with actual flat mover faces and static room tree; ordering fixtures from full original499ed0. Exact source index remap, sphere/solid/room identity, velocity and synthetic metadata propagation; deliberately incompatible ray pose fields. No native adapter execution, live scene wiring or real texture/material lookup claimed.')
(root/'artifacts/geometry-body-sweep-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
