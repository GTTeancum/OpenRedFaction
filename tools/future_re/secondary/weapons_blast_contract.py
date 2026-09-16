"""Execute RF.exe blast victim arithmetic/query setup; never launch the game.

Run from repository root: python tools/future_re/secondary/weapons_blast_contract.py
Only writes artifacts/secondary-re/weapons. Geometry and damage services are
explicit fixtures; no assertion of original level visibility or applied health.
"""
import hashlib
import json
import math
import struct
import sys
from pathlib import Path
sys.dont_write_bytecode=True

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'local/python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_FPCW

SHA = 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
exe = ROOT / 'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest() == SHA
im = pefile.PE(str(exe)).get_memory_mapped_image()
B, STACK, STOP = 0x30000000, 0x300e0000, 0x300f0000
ACTOR, CLASS, ORIGIN, MOVER, SOLID = B, B+0x4000, B+0x8000, B+0x10000, B+0x20000
w = lambda *v: struct.pack('<'+'I'*len(v), *(x & 0xffffffff for x in v))
f = lambda *v: struct.pack('<'+'f'*len(v), *v)
f32 = lambda v: struct.unpack('<f', f(v))[0]
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(im)+4095)&~4095)
u.mem_write(0x400000, im)
u.mem_map(B, 0x100000)
word = lambda a: struct.unpack('<I', u.mem_read(a, 4))[0]
vec = lambda a: list(struct.unpack('<3f', u.mem_read(a, 12)))
trace = []
mode = 'victim'
blocked = 0
world_hit = mover_hit = 0

def ret(value=0):
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, value)
    u.reg_write(UC_X86_REG_EIP, word(sp))
    u.reg_write(UC_X86_REG_ESP, sp+4)

def hook(cpu, address, size, data):
    sp = cpu.reg_read(UC_X86_REG_ESP)
    if address == 0x488ded and mode == 'admission':
        cpu.emu_stop()
    elif address == 0x498e80 and mode == 'victim':
        args = [word(sp+4+i*4) for i in range(4)]
        trace.append(dict(service='cover', start=vec(args[0]), end=vec(args[1]), flags=args[2], output=args[3]))
        ret(blocked)
    elif address == 0x426fc0:
        ret(ACTOR if word(sp+4) == 0x12345 else 0)
    elif address == 0x4892c0:
        args = [word(sp+4+i*4) for i in range(8)]
        trace.append(dict(service='damage', args=args, amount=struct.unpack('<f', w(args[1]))[0]))
        # Return a real x87 zero through a two-instruction fixture trampoline.
        cpu.reg_write(UC_X86_REG_EIP, STOP+0x100)
    elif address == 0x4df1c0 and mode == 'cover':
        descriptor, output = word(sp+4), word(sp+8)
        solid = cpu.reg_read(UC_X86_REG_ECX)
        trace.append(dict(service='geometry', solid='world' if solid == SOLID else 'mover',
                          flags=word(descriptor+0x50), origin=vec(descriptor+0x3c), delta=vec(descriptor+0x48)))
        cpu.mem_write(output, w(world_hit if solid == SOLID else mover_hit))
        ret()
        cpu.reg_write(UC_X86_REG_ESP, sp+16)  # geometry method ret 0xc

u.hook_add(UC_HOOK_CODE, hook)
u.mem_write(STOP+0x100, b'\xd9\xee\xc3')

def run(entry, args):
    u.mem_write(STACK, w(STOP)+args)
    u.reg_write(UC_X86_REG_ESP, STACK)
    u.reg_write(UC_X86_REG_FPCW, 0x27f)
    u.emu_start(entry, STOP, count=100000)
    assert u.reg_read(UC_X86_REG_EIP) == STOP

