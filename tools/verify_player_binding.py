"""Original local-player binding versus shared C; weapon selection is observed only."""
import hashlib
import itertools
import json
import re
import struct
import subprocess
import sys
from pathlib import Path
import pefile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX

exe=ROOT/'Installed_Game/RF.exe'
sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
ENTITY,PLAYER,STACK,STOP=[0x30000000+i*0x10000 for i in range(4)]
for a in (ENTITY,PLAYER,STACK,STOP):u.mem_map(a,65536)

def put(a,*values):u.mem_write(a,struct.pack('<'+'I'*len(values),*[x&0xffffffff for x in values]))
def word(a):return struct.unpack('<I',u.mem_read(a,4))[0]

observations=[]
def hook(uc,address,size,context):
    if address!=0x4a4980:return
    sp=uc.reg_read(UC_X86_REG_ESP)
    ret,player,weapon=struct.unpack('<3I',uc.mem_read(sp,12))
    assert player==PLAYER
    assert word(0x5cb054)==ENTITY and word(0x5af45c)==ENTITY+0x2a0
    observations.append(struct.pack('<9I',*[word(ENTITY+x) for x in (0x24,0x2c,0x1f8,0x560,0x2a4)],
                                    word(PLAYER+0x14),weapon,1,1))
    # cdecl boundary: caller removes weapon arguments. No weapon behavior stubbed in.
    uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
inputs=bytearray();reports=[]
for kind,handle,weapon,state in itertools.product((0,1,-1),(-1,0,0xabcd03ff),(-1,0,5),(7,-1,0x12345678)):
    before=bytearray(b'\xa5'*0x1500)
    values=(kind,handle,0x76543210,state,weapon)
    for offset,value in zip((0x24,0x2c,0x1f8,0x560,0x2a4),values):
        struct.pack_into('<I',before,offset,value&0xffffffff)
    u.mem_write(ENTITY,bytes(before));u.mem_write(PLAYER,b'\x5a'*0x1100)
    put(0x7c75d4,PLAYER);put(0x5cb054,0);put(0x5af45c,0)
    put(STACK+0x8000,STOP,ENTITY);u.reg_write(UC_X86_REG_ESP,STACK+0x8000)
    prior=len(observations);u.emu_start(0x4a40f0,STOP,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==STOP and len(observations)==prior+1
    struct.pack_into('<I',before,0x1f8,2)
    if kind==0:struct.pack_into('<I',before,0x560,0xffffffff)
    assert bytes(u.mem_read(ENTITY,len(before)))==before
    player_expected=bytearray(b'\x5a'*0x1100);struct.pack_into('<I',player_expected,0x14,handle&0xffffffff)
    assert bytes(u.mem_read(PLAYER,len(player_expected)))==player_expected
    inputs.extend(struct.pack('<5I',*[x&0xffffffff for x in values]))
    reports.append(dict(type=kind,handle=handle,weapon=weapon,initial_state=state,result='PASS'))

probe=ROOT/'build/pc/Release/rf_entity_probe.exe'
actual=subprocess.check_output([str(probe),'--player-bind'],input=inputs)
assert actual==b''.join(observations),'Compiled binding differs at weapon callback boundary'
nxdk_verified=False
if '--nxdk' in sys.argv:
    pe=pefile.PE(str(ROOT/'build/xbox/main.exe'));data=pe.get_memory_mapped_image()
    nx=Uc(UC_ARCH_X86,UC_MODE_32)
    nx.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096)
    nx.mem_write(pe.OPTIONAL_HEADER.ImageBase,data);nx.mem_map(STACK,65536)
    address=int(re.search(r'_rf_player_bind_local\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
    recorded=[]
    def selected(uc,pc,size,context):
        if pc!=STACK+0xf100:return
        sp=uc.reg_read(UC_X86_REG_ESP)
        ret,ctx,local,weapon=struct.unpack('<4I',uc.mem_read(sp,16))
        assert ctx==0x12345678 and local==STACK+32
        entity,inventory,handle=struct.unpack('<3I',uc.mem_read(local,12))
        assert entity==STACK and inventory==STACK+16
        recorded.append(bytes(uc.mem_read(entity,20))+struct.pack('<4I',handle,weapon,1,1))
        uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
    nx.hook_add(UC_HOOK_CODE,selected)
    for offset in range(0,len(inputs),20):
        nx.mem_write(STACK,bytes(inputs[offset:offset+20]));nx.mem_write(STACK+32,bytes(12))
        nx.mem_write(STACK+0x8000,struct.pack('<5I',STACK+0xf000,STACK+32,STACK,STACK+0xf100,0x12345678))
        nx.reg_write(UC_X86_REG_ESP,STACK+0x8000)
        nx.emu_start(address,STACK+0xf000,count=10000)
        assert nx.reg_read(UC_X86_REG_EIP)==STACK+0xf000 and nx.reg_read(UC_X86_REG_EAX)==0
    assert recorded==observations,'Compiled NXDK binding differs at weapon callback'
    nxdk_verified=True
report=dict(result='PASS',original_sha256=sha,pc_probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
            nxdk_verified=nxdk_verified,nxdk_sha256=hashlib.sha256((ROOT/'build/xbox/main.exe').read_bytes()).hexdigest() if nxdk_verified else None,
            scope='Full original 4a40f0/489f70/4895f0; 4a4980 observed and returned without weapon behavior. Shared callback observations compared. No factory or live ownership integration.',cases=reports)
(ROOT/'artifacts/player-binding-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'PASS: {len(reports)} original local-player bindings match compiled C at weapon callback; NXDK={nxdk_verified}')
