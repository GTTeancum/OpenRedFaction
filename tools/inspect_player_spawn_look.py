"""Execute original entity-factory look initialization for installed spawn matrices.

Uses parsed run descriptor and supplied physics orientation, not full entity creation.
All original callees execute unmodified; no emulation hooks.
"""
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path
import pefile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_FPCW

exe=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
ENTITY,MODE,STACK=[0x30000000+i*0x10000 for i in range(3)]
for address in (ENTITY,MODE,STACK):u.mem_map(address,65536)
descriptor=subprocess.check_output([str(ROOT/'build/pc/Release/rf_movement_probe.exe'),
    '--descriptor',str(ROOT/'Installed_Game/tables.vpp'),'1'])
assert len(descriptor)==32
u.mem_write(MODE,descriptor)
starts=json.loads((ROOT/'artifacts/player-start-verification.json').read_text())['levels']
cases=[]
for start in starts:
    orientation=struct.pack('<9I',*start['transform_words'][3:])
    u.mem_write(ENTITY,b'\xa5'*0x1500)
    u.mem_write(ENTITY+0x48,orientation)
    # Generic factory supplies its copied physics orientation at entity+fc.
    # Supplying the same spawn matrix here is an explicit fixture precondition.
    u.mem_write(ENTITY+0xfc,orientation)
    u.mem_write(ENTITY+0x858,struct.pack('<I',MODE))
    u.reg_write(UC_X86_REG_ESI,ENTITY);u.reg_write(UC_X86_REG_EBX,0)
    u.reg_write(UC_X86_REG_ESP,STACK+0x8000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x422e2c,0x422e82,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==0x422e82
    assert u.reg_read(UC_X86_REG_ESP)==STACK+0x8000
    angles=bytes(u.mem_read(STACK+0x8050,12))
    body=bytes(u.mem_read(ENTITY+0x864,12));eye=bytes(u.mem_read(ENTITY+0x87c,12))
    assert body[:4]==bytes(4) and body[8:]==bytes(4) and body[4:8]==angles[4:8]
    assert bytes(u.mem_read(ENTITY+0x48,36))==orientation
    cases.append(dict(file=start['file'],matrix_words=start['transform_words'][3:],
        extracted_angles=list(struct.unpack('<3f',angles)),body_angles=list(struct.unpack('<3f',body)),
        eye_angles=list(struct.unpack('<3f',eye)),body_words=list(struct.unpack('<3I',body)),
        eye_words=list(struct.unpack('<3I',eye))))
report=dict(result='PASS',original_sha256=sha,run_descriptor_words=list(struct.unpack('<8I',descriptor)),
    scope='Original 422e2c..422e82 and every callee, no hooks, FPCW037f. Verified level matrices and parsed run descriptor supplied; physics orientation equals spawn orientation by fixture. No complete factory, first look tick or reconstructed C comparison.',
    nonzero_body_yaw=sum(c['body_angles'][1]!=0 for c in cases),cases=cases)
(ROOT/'artifacts/player-spawn-look.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='cases'},indent=2))
print('L1S1:',next(c for c in cases if c['file'].lower()=='l1s1.rfl'))
