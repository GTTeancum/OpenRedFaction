"""Opening-level NPC selector fixture with actual class modes and mappings.
Not complete actor construction: fixture field assumptions are recorded explicitly.
"""
import hashlib,json,struct,subprocess,sys,re,os
from inspect_models import inspect
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
entity=0x30000000;info=entity+0x3000;mode=entity+0x5000;stack=entity+0xe000;stop=entity+0xf000
u.mem_map(entity,0x10000);u.mem_map(0,0x10000)
def put(address,fmt,*values):u.mem_write(address,struct.pack(fmt,*values))
def call(address,end=stop):
 u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(address,end,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==end
reads=[]
u.hook_add(UC_HOOK_MEM_READ,lambda uc,access,address,size,value,data:reads.append((address-entity,size)),begin=entity,end=entity+0x1493)
assets=str(root/'build/pc/Release/rf_entity_assets_probe.exe');motion=str(root/'build/pc/Release/rf_motion_probe.exe')
entity_probe=str(root/'build/pc/Release/rf_entity_probe.exe');game=root/'Installed_Game'
defaults={r['entity_class'].lower():r for r in json.loads((root/'artifacts/entity-default-weapons.json').read_text())['rows']}
pose_mode='--pose' in sys.argv
advance_mode='--advance' in sys.argv or pose_mode
controller_mode='--controller' in sys.argv or advance_mode
if controller_mode:
 xp=pefile.PE(str(root/'build/xbox/main.exe'));xim=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
 x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xim)+4095)//4096*4096);x.mem_write(xb,xim);x.mem_map(entity,0x10000)
 xentry=int(re.search(r'_rf_motion_apply_controller\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)

 if advance_mode:xupdate=int(re.search(r'_rf_motion_update\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)

obj=0x30100000;desc=0x30200000;motion_mem=0x30300000;data_mem=0x30400000
for address in (obj,desc,motion_mem,data_mem):u.mem_map(address,0x10000)
controller_results=[];advance_results=[];pose_results=[]
if pose_mode:
 entries={e['name'].lower():(a['path'],e) for a in json.loads((root/'artifacts/inventory.json').read_text())['files'] for e in a.get('vpp',{}).get('entries',[])}
 def asset_bytes(name):
  archive,e=entries[name.lower()]
  with (game/archive).open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
 pose_data=0x31000000;u.mem_map(pose_data,0x1000000)
 pose_env=dict(os.environ);pose_env['RF_PROBE_POSE_ONLY']='1'
 pose_env.pop('RF_PROBE_CACHE',None);pose_env.pop('RF_PROBE_EYE_SETUP',None)

initial=struct.pack('<iiffiI',0,-1,0,0,0,0);results=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 out=subprocess.check_output([assets,'--catalog',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'motions.vpp'),str(game/'meshes.vpp'),level],text=True)
 if pose_mode:
  skeleton_output=subprocess.check_output([assets,'--skeletons',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'meshes.vpp'),level],text=True)
  models={f[1]:f[2] for row in skeleton_output.splitlines() if (f:=row.split('\t'))[0]=='SKELETON_CLASS'}
 resource_rows={};envelopes={};markers={}
 for row in out.splitlines():
  f=row.split('\t')
  if f[0]=='CATALOG_RESOURCE':resource_rows[int(f[1]),int(f[2])]=(int(f[3]),f[4],f[5])
  elif f[0]=='CATALOG_ENVELOPE':envelopes[int(f[1]),int(f[2])]=struct.pack('<I4i',*map(int,f[3:]))
  elif f[0]=='CATALOG_MARKERS':markers[int(f[1]),int(f[2])]=list(map(int,f[3:]))
 for line in out.splitlines():
  if not line.startswith('CATALOG_MAP\t'):continue
  fields=line.split('\t');cls,weapon=fields[1:3];skeleton=int(fields[3])
  if weapon or skeleton==0xffffffff:continue
  mapping=list(map(int,fields[4:27]));actions=list(map(int,fields[27:72]))
  config=bytes.fromhex(subprocess.check_output([assets,'--class-physics',str(game/'tables.vpp'),cls],text=True).strip())
  flags,flags2,mode_id=struct.unpack_from('<3I',config,68)
  physics=struct.unpack('<I',subprocess.check_output([entity_probe,'--physics-flags'],input=struct.pack('<5I',0,flags,flags2,0,0)))[0]
  primary,secondary=[defaults[cls.lower()][k] for k in ('primary','secondary')]
  for prior in (0,-1,123456):
   u.mem_write(entity,bytes(0xd000));reads.clear()
   put(entity+0x24,'<I',0);put(entity+0x2c,'<i',5);put(entity+0x200,'<i',-1)
   put(entity+0x294,'<I',info);put(entity+0x29c,'<I',info);put(info+0x94,'<I',2)
   put(info+0x724,'<II',flags,flags2);put(entity+0x858,'<I',mode);put(mode+4,'<i',mode_id)
   put(entity+0x1a8,'<I',physics);put(entity+0x2a0,'<I',entity);put(entity+0x2a4,'<ii',primary,secondary)
   # Execute the recovered scalar initializer span, not fabricated action/behavior values.
   put(entity+0x520,'<I',0xa5a5a5a5);put(entity+0x554,'<I',0xa5a5a5a5)
   u.reg_write(UC_X86_REG_ESI,entity+0x2a0);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_ESP,stack)
   call(0x402d68,0x402dad)
   put(entity+0x834,'<i',-1);put(entity+0x1380,'<i',prior);u.mem_write(entity+0x138c,initial[:16])
   for i,value in enumerate(mapping):put(entity+0x8e4+16*i,'<i',value)
   for i,value in enumerate(actions):put(entity+0xa54+16*i,'<i',value)
   put(0x7c75cc,'<I',0);u.mem_write(0x64ecb9,b'\0');u.mem_write(0x6fc4d8,b'\0')
   put(stack,'<II',stop,entity);u.reg_write(UC_X86_REG_ESP,stack);call(0x41f400)
   actual=bytes(u.mem_read(entity+0x138c,16))+initial[16:]
   priority=struct.pack('<10i3fI',-1,0,physics if physics<0x80000000 else physics-0x100000000,mode_id,0,prior,0,0,0,5,0,0,0,0)
   selected=subprocess.check_output([motion,'--priority'],input=initial+struct.pack('<23i',*mapping)+priority)
   status,handled=struct.unpack_from('<2i',selected);assert status==0
   expected=selected[8:]
   if not handled:
    movement=struct.pack('<3f5i',0,0,0,mode_id,0,0,2,4)
    selected=subprocess.check_output([motion,'--movement'],input=expected+struct.pack('<23i',*mapping)+movement)
    assert selected[:4]==bytes(4);expected=selected[4:]
   assert actual==expected,(level,cls,prior,actual.hex(),expected.hex())
   if controller_mode:
    count=sum(model==skeleton for model,index in resource_rows);assert 0<count<=172
    resources=[];cache_ids=[];cache_names=[]
    for i in range(count):
     loop,name,file=resource_rows[skeleton,i];stem=name.rsplit('.',1)[0].lower()
     if stem not in cache_names:cache_names.append(stem)
     cache_ids.append(cache_names.index(stem))
     resources.append(envelopes[skeleton,i]+struct.pack('<I3i',loop,*markers[skeleton,i][1:],0))
    for delta in (0.,1/60,1/30):
     delta=struct.unpack('<f',struct.pack('<f',delta))[0]
     u.mem_write(obj,bytes(0x10000));u.mem_write(desc,bytes(0x10000));u.mem_write(motion_mem,bytes(0x10000));u.mem_write(data_mem,bytes(0x10000))
     put(obj+0x1d50,'<I',desc);put(obj+0x1cfc,'<ii',-1,-1);put(obj+0x1d48,'<i',-1);put(obj+0x1cf8,'<H',1)
     put(desc+0xf58,'<I',count)
     for i,r in enumerate(resources):
      m=motion_mem+cache_ids[i]*256;d=data_mem+cache_ids[i]*256
      put(desc+0xf5c+i*4,'<I',m);put(m+0x78,'<I',d);u.mem_write(d+16,r[4:12]);u.mem_write(desc+0x120c+i,r[20:21])
      u.mem_write(m+0x50,r[24:28]);u.mem_write(m+0x64,r[28:32])
      u.mem_write(d+36,r[12:20]);put(d+80,'<I',84);u.mem_write(d+84,r[:4])
     wrapper=obj+0x4000;put(wrapper,'<II',2,obj);put(entity+0x80,'<I',wrapper)
     u.mem_write(entity+0x138c,initial[:16]);put(0x5a4014,'<f',delta)
     put(stack,'<II',stop,entity);u.reg_write(UC_X86_REG_ESP,stack);call(0x41f270)
     empty=struct.pack('<I',0)+bytes(192)+struct.pack('<3i',-1,-1,-1)+bytes(40)+struct.pack('<fII',0,1,0)
     wire=struct.pack('<I',count)+empty+b''.join(resources)+expected+struct.pack('<23if',*mapping,delta)
     actual_c=subprocess.check_output([motion,'--controller-count'],input=wire)
     x.mem_write(entity,empty);x.mem_write(entity+0x2000,b''.join(resources));x.mem_write(entity+0x8000,expected);x.mem_write(entity+0x9000,struct.pack('<23i',*mapping))
     x.mem_write(stack,struct.pack('<IIIfIII',stop,entity+0x8000,entity+0x9000,delta,entity,entity+0x2000,count))
     x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(xentry,stop,count=100000)
     assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
     xresult=bytes(4)+bytes(x.mem_read(entity,260))+bytes(x.mem_read(entity+0x8000,24))+b''.join(bytes(x.mem_read(entity+0x2000+i*36+32,4)) for i in range(count))
     assert xresult==actual_c,(level,cls,prior,delta,'NXDK controller')

     read=lambda offset,size:bytes(u.mem_read(obj+offset,size))
     project=lambda:read(0x12d0,196)+read(0x1cfc,8)+read(0x1d48,4)+struct.pack('<II',read(0x1d4c,1)[0],read(0x1d14,1)[0])+read(0x1d18,32)+read(0x1d04,4)+struct.pack('<II',struct.unpack('<H',read(0x1cf8,2))[0],read(0x1d44,1)[0]|read(0x1d45,1)[0]<<1)
     state=project()
     control=bytes(u.mem_read(entity+0x138c,16))+initial[16:]
     assert actual_c[:288]==bytes(4)+state+control,(level,cls,prior,delta,'state/controller')
     refs=struct.unpack('<'+'i'*count,actual_c[288:])
     for identity in range(len(cache_names)):
      expected_refs=struct.unpack('<i',u.mem_read(motion_mem+identity*256+0x74,4))[0]
      assert sum(refs[i] for i in range(count) if cache_ids[i]==identity)==expected_refs,(level,cls,delta,'cache references')
     active=struct.unpack_from('<I',state)[0]
     controller_results.append(dict(level=level,entity_class=cls,delta=delta,prior_action=prior,resources=count,
      controller=list(struct.unpack('<iiff',control[:16])),slots=[list(struct.unpack_from('<iif',state,4+i*12)) for i in range(active)]))
     if advance_mode:
      # Factory calls the full wrapper after selector/weighting, with these trailing arguments.
      put(stack,'<IIf4I',stop,wrapper,delta,0,0,0,1);u.reg_write(UC_X86_REG_ESP,stack);call(0x503360)
      updated_resources=[r[:32]+struct.pack('<i',refs[i]) for i,r in enumerate(resources)]
      update_c=subprocess.check_output([motion,'--update-count'],input=struct.pack('<I',count)+state+b''.join(updated_resources)+struct.pack('<f',delta))
      x.mem_write(stack,struct.pack('<IIIIf',stop,entity,entity+0x2000,count,delta))
      x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(xupdate,stop,count=100000)
      assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
      update_x=bytes(4)+bytes(x.mem_read(entity,260))+b''.join(bytes(x.mem_read(entity+0x2000+i*36+32,4)) for i in range(count))
      assert update_x==update_c,(level,cls,prior,delta,'NXDK advance')
      advanced=project()
      assert update_c[:264]==bytes(4)+advanced,(level,cls,prior,delta,'original advance',[(i,update_c[4+i:8+i].hex(),advanced[i:i+4].hex()) for i in range(0,260,4) if update_c[4+i:8+i]!=advanced[i:i+4]])
      advanced_refs=struct.unpack('<'+'i'*count,update_c[264:])
      for identity in range(len(cache_names)):
       original_refs=struct.unpack('<i',u.mem_read(motion_mem+identity*256+0x74,4))[0]
       assert sum(advanced_refs[i] for i in range(count) if cache_ids[i]==identity)==original_refs
      active=struct.unpack_from('<I',advanced)[0]
      advance_results.append(dict(level=level,entity_class=cls,delta=delta,prior_action=prior,
       slots=[list(struct.unpack_from('<iif',advanced,4+i*12)) for i in range(active)],
       phase=struct.unpack_from('<f',advanced,248)[0],generation=struct.unpack_from('<I',advanced,252)[0],events=struct.unpack_from('<I',advanced,256)[0]))
      if pose_mode:
       model_name=models[cls];raw_model=asset_bytes(model_name)
       section=next(q for q in inspect(raw_model)['sections'] if q['type']=='0x424f4e45');begin=section['offset']+8
       bone_count=struct.unpack_from('<I',raw_model,begin)[0];assert 0<bone_count<=50
       parents=[struct.unpack_from('<i',raw_model,begin+4+i*56+52)[0] for i in range(bone_count)]
       def depth(i):return 0 if parents[i]<0 else 1+depth(parents[i])
       order=bytes(sorted(range(bone_count),key=depth));put(desc+0x48,'<I',bone_count)
       for i,parent in enumerate(parents):put(desc+0x94+i*0x4c,'<i',parent)
       u.mem_write(desc+0x8000,order)
       compact=bytearray(advanced);motion_names=[];loop_mask=0;cursor=pose_data
       for slot in range(active):
        index=struct.unpack_from('<i',advanced,4+slot*12)[0]
        loop,name,filename=resource_rows[skeleton,index];data=asset_bytes(filename)
        assert cursor+len(data)<=pose_data+0x1000000
        u.mem_write(cursor,data);put(motion_mem+cache_ids[index]*256+0x78,'<I',cursor);cursor+=(len(data)+4095)//4096*4096
        struct.pack_into('<i',compact,4+slot*12,slot);motion_names.append(filename);loop_mask|=loop<<slot
       if not motion_names:motion_names=[resource_rows[skeleton,0][2]]
       pose_c=subprocess.check_output([str(root/'build/pc/Release/rf_skeleton_probe.exe'),str(game/'meshes.vpp'),str(game/'motions.vpp'),model_name,*motion_names],input=bytes(compact)+struct.pack('<I',loop_mask),env=pose_env)
       assert struct.unpack_from('<I',pose_c)[0]==bone_count
       put(stack,'<4I',stop,bone_count,desc+0x8000,obj);u.reg_write(UC_X86_REG_ESP,stack);call(0x51b500)
       original_pose=bytes(u.mem_read(obj,bone_count*48))+b''.join(bytes(u.mem_read(obj+0x1394+i*48,2)) for i in range(bone_count))
       assert pose_c[4:]==original_pose,(level,cls,prior,delta,'pose',[(i,pose_c[4+i:8+i].hex(),original_pose[i:i+4].hex()) for i in range(0,bone_count*48,4) if pose_c[4+i:8+i]!=original_pose[i:i+4]])
       pose_results.append(dict(level=level,entity_class=cls,model=model_name,delta=delta,prior_action=prior,bones=bone_count,pose_sha256=hashlib.sha256(original_pose).hexdigest()))
   results.append(dict(level=level,entity_class=cls,mode=mode_id,primary=primary,secondary=secondary,prior_action=prior,
    prior_action_reads=sum(a<=0x1380<a+n for a,n in reads),controller=list(struct.unpack('<iiff',actual[:16])),read_fields=sorted({hex(a) for a,n in reads})))
report=dict(result='PASS',cases=len(results),original_sha256=digest,scope='Full original41f400 selector versus PC priority/movement composition using installed base maps, class flags/movement modes and default weapon IDs. Original402d68 scalar span executes. Fixture zeroes unspecified actor state, uses kind0/no links/nonplayer, zero velocity and no external events; not full factory or first pose/weight update. Field1380 sensitivity is tested, not assumed.',results=results)
(root/'artifacts/npc-initial-selection.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})

if controller_mode:
 report=dict(result='PASS',cases=len(controller_results),original_sha256=digest,scope='Original complete41f270 and actual loaded-motion callees versus PC priority/movement/controller composition. NXDK controller also matches all99 cases. Real catalog envelopes/loop flags, cache aliases and aggregated references; zero/60Hz/30Hz deltas. Actor construction remains an explicit fixture; no503360 playback advance or pose sampling.',results=controller_results)
 (root/'artifacts/npc-initial-controller.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})

if advance_mode:
 report=dict(result='PASS',cases=len(advance_results),original_sha256=digest,scope='Complete original503360/501ab0/51ba80 after41f270 versus PC and NXDK rf_motion_update; loaded catalog envelopes and markers with shared cache aliases. Full compact playback state and aggregated references match. Zero/60Hz/30Hz first advances only; actor construction is a fixture, no pose sampling or live NPC animation.',results=advance_results)
 (root/'artifacts/npc-initial-advance.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})

if pose_mode:
 report=dict(result='PASS',cases=len(pose_results),bone_matrices=sum(r['bones'] for r in pose_results),original_sha256=digest,scope='Complete original51b500 and callees after verified first startup advance versus PC archive-based evaluator. Real model parent trees and complete active motion bytes; matrices and cache generations match exactly. C wire packs active files; the probe constructs sparse catalog IDs and calls rf_entity_pose_evaluate without changing slot order. No live actor ownership, rendering or NXDK pose execution.',results=pose_results)
 (root/'artifacts/npc-initial-pose.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
