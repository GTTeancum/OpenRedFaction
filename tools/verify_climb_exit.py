"""PC/NXDK climb exit compared with original exit fixtures."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
subprocess.run([sys.executable,str(root/'tools/inspect_climb_exit.py')],cwd=root,check=True)
cases=json.loads((root/'artifacts/climb-exit-reference.json').read_text())['results']
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v)
commands=[];expected=[]
for c in cases:
 walk,crouch,blocked,index,enabled,forced=[c[k] for k in ('walk','crouched','blocked','default_index','enabled','forced_action')]
 stopped=walk and crouch and blocked;changed=walk and not stopped
 selected=(index if enabled else 0) if changed and index>=0 else 77
 speed=0 if stopped else 1.75 if forced!=-1 else 3.5
 commands.append(w(walk,crouch,blocked,index,enabled,forced))
 expected.append(w(int(changed),1,(-1 if index<0 else selected) if changed else 2,int(changed),123,0 if stopped or forced!=-1 else 1,selected,0 if changed else 0x40e00000,int(walk and crouch))+f(speed))
probe=root/'build/pc/Release/rf_entity_probe.exe'
assert subprocess.check_output([str(probe),'--climb-exit'],input=b''.join(commands))==b''.join(expected)
binary=root/'build/xbox/main.exe';pe=pefile.PE(str(binary));im=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(im)+4095)//4096*4096);x.mem_write(origin,im)
base=0x30000000;x.mem_map(base,65536)
state,inp,region,table,config,selected,identity,callback,stack,stop=[base+n for n in (0,0x1000,0x2000,0x3000,0x4000,0x5000,0x6000,0xc000,0xe000,0xf000)]
entry=int(re.search(r'_rf_player_climb_exit\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(callback,b'\xc3');calls=0;blocked=0
initial=w(region,region,table+64,0,123,0,0,0,0x40e00000)
def hook(cpu,address,size,data):
 global calls
 if address!=callback:return
 sp=cpu.reg_read(UC_X86_REG_ESP);context,stood=struct.unpack('<2I',cpu.mem_read(sp+4,8))
 assert bytes(cpu.mem_read(state,36))==initial
 cpu.mem_write(stood,w(not blocked));cpu.reg_write(UC_X86_REG_EAX,0);calls+=1
x.hook_add(UC_HOOK_CODE,hook)
for data,record in zip(commands,expected):
 walk,crouch,blocked,index,enabled,forced=struct.unpack('<6i',data)
 x.mem_write(state,initial);x.mem_write(table,bytes(512));x.mem_write(selected,w(77))
 if index>=0:x.mem_write(table+index*32,w(enabled))
 x.mem_write(config,w(walk)+f(3.5,.5,0,0,0,0))
 x.mem_write(inp,w(config,table,identity,index,forced,0x3f800000,crouch,0))
 x.mem_write(stack,w(stop,state,inp,selected,callback,0));x.reg_write(UC_X86_REG_ESP,stack);calls=0
 x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 result=struct.unpack('<10I',record);movement=0 if result[2]==0xffffffff else table+result[2]*32
 want=w(0 if result[0] else region,region,movement,identity if result[3] else 0,123,0,result[9],result[5],result[7])
 assert bytes(x.mem_read(state,36))==want and bytes(x.mem_read(selected,4))==w(result[6]) and calls==result[8]
report=dict(result='PASS',cases=len(cases),pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Shared exit matches original resolved branch/state fixtures, including blocked preservation, callback timing, speed, default movement/fallback, identity and step offset. Name resolution and standing world effects supplied; no live campaign climbing.')
(root/'artifacts/climb-exit-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(cases),'shared PC/NXDK climb exits')
