"""Audit original cached-tag construction and lookup using installed model records.

Execute51d4ce..51d59f unchanged, including getter and matrix helpers. The file
loader is outside this slice: descriptor arrays are supplied from audited V3C
sections. Parent sentinel preservation prevents guessing later parent binding.
"""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
binary=root/'Installed_Game/RF.exe';digest=hashlib.sha256(binary.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(binary)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
b=0x30000000;u.mem_map(b,0x20000);stack=b+0x1d000;stop=b+0x1e000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
get=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
records=[];queries=0
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for entry in archive.get('vpp',{}).get('entries',[]):
  if not entry['name'].lower().endswith('.v3c'):continue
  with (root/'Installed_Game'/archive['path']).open('rb') as f:f.seek(entry['offset']);data=f.read(entry['size'])
  header=struct.unpack_from('<10I',data);sections=inspect(data)['sections'];spheres=[];bones=[];attachments=[]
  for section in sections:
   off=section['offset']+8
   if section['type']=='0x43535048':
    assert section['declared']==44;spheres.append(data[off:off+44])
   if section['type']=='0x424f4e45':
    count=struct.unpack_from('<I',data,off)[0]
    bones=[data[off+4+i*56:off+28+i*56].split(b'\0')[0] for i in range(count)]
   if section.get('lods') and not attachments:
    lod=section['lods'][0];attachments=[data[lod['attachment_offset']+i*100:lod['attachment_offset']+(i+1)*100] for i in range(lod['props'])]
  assert header[8]==0, 'nonempty legacy attachment header requires loader recovery'
  assert header[9]==len(spheres) and len(spheres)<=32 and len(bones)<=50
  u.mem_write(b,bytes(0x10000));u.mem_write(b+0x44,w(0x120));u.mem_write(b+0x48,w(len(bones)))
  # First submesh fields returned by51db90/51dbd0. No legacy attachments in installed headers.
  u.mem_write(b+0x19c0+0x58,w(0,0,len(spheres),b+0x4000))
  u.mem_write(b+0x4000,b''.join(spheres) or b'\0')
  for i in range(len(spheres)):u.mem_write(b+0x12f0+i*56,w(0x13570000+i))
  u.mem_write(stack,bytes(0x100));u.mem_write(stack+0x5c,w(stop))
  u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_EBP,0);u.reg_write(UC_X86_REG_FPCW,0x37f)
  u.emu_start(0x51d4ce,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
  assert get(b+0x12b8)==len(spheres) and get(b+0x44)==0x124
  for i,raw in enumerate(spheres):
   assert get(b+0x12bc+i*56)==b+0x4000+i*44
   expected=struct.pack('<9f',1,0,0,0,1,0,0,0,1)+raw[28:40]
   assert bytes(u.mem_read(b+0x12c0+i*56,48))==expected
   assert get(b+0x12f0+i*56)==0x13570000+i, 'parent changed: investigate before using raw CSPH parent'
  for i,name in enumerate(bones):u.mem_write(b+0x4c+i*76,name+b'\0')
  u.mem_write(b+0x1a50,w(b+0x6000));u.mem_write(b+0x608c,w(b+0x6200));u.mem_write(b+0x6204,w(b+0x6400))
  u.mem_write(b+0x6410,w(b+0x7000,len(attachments)));u.mem_write(b+0x7000,b''.join(attachments) or b'\0')
  names=bones+[r[:24].split(b'\0')[0] for r in spheres]+[r[:68].split(b'\0')[0] for r in attachments]
  results={}
  for query in list(dict.fromkeys(names+[b'eye',b'spine',b'__missing_tag__'])):
   u.mem_write(b+0x9000,query+b'\0');u.mem_write(stack,w(stop,b+0x9000));u.mem_write(0x20852f4,w(0))
   u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b)
   u.emu_start(0x51d5b0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
   got=u.reg_read(UC_X86_REG_EAX);want=next((i for i,name in enumerate(names) if name.lower()==query.lower()),0xffffffff)
   assert got==want,(entry['name'],query,got,want);queries+=1
   results[query.decode('cp1252')]=-1 if got==0xffffffff else got
  corpse_tags={}
  for query in (b'eye',b'spine'):
   # Omitting cached CSPH tags is valid for these two source-effect queries
   # only when it preserves the selected bone/LOD attachment or missing result.
   assert not any(raw[:24].split(b'\0')[0].lower()==query for raw in spheres)
   original=results[query.decode()]
   file_names=bones+[raw[:68].split(b'\0')[0] for raw in attachments]
   selected=next((i for i,name in enumerate(file_names) if name.lower()==query),-1)
   if original<0:
    u.mem_write(b+0x9000,query+b'\0');u.mem_write(stack,w(stop,b+0x9000))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b)
    u.emu_start(0x51d690,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    original=u.reg_read(UC_X86_REG_EAX);original=-1 if original==0xffffffff else original
    selected=next((i for i,name in enumerate(bones) if query in name),-1)
    normalized=original
   else:normalized=original-len(spheres) if original>=len(bones)+len(spheres) else original
   assert selected==normalized,(entry['name'],query,original,selected)
   corpse_tags[query.decode()]=dict(original_index=original,file_view_index=selected)
  records.append(dict(model=entry['name'],bones=len(bones),cached_spheres=len(spheres),lod0_attachments=len(attachments),lookups=results,corpse_tags=corpse_tags))
report=dict(result='PASS',original_sha256=digest,models=len(records),cached_spheres=sum(r['cached_spheres'] for r in records),lookup_cases=queries,corpse_query_cases=2*len(records),
 scope='Original cached-tag initialization slice with supplied asset-derived submesh arrays, unchanged getters/matrix helpers and original lookup/comparator. Header legacy attachment count zero in all installed95 models. Cached group gets CSPH names and center-only local transforms; parent words remain supplied sentinels. The190 eye/spine exact-plus-bone-substring queries select the same records when cached tags are omitted and indices are translated. Does not establish full loader or later parent binding, cached tag pose, shared C/NXDK equivalence or live death integration.',records=records)
(root/'artifacts/model-cached-tags.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='records'})