rows = []
# Independent pos/radius changes prove the executed formula reads physics +e4,
# not object +3c, object radius +78, or physics radius +180.
cases = [(x, 0., 0.) for x in (0., 1., 2.5, 4., f32(5.-2**-21), 5., f32(5.+2**-21), 7.)]
cases += [(1., 2., 2.), (-1., -2., -2.), (0., 0., 4.)]
for delta in cases:
    for blocked in (0, 1):
        trace.clear()
        u.mem_write(ACTOR, bytes(0x6000))
        u.mem_write(ACTOR+0x294, w(CLASS))
        u.mem_write(ACTOR+0x2c, w(0x12345))
        u.mem_write(ACTOR+0x34, f(100))
        u.mem_write(ACTOR+0x3c, f(100, 200, 300))
        u.mem_write(ACTOR+0xe4, f(*delta))
        u.mem_write(ACTOR+0x78, f(99))
        u.mem_write(ACTOR+0x180, f(88))
        u.mem_write(ORIGIN, f(0, 0, 0))
        u.mem_write(0x64ecb9, b'\0\0')
        u.mem_write(0x7c7634, w(0))
        run(0x489010, w(ACTOR, ORIGIN)+f(400, 5)+w(0x6789, 3))
        damage = [r for r in trace if r['service'] == 'damage']
        expected = f32(400 * (1-math.sqrt(sum(v*v for v in delta))/5))
        # Unicorn's x87 intermediates are retained until the original fstp.
        assert len(damage) == int(not blocked and expected > 0), (delta, trace, expected)
        if damage:
            assert damage[0]['amount'] == expected, (delta, trace, expected)
            assert damage[0]['args'][0] == 0x12345
            assert damage[0]['args'][2:] == [0x6789, 0xffffffff, 3, 0, 0xffffffff, 0]
        assert trace[0] == dict(service='cover', start=[0.,0.,0.], end=list(delta), flags=5, output=0)
        rows.append(dict(delta=delta, blocked=blocked, trace=list(trace)))

# Execute complete original ray wrapper with synthetic identity mover geometry.
# All query construction, conversion, early return and final selection execute.
mode = 'cover'
cover_rows = []
for present in (0, 1):
    for mover_hit, world_hit in ((0,0), (0,1), (1,0), (1,1)):
        if not present and mover_hit: continue
        trace.clear()
        u.mem_write(MOVER, bytes(0x4000))
        u.mem_write(MOVER+0x28c, w(0x64e6e0))
        u.mem_write(MOVER+0x294, w(SOLID+0x1000))
        identity = f(1,0,0,0,1,0,0,0,1)
        u.mem_write(MOVER+0x48, identity)
        u.mem_write(MOVER+0xfc, identity)
        u.mem_write(MOVER+0x190, f(-10,-10,-10))
        u.mem_write(MOVER+0x19c, f(10,10,10))
        u.mem_write(0x64e96c, w(MOVER if present else 0x64e6e0))
        u.mem_write(0x6460e8, w(SOLID))
        u.mem_write(ORIGIN, f(0,0,0)+f(1,2,3))
        run(0x498e80, w(ORIGIN, ORIGIN+12, 5, 0))
        assert (u.reg_read(UC_X86_REG_EAX)&255) == int(bool(mover_hit or world_hit))
        expected_solids = ['mover'] if present and mover_hit else (['mover','world'] if present else ['world'])
        assert [r['solid'] for r in trace] == expected_solids, trace
        assert all(r['flags'] == (0x41 if r['solid']=='mover' else 0x45) for r in trace), trace
        cover_rows.append(dict(mover_present=present, mover_hit=mover_hit, world_hit=world_hit, trace=list(trace)))

mode = 'admission'
admission_rows = []
threshold = f32(.1)
for damage, radius in ((0,5),(-1,5),(400,0),(400,threshold),(400,f32(threshold+2**-27)),(400,5)):
    u.mem_write(STACK,w(STOP,ORIGIN)+f(damage,radius)+w(0x6789,3))
    u.reg_write(UC_X86_REG_ESP,STACK)
    u.emu_start(0x488dc0,STOP,count=1000)
    admitted = u.reg_read(UC_X86_REG_EIP)==0x488ded
    assert admitted == (damage>0 and radius>threshold)
    admission_rows.append(dict(damage=damage,radius=radius,admitted=admitted))

report = dict(result='PASS', original_sha256=SHA, victim_entry='0x489010', cover_entry='0x498e80',
              victim_cases=rows, cover_cases=cover_rows,
              admission_cases=admission_rows,
              boundaries=['cover return supplied for victim cases', 'handle registry supplied',
                          'damage service recorded and returns x87 zero',
                          'geometry count supplied for cover cases; no actual level geometry tested',
                          'ordinary actor class flags, no shield, player camera feedback, or skeletal hit direction'])
out = ROOT/'artifacts/secondary-re/weapons'
out.mkdir(parents=True, exist_ok=True)
(out/'blast-contract.json').write_text(json.dumps(report, indent=2)+'\n')
print('PASS',len(rows),'victim cases,',len(cover_rows),'cover wrapper cases,',len(admission_rows),'admission cases; RF.exe',SHA)
