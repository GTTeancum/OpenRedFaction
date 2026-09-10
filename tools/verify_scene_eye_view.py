"""Replay every PC diagnostic eye frame through original eye/pose instructions."""
import argparse, hashlib, json, struct, sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EIP,UC_X86_REG_FPCW
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);args=p.parse_args()
exe=root/'Installed_Game/RF.exe';fingerprint=hashlib.sha256(exe.read_bytes()).hexdigest()
assert fingerprint=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,cls,info,camera,stack,stop=[0x30000000+i*0x10000 for i in range(6)]
for address in (entity,cls,info,camera,stack,stop):u.mem_map(address,65536)
def put(address,*words):u.mem_write(address,struct.pack('<%dI'%len(words),*words))
put(entity+0x29c,cls);put(entity+0x294,info)
rows=[list(map(int,line.split()[1:])) for line in args.trace.read_text().splitlines() if line.startswith('EYE_FRAME ')]
assert len(rows)==664
look_rows=[list(map(int,line.split()[1:])) for line in args.trace.read_text().splitlines() if line.startswith('LOOK_FRAME ')]
if look_rows:assert len(look_rows)==len(rows)
look_state=bytes(56)
look_offsets=[0x708,0x70c,0x710,0x728,0x724,0x864,0x868,0x86c,0x87c,0x880,0x884,0x150,0x154,0x158]
transitions=0
for frame,row in enumerate(rows):
    assert len(row)==46 and row[0]==frame
    raw=struct.pack('<46I',*row);data=raw[4:100]
    if look_rows:
        assert struct.unpack("<9f",data[12:48])==(1,0,0,0,1,0,0,0,1), ("pitch fixture body orientation",frame)
        look=look_rows[frame];assert len(look)==33 and look[0]==frame
        command=(.25 if frame%180<90 else -.25) if frame else 0
        before=struct.pack('<f',command)+look_state[4:]
        for j,offset in enumerate(look_offsets):u.mem_write(entity+offset,before[j*4:j*4+4])
        u.mem_write(info+0x64,struct.pack('<f',1));u.mem_write(entity+0x1b0,struct.pack('<f',1/60))
        put(stack+64000,stop,entity);u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
        u.emu_start(0x49de50,0x49e02c,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49e02c
        look_state=b''.join(bytes(u.mem_read(entity+offset,4)) for offset in look_offsets)
        original=look_state+bytes(u.mem_read(entity+0x48,36))+bytes(u.mem_read(entity+0x7e0,36))
        assert struct.pack('<32I',*look[1:])==original,('look pose',frame)
        assert raw[148:184]==original[92:128],('bound eye orientation',frame)
    else:u.mem_write(entity+0x7e0,data[12:48])
    u.mem_write(entity+0x3c,data[:12]);u.mem_write(entity+0x48,data[12:48])
    u.mem_write(cls+0x9c,data[48:72]);u.mem_write(info+0x728,data[72:76]);u.mem_write(cls+0x1ec,data[76:80])
    u.mem_write(entity+0x138c,data[80:96]);put(stack+64000,stop,entity)
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x4194e0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    u.reg_write(UC_X86_REG_ESP,stack+64000);u.reg_write(UC_X86_REG_ESI,camera);u.reg_write(UC_X86_REG_EDI,entity)
    u.emu_start(0x40d88c,0x40d8be,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x40d8be
    expected=bytes(u.mem_read(camera+0x3c,48))+bytes(u.mem_read(camera+0x7e0,36))
    assert raw[100:]==expected,(frame,raw[100:].hex(),expected.hex())
    transitions+=struct.unpack_from('<f',data,88)[0]>0
report=dict(result='PASS',frames=len(rows),transition_frames=transitions,rf_sha256=fingerprint,
            trace_sha256=hashlib.sha256(args.trace.read_bytes()).hexdigest(),
            scope='Every logged PC eye input replays original non-linked 4194e0 and 40d88c..40d8be with unmodified callees; all 84 pose bytes match. Scripted body-aligned view, no look input/effects/camera collision.')
if look_rows:report['scope']='All 664 original 49de50 scalar/body/eye outputs match the continuous scripted pitch trace, followed by original 4194e0 and camera pose copy. Eye orientation matches actual view; no yaw/body physics or interactive input claim.'
(root/('artifacts/scene-look-view-verification.json' if look_rows else 'artifacts/scene-eye-view-verification.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
