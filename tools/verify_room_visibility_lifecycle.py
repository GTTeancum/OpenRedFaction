"""Original room visibility reset and nonrecursive visit, with real callees."""
import runpy,struct,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,base,stack,stop,root=(c[k] for k in ('u','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
u.mem_map(base+0x10000,0x20000)
def put(a,v):u.mem_write(a,struct.pack('<I',v))
def call(a,args=(),this=0):
    u.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,this)
    u.emu_start(a,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
reset_cases=0;visit_cases=0
for count in (0,1,17,128):
    world=base+0x2000;array=base+0x3000
    put(world+0x90,count);put(world+0x98,array)
    for i in range(count):
        room=base+0x10000+i*384;put(array+i*4,room)
        u.mem_write(room,bytes([0xa5])*384)
    call(0x4d2f80,this=world)
    for i in range(count):
        expected=bytearray([0xa5])*384;expected[0x160]=0
        assert bytes(u.mem_read(base+0x10000+i*384,384))==expected
    reset_cases+=1
for blocked0,blocked1 in ((0,0),(1,0),(0,1),(255,255)):
    room=base+0x10000;rect=base+0x4000
    u.mem_write(room,bytes(384));u.mem_write(room,bytes((blocked0,blocked1)))
    put(0x9bb57c,0);put(0x9bb55c,0)
    for n,values in enumerate(((10.,20.,80.,90.),(5.,30.,70.,100.))):
        u.mem_write(rect,struct.pack('<4f',*values))
        call(0x4d4860,(0,room,rect,3+n,1,0)) # flag 1 disables portal recursion.
        raw=bytes(u.mem_read(room,384))
        if blocked0 or blocked1:
            assert raw[0x160:0x162]==bytes(2)
            assert struct.unpack('<I',u.mem_read(0x9bb57c,4))[0]==0
        else:
            assert raw[0x160:0x162]==b'\1\1'
            assert struct.unpack_from('<I',raw,0x164)[0]==3+n
            assert struct.unpack_from('<4f',raw,0x16c)==((10.,20.,80.,90.) if n==0 else (5.,20.,80.,100.))
            assert struct.unpack('<I',u.mem_read(0x9bb57c,4))[0]==1
            assert struct.unpack('<I',u.mem_read(0x9a8548,4))[0]==room
        visit_cases+=1
report=dict(result='PASS',reset_cases=reset_cases,visit_cases=visit_cases,
    scope='Original 4d2f80 full room-vector reset and 4d4860 nonrecursive visit with actual predicates/rectangle helpers. Reset changes only +160; accepted visits set +160/+161, union screen bounds and avoid duplicate list entries; room bytes 0/1 gate visits. Portal recursion, full rendering and simulation/render timing excluded.')
(root/'artifacts/room-visibility-lifecycle-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
