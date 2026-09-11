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
camera=symbol('campaign_camera_effect')
for kind,flags,count,deadline in ((0,0,2,1500),(2,0,2,0),(0,1,1,0),(5,0,2,1500)):
    u.mem_write(owner,struct.pack('<4f',100,100,100,100));u.mem_write(view+12,w(8,flags))
    assert call('rf_screen_flash_reset',flash)==0
    assert call('rf_camera_effect_reset',camera,0)==0
    # Effects pointer, difficulty, damage clock bits, status/count/amount, camera clock.
    u.mem_write(base+0x500,struct.pack('<IfIIIfi',base+0x400,100,0x3f800000,0,0,0,1000))
    assert call('rf_scene_event_damage_bind',base+0x500,base+0x600)==0
    u.mem_write(base+0x700,w(handle))
    u.mem_write(base+0x800,struct.pack('<IIiIIf',1,base+0x700,1,kind,handle,.05))
    notifications.clear()
    assert call('rf_event_continuous_damage_action',base+0x800,1,base+0x600)==0
    assert struct.unpack('<II',u.mem_read(base+0x50c,8))==(0,count)
    assert struct.unpack('<i',u.mem_read(camera+8,4))[0]==deadline
    assert struct.unpack('<I',u.mem_read(flash+4,4))[0]==128
    assert not notifications  # Each tiny hit is below both pain thresholds.
for campaign,eye in ((0,0),(0,1),(1,0),(1,1)):
    u.mem_write(symbol('campaign_spawn'),w(campaign));u.mem_write(symbol('rf_scene_actor_eye_enabled'),w(eye))
    u.mem_write(owner,struct.pack('<4f',100,100,100,100));u.mem_write(view+12,w(8,0))
    u.mem_write(base+0x200,struct.pack('<fIiIII',10,0xffffffff,2,0,0xffffffff,0));notifications.clear()
    assert call('rf_scene_player_damage',handle,base+0x200,0x3f800000,0,base+0x400,base+0x300)==0
    assert [item[1] for item in notifications]==([1] if campaign and eye else [0,1])
report=dict(result='PASS',cases=len(cases),event_cases=4,pain_profile_cases=4,scope='Linked NXDK player damage and Continuous_Damage binding with real registration and retained health/flash/camera. Four profile cases suppress flinch only for campaign first-person eye mode while preserving pain sound. Event cases check duplicate hits, exclusion, kind gates and clocks. Direct cases cover scaling, force, death and flash gates. External reactions recorded; no game audio/death or authored hazard activation. Original core equivalence is separately verified.')
(root/'artifacts/player-damage-binding.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
