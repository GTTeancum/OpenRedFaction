"""Compiled NXDK controller adapter with explicitly simulated SDL devices.
No OS input; does not validate real USB hardware or SDL's internal driver.
"""
import json,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
p=pefile.PE(str(root/'build/xbox/main.exe'));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,raw)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000);u.reg_write(UC_X86_REG_FPCW,0x37f)
mapping=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
names=['SDL_InitSubSystem','SDL_QuitSubSystem','SDL_PollEvent','SDL_GameControllerGetAttached','SDL_GameControllerClose','SDL_NumJoysticks','SDL_IsGameController','SDL_GameControllerOpen','SDL_GameControllerUpdate','SDL_GameControllerGetAxis','SDL_GameControllerGetButton']
hooks={symbol(n):n for n in names};connected=True;axes=[0]*4;buttons=set();calls=[]
def hook(m,addr,size,data):
 if addr not in hooks:return
 name=hooks[addr];calls.append(name);sp=m.reg_read(UC_X86_REG_ESP);ret,arg0,arg1=struct.unpack('<3I',m.mem_read(sp,12));value=0
 if name in ['SDL_GameControllerGetAttached','SDL_NumJoysticks','SDL_IsGameController']:value=int(connected)
 if name=='SDL_GameControllerOpen':value=base+0x200
 if name=='SDL_GameControllerGetAxis':value=axes[arg1]&0xffffffff
 if name=='SDL_GameControllerGetButton':value=int(arg1 in buttons)
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
def invoke(name,*args):
 u.mem_write(stack,struct.pack('<%dI'%(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(symbol(name),stop,count=200000);assert u.reg_read(UC_X86_REG_EIP)==stop
 return struct.unpack('<i',struct.pack('<I',u.reg_read(UC_X86_REG_EAX)))[0]
assert invoke('rf_xbox_input_open')==0
cases=[([0,0,0,0],True,set()),([4000,-4000,4000,-4000],True,set()),([32767,0,32767,0],True,set()),([-32768,0,-32768,0],True,set()),([0,-32768,0,-32768],True,{1}),([32767,32767,32767,32767],True,set()),([32767]*4,False,{1}),([0]*4,True,set()),([0]*4,True,{4,6})]
for frame,(axes,connected,buttons) in enumerate(cases):
 calls.clear();u.mem_write(base,b'\xa5'*24);status=invoke('rf_xbox_input_poll',0,frame,base);v=struct.unpack('<5fI',u.mem_read(base,24))
 assert status==(-3 if frame==8 else 0),(frame,status)
 assert all(-1.000001<=x<=1.000001 for x in v[:5])
 if frame in [0,1,6,7,8]:assert v==(0,0,0,0,0,0),(frame,v)
 if frame==2:assert v==(1,0,0,0,1,0),v
 if frame==3:assert v==(-1,0,0,0,-1,0),v
 if frame==4:assert v==(0,0,1,1,0,1),v
 if frame==5:assert abs(v[0]**2+v[2]**2-1)<1e-6 and abs(v[3]**2+v[4]**2-1)<1e-6
 if frame==6:assert 'SDL_GameControllerClose' in calls and 'SDL_GameControllerGetAxis' not in calls
 if frame==7:assert 'SDL_GameControllerOpen' in calls
invoke('rf_xbox_input_close')
assert invoke('rf_xbox_input_poll',0,10,base)==-4
report=dict(result='PASS',cases=len(cases),scope='Compiled NXDK adapter with simulated SDL API returns: deadzone, axis extrema, diagonal normalization, crouch, disconnect/reconnect, Back+Start clean stop, closed guard. No hardware or host input.')
(root/'artifacts/xbox-input-adapter-verification.json').write_text(json.dumps(report,indent=2));print(report)
