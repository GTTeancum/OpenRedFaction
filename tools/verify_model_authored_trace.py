"""Original54e000 traversal/preparation versus PC/NXDK; only54daa0 supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
b=0x30000000;model=b+0x1000;count=model+0x48;query=b+0x2000;hit=b+0x3000;backend=b+0x4000;callback=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,0x200000);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
from inspect_models import inspect
u=machine(exe);u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
model=b;partbase=b+0x1000;tablebase=b+0x4000;lodbase=b+0x8000;rawbase=b+0x10000;query=b+0x1f0000;hit=b+0x1f1000;stack=b+0x1fe000;stop=b+0x1ff000
models=cases=hits=0
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for resource in archive.get('vpp',{}).get('entries',[]):
  if not resource['name'].lower().endswith('.v3m'):continue
  path=root/'Installed_Game'/archive['path']
  with path.open('rb') as stream:stream.seek(resource['offset']);raw=stream.read(resource['size'])
  sections=[s for s in inspect(raw)['sections'] if 'lods' in s];assert len(raw)<0x1d0000 and len(sections)*0x90<0x3000
  u.mem_write(rawbase,raw);u.mem_write(model+0x48,w(len(sections),partbase));lod_index=0;metadata=[]
  for part,section in enumerate(sections):
   count=len(section['lods']);table=tablebase+part*0x80;u.mem_write(partbase+part*0x90+0x8c,w(table));u.mem_write(table,w(count))
   offset=section['offset']+8+56+count*4;meta=raw[offset:offset+40];metadata.append(struct.unpack('<10f',meta));u.mem_write(table+0x1c,meta)
   for local,lod in enumerate(section['lods']):
    lp=lodbase+lod_index*0x50;lod_index+=1;assert lp+0x50<=rawbase
    u.mem_write(table+4+local*4,w(lp));u.mem_write(lp,b'\x00'*0x44);u.mem_write(lp+8,w(rawbase+lod['data_offset']));u.mem_write(lp+12,struct.pack('<H',lod['batches']));u.mem_write(lp+0x40,w(lod['flags']))
    start=lod['data_offset'];relative=(lod['batches']*56+15)&~15
    for i in range(lod['batches']):
     v,t,p,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',raw,start+lod['data_bytes']+4+i*18)
     sizes=[p,p,uv,ix,t*16,extra,links,lod['unknown']*2 if lod['flags']&1 else 0];regions=[]
     for size in sizes:regions.append(rawbase+start+relative if size else 0);relative=(relative+size+15)&~15
     batch=rawbase+start+i*56
     for field,region in [(4,0),(8,1),(12,2),(16,4),(20,3),(24,5),(28,6),(36,7)]:u.mem_write(batch+field,w(regions[region]))
     u.mem_write(batch+40,struct.pack('<7H',v,t,p,ix,uv,links,extra))
  commands=[];answers=[]
  for part,meta in enumerate(metadata):
   for axis in range(3):
    for direction in (-1,1):
     for radius in (0,.25):
      lo=meta[4:7];hi=meta[7:10];start=[(lo[j]+hi[j])*.5+meta[j] for j in range(3)];delta=[0,0,0]
      start[axis]=(lo[axis]-1 if direction==1 else hi[axis]+1)+meta[axis];delta[axis]=direction*(hi[axis]-lo[axis]+2)
      q=f([0,0,0,1,0,0,0,1,0,0,0,1]+start+delta+[radius])+w(2)+b'\xa5'*24;initial=f([1,11,12,13,14,15,16])+w(0x12345678);commands.append(q+initial+w(0))
      u.mem_write(query,q);u.mem_write(hit,initial);u.mem_write(stack,w(stop,query,hit,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,model)
      u.emu_start(0x54e000,stop,count=2000000);assert u.reg_read(UC_X86_REG_EIP)==stop
      accepted=u.reg_read(UC_X86_REG_EAX)&255;result=bytearray(u.mem_read(hit,32))
      if accepted:
       token=struct.unpack_from('<I',result,28)[0];assert rawbase<=token<rawbase+len(raw);struct.pack_into('<I',result,28,token-rawbase)
      answers.append(w(accepted)+bytes(u.mem_read(query,104))+result);hits+=accepted
  actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(path),resource['name'],'--collision-trace'],input=b''.join(commands))
  assert len(actual)==len(answers)*140
  for n,expected in enumerate(answers):assert actual[n*140:n*140+140]==expected,(resource['name'],n,expected.hex(),actual[n*140:n*140+140].hex())
  models+=1;cases+=len(answers)
report=dict(result='PASS',models=models,cases=cases,hits=hits,original_sha256=sha,scope='Every shipped static model through complete original54e000 and actual geometry callees versus PC archive-loaded owned resource.12 axis-aligned thin/sphere sweeps per authored part; exact query/result/return with original pointers normalized to file-offset tokens. Original layout rebuilt from stored LOD bytes; no original loader or native XEMU claim.')
(root/'artifacts/model-authored-trace.json').write_text(json.dumps(report,indent=2));print(report)
