"""Compare shared4087a0 on PC/NXDK with the full original oracle."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
subprocess.run([sys.executable,str(root/'tools/verify_ai_arbitration_original.py')],check=True)
records=json.loads((root/'artifacts/ai-arbitration-original.json').read_text())['records']
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
bits=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
codes={'global':1,'event':2,'actor':3,'busy':4,'dead':5,'destination':6,'stance':7,'action':8,'state':9}
commands=[];expected=[]
for rec in records:
 c=rec['input'];wire=rec['initial']+[c['scalar_bits'],c['gate'],c['event_present'],c['actor_present'],c['event_type'],c['destination'],c['network'],c['peer_count']]+[bits(v) for v in c['changed_position']]
 for peer in c['peers']:wire.extend(peer[k] for k in ('handle','busy','dead','event','action'))
 commands.append(w(*wire));trace=[]
 for row in rec['trace']:trace.extend([codes[row[0]],*row[1:],*([0]*(3-len(row)))])
 expected.append(w(0,rec['result'],*rec['final'],len(trace)//3,*trace,*([0]*(36-len(trace)))))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-arbitration'],input=b''.join(commands));assert len(actual)==len(records)*196
for i,want in enumerate(expected):assert actual[196*i:196*(i+1)]==want,('PC',i)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase
x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im)
B=0x30000000;x.mem_map(B,0x10000);ACTORS=[B,B+0x100,B+0x200];EVENT=B+0x500;FRAME=B+0x600;LIST=B+0x700;BE=B+0x800;OUT=B+0x900;STACK=B+0xe000;STOP=B+0xf000;CB=[STOP+0x100+i*0x100 for i in range(6)]
entry=int(re.search(r'\s_rf_entity_ai_arbitrate\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
cfg={};trace=[];calls=0;failure=-1
def hook(cpu,address,size,context):
 global calls
 if address not in CB:return
 calls+=1;sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);status=0
 if failure==calls:status=0xffffffff
 elif address==CB[0]:trace.extend((1,cfg['gate'],0));cpu.mem_write(arg(1),w(cfg['gate']))
 elif address==CB[1]:
  assert arg(1)==77;trace.extend((2,77,0));cpu.mem_write(arg(2),w(EVENT if cfg['event_present'] else 0))
 elif address==CB[2]:
  assert arg(1)==55;trace.extend((3,55,0));cpu.mem_write(arg(2),w(B if cfg['actor_present'] else 0))
 elif address==CB[3]:
  i=ACTORS.index(arg(1));op=arg(2);assert op in (0x40a110,0x427020);value=cfg['peers'][i]['busy' if op==0x40a110 else 'dead'];trace.extend((4 if op==0x40a110 else 5,i,value));cpu.mem_write(arg(3),w(value))
 elif address==CB[4]:
  assert arg(1)==55 and arg(2)==EVENT+4;trace.extend((6,55,cfg['destination']));cpu.mem_write(arg(3),w(cfg['destination']))
 else:
  assert arg(1)==B;trace.extend((7,0,0));cpu.mem_write(EVENT+4,struct.pack('<3f',*cfg['changed_position']))
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook)
def setup(rec):
 global cfg,trace,calls
 cfg=rec['input'];trace=[];calls=0
 for i,a in enumerate(ACTORS):
  peer=cfg['peers'][i];x.mem_write(a,bytes(56));x.mem_write(a,w(a,peer['action']));x.mem_write(a+32,w(peer['handle'],peer['event']))
 x.mem_write(B+4,w(*rec['initial'][:7]));x.mem_write(B+40,w(cfg['scalar_bits'],*rec['initial'][7:]));x.mem_write(EVENT,w(cfg['event_type'],bits(1),bits(2),bits(3)))
 x.mem_write(FRAME,w(bits(cfg['clock']),cfg['network']&1,cfg['network']>>1,LIST,cfg['peer_count']));x.mem_write(LIST,w(*ACTORS));x.mem_write(BE,w(*CB,0));x.mem_write(OUT,w(123))
 x.mem_write(STACK,w(STOP,B,FRAME,BE,OUT));x.reg_write(UC_X86_REG_ESP,STACK)
def run():
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
for i,rec in enumerate(records):
 setup(rec);peers=bytes(x.mem_read(ACTORS[1],0x138));status=run();result=r(OUT)
 if not status and result:trace.extend((8,16,0,9,1,0))
 actual=w(status,result)+bytes(x.mem_read(B+4,28))+bytes(x.mem_read(B+44,12))+w(len(trace)//3,*trace,*([0]*(36-len(trace))))
 assert actual==expected[i],('NXDK',i)
 assert bytes(x.mem_read(ACTORS[1],0x138))==peers
rec=next(rec for rec in records if rec['result'] and any(t[0]=='dead' for t in rec['trace']))
setup(rec);run();boundary_count=calls
for failure in range(1,boundary_count+1):
 setup(rec);before=bytes(x.mem_read(B,56));assert run()==0xffffffff and calls==failure and r(OUT)==123
 assert bytes(x.mem_read(B,56))==before
failure=-1
for bad in (0x7fc00000,0x7f800000,0xff800000,0x4f000000):
 setup(rec);x.mem_write(FRAME,w(bad));before=bytes(x.mem_read(B,56));assert run()==0xfffffffc and r(OUT)==123 and not calls and bytes(x.mem_read(B,56))==before
report=dict(result='PASS',original_pc_nxdk_cases=len(records),accepted=sum(r['result'] for r in records),callback_failures=boundary_count,clock_guards=4,scope='Shared complete4087a0 retained state and ordered boundary trace match original on PC and compiled NXDK. Includes post-stance event-position mutation, exact gate bytes and unchanged peers. External service implementations and live AI scheduling excluded.')
(root/'artifacts/ai-arbitration-shared.json').write_text(json.dumps(report,indent=2));print(report)
