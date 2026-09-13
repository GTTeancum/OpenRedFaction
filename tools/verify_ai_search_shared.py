"""Shared bounded4ce8c0 search and4ceb50 paths versus original PC/NXDK."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
subprocess.run([sys.executable,str(root/'tools/verify_ai_search_original.py')],check=True)
records=json.loads((root/'artifacts/ai-search-original.json').read_text())['records']
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
bits=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
commands=[];expected=[]
def callback_hash(trace):
 h=2166136261
 for row in trace:
  if row[0] not in ('visible','edge'):continue
  for word in [1 if row[0]=='visible' else 2,*row[1:]]:h=((h^word)*16777619)&0xffffffff
 return h
for rec in records:
 c=rec['input'];n=c['count'];cmd=w(n,c['goal'],c['alternate'],bits(c['limit']))+b''.join(w(*v) for v in rec['initial_node_words'])+bytes((8-n)*68)
 cmd+=w(*[len(v) for v in c['neighbors']],*([0]*(8-n)))
 cmd+=b''.join(w(*v,*([0]*(8-len(v)))) for v in c['neighbors'])+bytes((8-n)*32)
 cmd+=w(*c['visible'],*([0]*(8-n)))+b''.join(w(*v,*([0]*(8-n))) for v in c['edges'])+bytes((8-n)*32)
 assert len(cmd)==1136;commands.append(cmd)
 expected.append(w(0,rec['result'],rec['cost_bits'],len(rec['path']),*rec['path'],*([0]*(8-len(rec['path']))),callback_hash(rec['trace']))+b''.join(w(*v) for v in rec['node_words'])+bytes((8-n)*68))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-search'],input=b''.join(commands))
assert len(actual)==596*len(records)
for i,want in enumerate(expected):assert actual[596*i:596*(i+1)]==want,('PC',i,actual[596*i:596*i+52].hex(),want[:52].hex())
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im)
B=0x30000000;x.mem_map(B,0x10000);NODES=[B+i*0x100 for i in range(8)];Q=B+0x1000;REFS=B+0x2000;ADJ=B+0x3000;SCRATCH=B+0x4000;BE=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xf000;CB=[STOP+0x100,STOP+0x200,STOP+0x300]
entry=int(re.search(r'\s_rf_entity_navigation_search\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
cfg={};path=[];trace=[];calls=0;failure=-1

def hook(cpu,address,size,context):
 global calls
 if address not in CB:return
 calls+=1;sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);status=0
 if calls==failure:status=0xffffffff
 elif address==CB[0]:
  i=NODES.index(arg(1));assert arg(2)==(B+0x6000 if cfg['alternate'] else NODES[cfg['goal']]) and arg(3)==(0 if cfg['alternate'] else bits(.1)) and arg(4)==bits(1)
  value=cfg['visible'][i];trace.append(['visible',i,value]);cpu.mem_write(arg(5),w(value))
 elif address==CB[1]:
  assert arg(1)==B+0x6000 and arg(4)==0;i=NODES.index(arg(2)-12);j=NODES.index(arg(3)-12);value=cfg['edges'][i][j];trace.append(['edge',i,j,value]);cpu.mem_write(arg(5),w(value))
 else:path.append(NODES.index(arg(1)))
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook)
def setup(rec):
 global cfg,path,trace,calls
 cfg=rec['input'];path=[];trace=[];calls=0;n=cfg['count']
 for i in range(8):
  x.mem_write(NODES[i],w(*rec['initial_node_words'][i]) if i<n else bytes(68))
  adj=cfg['neighbors'][i] if i<n else [];x.mem_write(ADJ+i*32,w(*adj,*([0]*(8-len(adj)))));x.mem_write(REFS+i*16,w(NODES[i],NODES[i],ADJ+i*32,len(adj)))
 x.mem_write(Q,w(0,cfg['goal'],B+0x6000 if cfg['alternate'] else 0,bits(cfg['limit']),bits(1),0,0x12345678));x.mem_write(BE,w(*CB,0));x.mem_write(OUT,w(99));x.mem_write(STACK,w(STOP,REFS,n,Q,SCRATCH,8,BE,OUT));x.reg_write(UC_X86_REG_ESP,STACK)
def run():
 x.emu_start(entry,STOP,count=200000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
for i,rec in enumerate(records):
 setup(rec);status=run();got=w(status,r(OUT),r(Q+24),len(path),*path,*([0]*(8-len(path))),callback_hash(trace))+b''.join(bytes(x.mem_read(a,68)) for a in NODES)
 assert got==expected[i],('NXDK',i,got[:52].hex(),expected[i][:52].hex())
# Errors preserve result/cost, including output append failures after preceding appends.
rec=max(records,key=lambda r:len([t for t in r['trace'] if t[0] in ('visible','edge')])+len(r['path']))
setup(rec);run();boundary_count=calls
for failure in range(1,boundary_count+1):
 setup(rec);assert run()==0xffffffff and calls==failure and r(OUT)==99 and r(Q+24)==0x12345678
failure=-1
# Exercise every append position explicitly, even if the maximum-call case has no route.
rec=max(records,key=lambda r:len(r['path']));setup(rec);run();append_count=len(path);first_append=calls-append_count+1
assert append_count>1
for index in range(append_count):
 failure=first_append+index;setup(rec);assert run()==0xffffffff and path==rec['path'][:index] and r(Q+24)==0x12345678 and r(OUT)==99
failure=-1
for bad in range(3):
 setup(records[-1]);before=b''.join(bytes(x.mem_read(a,68)) for a in NODES)
 if bad==0:x.mem_write(STACK+20,w(0))
 elif bad==1:x.mem_write(REFS+4,w(0))
 else:x.mem_write(REFS+12,w(1));x.mem_write(ADJ,w(99))
 assert run()!=0 and not calls and r(OUT)==99 and r(Q+24)==0x12345678 and b''.join(bytes(x.mem_read(a,68)) for a in NODES)==before
report=dict(result='PASS',original_pc_nxdk_cases=len(records),callback_failures=boundary_count,append_failures=append_count,preflight_guards=3,scratch_bytes_per_node=4,scope='Bounded shared search matches original node bytes, results, paths, cost and visibility/edge callback order on PC/NXDK. Iterative path reversal reuses open-list scratch. Actual scene visibility, endpoint preparation and route-output allocation remain external.')
(root/'artifacts/ai-search-shared.json').write_text(json.dumps(report,indent=2));print(report)
