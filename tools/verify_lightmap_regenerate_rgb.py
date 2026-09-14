"""Compiled NXDK retained-RGB composition versus separately verified stages.

This is integration evidence, not a new original-executable oracle. Real
special surfaces are additionally covered by verify_campaign_shadows.py.
"""
import json, random, re, struct, sys
from pathlib import Path
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
w = lambda *v: struct.pack('<' + 'I' * len(v), *v)
f = lambda *v: struct.pack('<' + 'f' * len(v), *v)
B = 0x30000000
VIEW, BASE, RGB, DIRTY, GLOBAL, ROOM = [B + v for v in (0, 128, 4096, 160, 192, 208)]
CHANNELS, SOURCES, ACC, STACK, STOP = [B + v for v in (8192, 16384, 256, 57344, 65280)]
pe = pefile.PE(str(root / 'build/xbox/main.exe'))
im = pe.get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(pe.OPTIONAL_HEADER.ImageBase, (len(im) + 4095) // 4096 * 4096)
u.mem_write(pe.OPTIONAL_HEADER.ImageBase, im)
u.mem_map(B, 65536)
symbols = (root / 'build/xbox/main.map').read_text()
def call(name, *args):
    address = int(re.search(r'\s_' + name + r'\s+([0-9a-fA-F]+)', symbols)[1], 16)
    u.mem_write(STACK, w(STOP, *args))
    u.reg_write(UC_X86_REG_ESP, STACK)
    u.reg_write(UC_X86_REG_FPCW, 0x27f)
    u.emu_start(address, STOP, count=2000000)
    assert u.reg_read(UC_X86_REG_EIP) == STOP
    return u.reg_read(UC_X86_REG_EAX)
rng = random.Random(0x4f26a0)
for i in range(256):
    width, height, count = 1 + i % 16, 1 + i // 16, i % 4
    x, y = rng.randrange(8), rng.randrange(8)
    sample = w(32, 32, x, y) + f(1, 1, 0, 0, 0, 0, 1, 0) + w(2, 0)
    view = sample + w(width, height, SOURCES, count, 0, 0) + f(.25) + w(CHANNELS, CHANNELS + 1024, CHANNELS + 2048, 256)
    u.mem_write(VIEW, view)
    u.mem_write(BASE, w(RGB, 32, 32, 3072))
    u.mem_write(GLOBAL, f(*[rng.random() for _ in range(3)]))
    u.mem_write(ROOM, bytes([i % 3] + [rng.randrange(256) for _ in range(3)]))
    for j in range(count):
        source = w(2, j) + f(0, 0, 1, 0, 0, 0, 0, 0, 1, .25, .5, .75, 5, 1, -.5, .75) + w(j % 2)
        assert len(source) == 76
        u.mem_write(SOURCES + j * 76, source)
    initial = rng.randbytes(3072)
    dirty = i
    def reset():
        u.mem_write(RGB, initial)
        u.mem_write(CHANNELS, bytes(3072))
        u.mem_write(DIRTY, bytes([dirty]))
    reset()
    offset = (y * 32 + x) * 3
    if not count:
        assert call('rf_lightmap_fill_ambient', RGB + offset, 3072 - offset, 96, width, height, GLOBAL, ROOM, DIRTY) == 0
    else:
        assert call('rf_lightmap_seed_ambient', VIEW, GLOBAL, ROOM) == 0
        assert call('rf_lightmap_accumulate_samples', VIEW) == 0
        u.mem_write(ACC, w(CHANNELS, CHANNELS + 1024, CHANNELS + 2048, 256, width, height))
        assert call('rf_lightmap_resolve_rgb', ACC, RGB + offset, 3072 - offset, 96, DIRTY) == 0
    expected = bytes(u.mem_read(RGB, 3072)) + bytes(u.mem_read(DIRTY, 1))
    reset()
    assert call('rf_lightmap_regenerate_rgb', VIEW, 0, 0, 0, GLOBAL, ROOM, BASE, DIRTY) == 0
    assert bytes(u.mem_read(RGB, 3072)) + bytes(u.mem_read(DIRTY, 1)) == expected, i

# Invalid atlas origin, image size, source overflow, output size, channel
# capacity, and special-grid dimensions must retain RGB and dirty state.
guards = [(VIEW + 8, 32), (VIEW, 31), (VIEW + 68, 64),
          (BASE + 12, 3071), (BASE + 4, 0xffffffff), (VIEW + 96, 0), (VIEW + 56, 1)]
for address, value in guards:
    saved = bytes(u.mem_read(address, 4))
    u.mem_write(address, w(value))
    before = bytes(u.mem_read(RGB, 3072)) + bytes(u.mem_read(DIRTY, 1))
    assert call('rf_lightmap_regenerate_rgb', VIEW, 0, 0, int(address == VIEW + 56), GLOBAL, ROOM, BASE, DIRTY) != 0
    assert bytes(u.mem_read(RGB, 3072)) + bytes(u.mem_read(DIRTY, 1)) == before
    u.mem_write(address, saved)
report = dict(result='PASS', nxdk_composed_rectangles=256, nxdk_guards=len(guards),
              scope='Retained atlas and dirty bytes versus independently called verified ambient, ordinary accumulation and resolve stages; 0..3 sources, room overrides, offsets, all 1..16 dimensions. No native rendering or full original-scene oracle.')
(root / 'artifacts/lightmap-regenerate-rgb.json').write_text(json.dumps(report, indent=2))
print(report)
