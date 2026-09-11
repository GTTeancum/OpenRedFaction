"""Execute linked NXDK player adapter with real port registration and flash owner."""
import json,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
pe=pefile.PE(str(root/'build/xbox/main.exe'));blob=pe.get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(blob)+4095)//4096*4096)
u.mem_write(pe.OPTIONAL_HEADER.ImageBase,blob);base=0x30000000;stack=base+0xe000;stop=base+0xf000
u.mem_map(base,65536);mapping=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
def w(*values):return struct.pack('<'+'I'*len(values),*values)
def call(name,*args):
    u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(symbol(name),stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop,name
    return u.reg_read(UC_X86_REG_EAX)
notifications=[]
def hook(m,a,size,data):
    if not stop+16<=a<stop+144:return
    index=(a-stop-16)//16;sp=m.reg_read(UC_X86_REG_ESP)
    assert index==5,('Unexpected external effect',index)
    notifications.append(struct.unpack('<IIIfI',m.mem_read(sp+4,20)))
    m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4)
    m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0])
u.hook_add(UC_HOOK_CODE,hook)
registry=symbol('campaign_registry');entities=symbol('campaign_entities');view=symbol('campaign_player_view')
owner=symbol('campaign_player_damage');flash=symbol('campaign_player_flash')
# amount,difficulty,kind,force,flags,health,armor,expected result/health/armor/flash.
cases=[(10,.5,2,0,8,100,100,7.5,96.4,96.1,128),
 (10,.5,10,0,8,100,100,5,97.6,97.4,0),
 (10,.5,9,0,8,100,100,10,95.2,94.8,128),
 (10,100,2,1,12,100,100,15,92.8,92.2,128),
 (10,1,2,0,12,100,100,0,100,100,0),
 (10,1,2,256,12,100,100,0,100,100,0),
 (.3,1,-1,0,8,.75,0,.3,-.1,0,128),
 (.3,1,-1,0,8,-.1,0,.3,-.4,0,0)]
for case in cases:
    amount,difficulty,kind,force,flags,health,armor,result,next_health,next_armor,alpha=case
    call('rf_object_registry_init',registry);u.mem_write(entities,bytes(4096));u.mem_write(view,bytes(56))
    u.mem_write(view+12,w(flags));u.mem_write(base+0x100,bytes(12))
    assert call('rf_entity_view_register',registry,entities,view,base+0x100)==0
    handle=struct.unpack('<I',u.mem_read(view,4))[0]
    state=struct.pack('<4f10I',health,armor,100,100,handle,0,0,0,0,0xffffffff,0,0,0xffffffff,0xffffffff)
    u.mem_write(owner,state+struct.pack('<11f',1,1,1.5,1,1,1,1,1,1,1,1)+w(0,flags))
    assert call('rf_screen_flash_reset',flash)==0
    u.mem_write(base+0x200,struct.pack('<fIiIII',amount,0xffffffff,kind,0,0xffffffff,force))
    u.mem_write(base+0x400,w(*(stop+16+i*16 for i in range(8)),0xabc));notifications.clear()
    difficulty_bits=struct.unpack('<I',struct.pack('<f',difficulty))[0]
    assert call('rf_scene_player_damage',handle,base+0x200,difficulty_bits,0x3f800000,base+0x400,base+0x300)==0
    actual=struct.unpack('<2f',u.mem_read(owner,8));actual_result=struct.unpack('<f',u.mem_read(base+0x300,4))[0]
    assert all(abs(a-b)<.00002 for a,b in zip((*actual,actual_result),(next_health,next_armor,result))),(case,actual,actual_result)
    assert struct.unpack('<I',u.mem_read(flash+4,4))[0]==alpha
    assert all(item[0]==0xabc and item[1] in (0,1) and item[2]==handle for item in notifications),notifications
report=dict(result='PASS',cases=len(cases),scope='Linked NXDK rf_scene_player_damage with real registry initialization/registration, retained vitals and flash. External pain notifications recorded; no game audio/death execution. Difficulty, kind9 exception, force low byte, blocked flags, class multipliers, half-point death and flash gates. Original core equivalence is separately covered by damage verifiers.')
(root/'artifacts/player-damage-binding.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
