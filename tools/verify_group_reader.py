"""Compare bounded C moving-group reader with independent installed inventory."""
import ctypes as c,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
class Group(c.Structure):
 _fields_=[('name',c.c_char*256),('sounds',(c.c_char*256)*4)]+[(k,c.c_uint32) for k in 'offset bytes key_offset key_count legacy_offset legacy_count'.split()]+[('ids_offset',c.c_uint32*2),('ids_count',c.c_uint32*2),('mode',c.c_uint32),('unknown',c.c_uint32),('header',c.c_uint8*2),('flags',c.c_uint8*6),('sound_values',c.c_float*4)]
class Key(c.Structure):
 _fields_=[(k,c.c_uint32) for k in 'uid offset bytes'.split()]+[('links',c.c_uint32*3),('position',c.c_float*3),('orientation',c.c_float*9),('timing',c.c_float*5),('rotation',c.c_float),('label',c.c_char*256),('flag',c.c_uint8)]
def same(a,b):return struct.pack('<'+'f'*len(a),*a)==struct.pack('<'+'f'*len(b),*b)
levels=json.loads((root/'artifacts/moving-groups.json').read_text())['results'];groups=keys=ids=0
for level in levels:
 run=subprocess.run([str(root/'build/pc/Release/rf_collision_probe.exe'),'--groups',str(root/'Installed_Game'/level['archive']),level['file']],capture_output=True);assert run.returncode==0,(level['file'],run.returncode);raw=run.stdout;at=0
 for reference in level['records']:
  g=Group.from_buffer_copy(raw,at);at+=c.sizeof(g);groups+=1
  assert g.name.decode('cp1252')==reference['name'] and g.offset==reference['offset'] and g.bytes==reference['bytes']
  assert g.key_count==len(reference['keys']) and g.legacy_count==len(reference['legacy'])
  assert list(g.header)==reference['header'] and list(g.flags)==reference['flags'] and g.mode==reference['mode'] and g.unknown==reference['unknown']
  for i,sound in enumerate(reference['sounds']):assert g.sounds[i].value.decode('cp1252')==sound['name'] and same([g.sound_values[i]],[sound['value']])
  for r in reference['keys']:
   k=Key.from_buffer_copy(raw,at);at+=c.sizeof(k);keys+=1;disk=r['orientation_disk']
   assert (k.uid,k.offset,k.bytes,k.flag,k.label.decode('cp1252'))==(r['uid'],r['offset'],r['bytes'],r['flag'],r['label'])
   assert list(k.links)==r['links'] and same(list(k.position),r['position']) and same(list(k.orientation),disk[3:]+disk[:3]) and same(list(k.timing),r['timing']) and same([k.rotation],[r['rotation']])
  for i,field in enumerate(['ids1','ids2']):
   n=g.ids_count[i];assert n==len(reference[field]);assert list(struct.unpack_from('<'+'I'*n,raw,at))==reference[field];at+=4*n;ids+=n
 assert at==len(raw),(level['file'],at,len(raw))
report=dict(result='PASS',levels=len(levels),groups=groups,keys=keys,ids=ids,truncated_groups=groups,scope='PC no-allocation C reader vs independent Python inventory for all records, raw metadata, poses, sounds and IDs; each group truncated at its final byte returns FORMAT with unchanged reader/output. Not original reader execution, runtime registration or XEMU.')
(root/'artifacts/group-reader-verification.json').write_text(json.dumps(report,indent=2));print(report)
