"""Two hits on a retained scene NPC versus original4892c0 and41a350."""
import hashlib,json,os,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
folder=root/'artifacts/npc-damage-binding';folder.mkdir(exist_ok=True)
env=dict(os.environ)
for key in ['RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID']:env.pop(key,None)
env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_DOOR_START='1',RF_REPLAY_DAMAGE_UID='8456')
run=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(root/'Installed_Game'),str(root/'artifacts/door-audio-reference/inputs.bin'),str(folder/'pc.ppm')],env=env,cwd=root,capture_output=True,text=True,check=True)
(folder/'pc.txt').write_text(run.stdout)
a=next(list(map(int,l.split()[1:])) for l in run.stdout.splitlines() if l.startswith('NPC_DAMAGE_TEST '))
assert len(a)==64 and a[0]==a[63]==0 and a[1]==8456
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,65536);stack=b+0xe000;finish=b+0xf000;stop=finish+6
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def words(addr,*v):u.mem_write(addr,w(*v))
u.mem_write(finish,b'\xd9\x1d'+w(b+0x2000))
words(b+0x24,0);words(b+0x2c,a[2]);words(b+0x294,b+0x4000);words(b+0x7c,a[18])
words(0x7394cc+4*(a[2]&0xffff),b)
# Map the compact persistent record to original fields, with the same class data.
state=a[4:18]
for off,value in zip([0x34,0x38,0x4044,0x4048,0x2c,0x810,0x814,0x4728,0x13d8,0x854,0x1f8,0x73c,0x144c],state[:13]):words(b+off,value)
words(b+0x4724,a[19]);words(b+0x53e8,*a[20:31]);words(0x6460f0,0x3f800000)
u.mem_write(0x64ecb9,bytes(2));u.mem_write(0x6fc4d8,bytes(1));words(0x593e54,1)
notifications=[]
# Only external selected-player predicates and downstream reaction boundaries.
# Object lookup, typed entity lookup, armor immunity and numeric damage run intact.
def observe(m,address,size,ctx):
 if address not in (0x48aaf0,0x42a8e0,0x4895d0,0x428740,0x4196f0,0x407fb0):return
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if address in (0x428740,0x4196f0,0x407fb0):notifications.append(address)
 m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,observe)
results=[]
for index,kind in enumerate((2,-1)):
 words(stack,finish,a[2],0x41200000,0xffffffff,0xffffffff,kind,0,0xffffffff,1)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x4892c0,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 expected=[struct.unpack('<I',u.mem_read(b+off,4))[0] for off in [0x34,0x38,0x4044,0x4048,0x2c,0x810,0x814,0x4728,0x13d8,0x854,0x1f8,0x73c,0x144c]]+[state[13]]
 flags=struct.unpack('<I',u.mem_read(b+0x7c,4))[0];value=struct.unpack('<I',u.mem_read(b+0x2000,4))[0]
 start=31 if index==0 else 47
 assert a[start:start+16]==expected+[flags,value],(index,a[start:start+16],expected+[flags,value])
 results.append(dict(kind=kind,health=struct.unpack('<f',w(expected[0]))[0],armor=struct.unpack('<f',w(expected[1]))[0],returned=struct.unpack('<f',w(value))[0]))
assert len(notifications)==a[3]==6 and results[-1]['health']<100 and results[-1]['armor']<100
report=dict(result='PASS',target_uid=a[1],handle=a[2],hits=results,notifications=notifications,scope='Real registered scene owner and class factors vs full original4892c0/41a350 plus actual object/typed lookup, armor immunity and armor arithmetic. Unselected NPC predicates and pain/AI reaction boundaries supplied. No weapon, audio, AI or lethal/burn claim. Native comparison is separate.')
(folder/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
