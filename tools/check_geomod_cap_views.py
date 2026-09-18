"""Inspect real saved beam joint cuts from elevated render-only views."""
import csv,hashlib,json,os,struct,subprocess
from audit_geomod_idle_lighting import audit as audit_idle_lighting
from check_geomod_material_owner import decode_tga
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];folder=ROOT/'artifacts/geomod-cap-views';folder.mkdir(parents=True,exist_ok=True)
(folder/'report.json').unlink(missing_ok=True)
inputs=folder/'neutral.bin';inputs.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(121*48))
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_AUTHORED_SOURCE='95',RF_REPLAY_AUTHORED_SOURCES='3',RF_REPLAY_PLAYER_CHECKPOINT='1',RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(ROOT/'artifacts/xemu/render-20260917-191001/xbox-checkpoint.rfds'))
views={'baseline':None,'near-cap':'-3,2.25,0,-5,1.5,2.5','far-cap':'-3,2.25,0,-5,1.5,-2.5','far-reverse':'-4,2.25,-4,-5,1.5,-2.5','uncut-far':'-3,2.25,0,-5,1.5,-2.5'}
report={};baseline=None
for name,camera in views.items():
 checkpoint=folder/(name+'.rfcp');checkpoint.unlink(missing_ok=True)
 local=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(checkpoint),RF_REPLAY_DEPTH_OUT=str(folder/(name+'.depth')),RF_REPLAY_TERRAIN_BASE_AUDIT=str(folder/(name+'-lightmaps.csv')),RF_REPLAY_TERRAIN_MESH_AUDIT=str(folder/(name+'-mesh.csv')))
 if name=='far-cap':
  local.update(RF_REPLAY_TERRAIN_MATERIAL_AUDIT=str(folder/'cap-material.bin'))
 if camera:local['RF_REPLAY_INSPECTION_CAMERA']=camera
 if name=='uncut-far':local.pop('RF_REPLAY_GEOMOD_CHECKPOINT_IN')
 with (folder/(name+'.log')).open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(inputs),str(folder/(name+'.ppm'))],env=local,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 assert result.returncode==0,(name,result.returncode)
 data=checkpoint.read_bytes()
 if baseline is None:baseline=data
 if name!='uncut-far':assert data==baseline,('inspection changed gameplay save',name)
 with Image.open(folder/(name+'.ppm')) as im:im.save(folder/(name+'.png'))
 report[name]=dict(camera=camera,checkpoint_bytes=len(data),matches_baseline=data==baseline)
# Fixed unobstructed floor region, selected from inspected elevated captures.
# The neighboring uncut post occludes pixels left of x354, so those are excluded.
region=(360,268,410,281)
with Image.open(folder/'far-cap.png') as cut,Image.open(folder/'uncut-far.png') as original:
 assert cut.crop(region).tobytes()==original.crop(region).tobytes(),'floor detail color changed'
depth=(folder/'far-cap.depth').read_bytes();uncut=(folder/'uncut-far.depth').read_bytes()
assert depth[:12]==uncut[:12]==b'RFD1'+struct.pack('<II',640,480)
for y in range(region[1],region[3]):
 start=12+(y*640+region[0])*4;end=12+(y*640+region[2])*4
 assert depth[start:end]==uncut[start:end],'floor detail depth changed'
line=next(l for l in (folder/'far-cap.log').read_text(encoding='utf-8').splitlines() if l.startswith('DEPTH_CAMERA '))
pose=struct.unpack('<12f',struct.pack('<12I',*map(int,line.split()[2:])))
x,y=375,275;zbuffer=struct.unpack_from('<f',depth,12+(y*640+x)*4)[0]
z=.1/(1-zbuffer/16777215/(1000/999.9))
view=((x+.5-320)*z/320,(240-y-.5)*z/320,z)
world=[pose[k]+sum(view[j]*pose[3+j*3+k] for j in range(3)) for k in range(3)]
assert abs(world[1]+2)<.002,('patch is not on expected floor',world)
report['floor_patch']=dict(region=region,color_and_depth_identical=True,pixel=[x,y],world=world,scope='Pre-existing floor detail, not a destruction-created floating polygon')
lightmaps=[]
for row in csv.DictReader((folder/'far-cap-lightmaps.csv').open(encoding='utf-8')):
 packed=bytes.fromhex(row['packed']);pixels=struct.unpack('<'+'H'*(len(packed)//2),packed)
 channels=[(p>>shift)&31 for p in pixels for shift in (10,5,0)]
 assert int(row['faces'])>0 and int(row['corners'])>=3,('unused connected map',row['map'])
 for axis,dimension in [('u','width'),('v','height')]:
  origin=int(row['atlas_x' if axis=='u' else 'atlas_y'])
  assert float(row['min_pixel_'+axis])>=origin-.001
  assert float(row['max_pixel_'+axis])<=origin+int(row[dimension])-1+.001
 assert min(channels)>0,('black texels',row['map'])
 lightmaps.append(dict(map=int(row['map']),faces=int(row['faces']),corners=int(row['corners']),material=int(row['material']),channel_min=min(channels),channel_max=max(channels)))
assert len(lightmaps)==7
report['connected_lightmaps']=lightmaps
idle=audit_idle_lighting((folder/'far-cap-lightmaps.csv').read_bytes())
assert idle['result']=='PASS',idle
report['idle_lighting']=idle
live=(folder/'cap-material.bin').read_bytes()
assert live[:4]==b'RFT1' and len(live)>=24
material,width,height,fmt,size=struct.unpack_from('<5I',live,4)
assert size==width*height*4 and len(live)==24+size
assert all(m['material']==material for m in lightmaps), 'audited image is not the cap material'
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text(encoding='utf-8'))
archive=next(a for a in inventory['files'] if a['path']=='ui.vpp')
entry=next(e for e in archive['vpp']['entries'] if e['name']=='rock02.tga')
with (ROOT/'Installed_Game/ui.vpp').open('rb') as source:
 source.seek(entry['offset']);tga=source.read(entry['size'])
ew,eh,expected=decode_tga(tga)
assert (width,height)==(ew,eh) and live[24:]==expected, 'cap image differs from level substrate rock02'
report['cap_material']=dict(asset='ui.vpp/rock02.tga',material=material,width=width,height=height,source_format=fmt,rgba_bytes=size,rgba_sha256=hashlib.sha256(expected).hexdigest(),asset_sha256=hashlib.sha256(tga).hexdigest(),scope='Actual retained CPU image and all seven cap-map material IDs; GPU upload, filtering and final appearance remain separate')
report.update(result='PASS'  ,scope='Checkpoint-invariant render camera; visual inspection is separate, not playable elevated player placement')
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
