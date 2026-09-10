"""Shared climb transition against original entry fixtures, PC and NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
subprocess.run([sys.executable,str(root/'tools/inspect_climb_entry.py')],cwd=root,check=True)
reference=json.loads((root/'artifacts/climb-entry-reference.json').read_text());commands=[];expected=[]
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v)
for case in reference['results']:
 climb=case['climb'];free=case['movement'] in (3,8) or (case['kind']==1 and case['attachment']==-1)
 commands.append(w(climb,int(free),case['region_kind'],case['descriptor_enabled']))
 emit='effect_18' in case['events'];selected=2 if case['descriptor_enabled'] else 0
 expected.append(w(*([1,1,1,0,18,0] if emit else [0]*6),climb,climb,selected if climb else 1,climb,climb,selected if climb else 77))
probe=root/'build/pc/Release/rf_entity_probe.exe'
assert subprocess.check_output([str(probe),'--climb-enter'],input=b''.join(commands))==b''.join(expected)
binary=root/'build/xbox/main.exe';pe=pefile.PE(str(binary));im=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(im)+4095)//4096*4096);x.mem_write(origin,im)
base=0x30000000;x.mem_map(base,65536)
state,inp,region,table,config,selected,callback,stack,stop=[base+n for n in (0,0x1000,0x2000,0x3000,0x4000,0x5000,0xc000,0xe000,0xf000)]
entry=int(re.search(r'_rf_player_climb_enter\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(callback,b'\xc3');events=[]
def hook(cpu,address,size,data):
 if address!=callback:return
 sp=cpu.reg_read(UC_X86_REG_ESP);context,s,request=struct.unpack('<3I',cpu.mem_read(sp+4,12))
 assert s==state
 assert bytes(cpu.mem_read(state,32))==w(0,region,table+32,0,123,0,0,0)
 assert bytes(cpu.mem_read(request,32))==w(0,18,0,0,0,0x3f800000,0,0)
 events.append('sound')
x.hook_add(UC_HOOK_CODE,hook)
for data,case in zip(commands,reference['results']):
 climb,free,kind,enabled=struct.unpack('<4I',data)
 initial=w(region,0,table+32,0,123,0,0,0)
 x.mem_write(state,initial);x.mem_write(region,w(kind)+bytes(60));x.mem_write(table,bytes(512));x.mem_write(table+64,w(enabled))
 x.mem_write(config,w(climb*4)+f(3.5,0,0,0,0,0));x.mem_write(selected,w(77))
 x.mem_write(inp,w(region,table,config,-1,0x3f800000,free,0)+w(0,1,0)+bytes(24))
 x.mem_write(stack,w(stop,state,inp,selected,callback,0));x.reg_write(UC_X86_REG_ESP,stack);events.clear();x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 choice=2 if enabled else 0
 want=w(0,region,table+choice*32,region+16,-1,0,0x40600000,1) if climb else initial
 assert bytes(x.mem_read(state,32))==want
 assert bytes(x.mem_read(selected,4))==w(choice if climb else 77)
 assert events==(['sound'] if 'effect_18' in case['events'] else [])
guards=0
for missing_sound,bad_config in ((True,False),(False,True)):
 initial=w(region,0,table+32,0,123,0,0,0)
 x.mem_write(state,initial);x.mem_write(region,w(2)+bytes(60));x.mem_write(selected,w(77))
 x.mem_write(config,w(4)+f(float('nan') if bad_config else 3.5,0,0,0,0,0))
 x.mem_write(inp,w(region,table,config,-1,0x3f800000,1,0)+w(0,1,0)+bytes(24))
 x.mem_write(stack,w(stop,state,inp,selected,0 if missing_sound else callback,0));x.reg_write(UC_X86_REG_ESP,stack)
 events.clear();x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0
 assert bytes(x.mem_read(state,32))==initial and bytes(x.mem_read(selected,4))==w(77) and not events
 guards+=1
report=dict(result='PASS',cases=len(commands),invalid_guards=guards,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
 scope='Shared climb transition matches 768 original fixtures, including capability, free-motion/kind sound condition, intermediate callback state, normal speed, descriptor fallback, region orientation and contact reset. No audio backend or live climb traversal.')
(root/'artifacts/climb-entry-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(commands),'shared PC/NXDK climb transitions')
