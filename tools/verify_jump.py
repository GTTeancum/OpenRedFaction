"""Compare reconstructed PC/NXDK jump state with original execution evidence."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
subprocess.run([sys.executable,str(root/'tools/inspect_jump.py')],cwd=root,check=True)
cases=json.loads((root/'artifacts/jump-original/report.json').read_text())['results']
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
bits=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
commands=[];expected=[]
for c in cases:
 accepted=c['accepted'];selected=(8 if c['alternate_fall'] else 3) if c['descriptor_enabled'] else 0
 commands.append(w(c['mode'],c['actor_flags'],c['physics_flags'],bits(c['velocity']),bits(c['jump_strength']),bits(c['frame_dt']),c['parent_block'],c['alternate_fall'],c['descriptor_enabled'],c['old_time'],456,777))
 expected.append(w(c['final_actor_flags'],c['final_physics_flags'],bits(c['output_velocity']),selected if accepted else -1,accepted,c['final_time'],selected if accepted else 77,accepted,c['old_time'] if accepted else 0,456 if accepted else 0))
probe=root/'build/pc/Release/rf_entity_probe.exe'
actual=subprocess.check_output([str(probe),'--jump'],input=b''.join(commands))
assert actual==b''.join(expected),next((i,actual[i*40:(i+1)*40].hex(),e.hex()) for i,e in enumerate(expected) if actual[i*40:(i+1)*40]!=e)
binary=root/'build/xbox/main.exe';pe=pefile.PE(str(binary));im=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase;pe.close()
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(im)+4095)//4096*4096);x.mem_write(origin,im)
base=0x30000000;x.mem_map(base,65536)
state,inp,old,table,identity,selected,callback,stack,stop=[base+n for n in (0,0x1000,0x2000,0x3000,0x4000,0x5000,0xc000,0xe000,0xf000)]
entry=int(re.search(r'_rf_player_jump\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(callback,b'\xc3');events=[];want_callback=b''
def hook(cpu,address,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP);context,observed,sound=struct.unpack('<3I',cpu.mem_read(sp+4,12))
 assert context==123 and observed==state and sound==456
 assert bytes(cpu.mem_read(state,24))==want_callback
 events.append(sound)
x.hook_add(UC_HOOK_CODE,hook,begin=callback,end=callback)
for command,record in zip(commands,expected):
 mode,flags,physics,velocity,strength,dt,parent,alt,enabled,old_time,sound,now=struct.unpack('<12I',command)
 result=struct.unpack('<10I',record);accepted=result[7]
 x.mem_write(state,w(flags,physics,velocity,old,0,old_time));x.mem_write(old,w(1,mode,0,0,0,0,0,0))
 x.mem_write(table,bytes(512));x.mem_write(table+3*32,w(enabled));x.mem_write(table+8*32,w(enabled));x.mem_write(selected,w(77))
 x.mem_write(inp,w(table,identity,strength,dt,parent,alt,sound,now))
 movement=table+result[3]*32 if accepted else old
 wanted=w(result[0],result[1],result[2],movement,identity if accepted else 0,result[5])
 want_callback=w(result[0],result[1],result[2],movement,identity,old_time)
 x.mem_write(stack,w(stop,state,inp,selected,callback,123));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f);events.clear()
 x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(state,24))==wanted,(command.hex(),bytes(x.mem_read(state,24)).hex(),wanted.hex())
 assert bytes(x.mem_read(selected,4))==w(result[6]) and len(events)==accepted
x.mem_write(stack,w(stop,0,0,0,0,0));x.reg_write(UC_X86_REG_ESP,stack);events.clear();x.emu_start(entry,stop,count=1000)
assert x.reg_read(UC_X86_REG_EAX)==0 and not events
report=dict(result='PASS',cases=len(cases),pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Shared jump state, descriptor fallback and callback-before-time-stamp match original fixtures. NXDK executed in Unicorn, not live XEMU. Resolved predicates/audio boundary supplied; no input dispatch or live jumping.')
(root/'artifacts/jump-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(cases),'shared PC/NXDK jumps')
