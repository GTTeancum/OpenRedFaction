"""Compare shared metadata order/search with original and compiled NXDK."""
import json
import random
import re
import struct
import subprocess
import verify_sound_metadata_lookup as original

root = original.root
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
pe = original.pefile.PE(str(root/'build/xbox/main.exe'))
image = pe.get_memory_mapped_image()
x = Uc(UC_ARCH_X86, UC_MODE_32)
x.mem_map(pe.OPTIONAL_HEADER.ImageBase, (len(image)+4095)//4096*4096)
x.mem_write(pe.OPTIONAL_HEADER.ImageBase, image)
base = 0x31000000; order_address=base+0x90000; stack=base+0xe0000; stop=base+0xf0000
x.mem_map(base, 0x100000)
mapping = (root/'build/xbox/main.map').read_text()
entries = [int(re.search('_rf_sound_metadata_'+name+r'\s+([0-9a-fA-F]+)', mapping)[1], 16) for name in ('order','find')]
word = original.word
def native(address, *args):
    x.mem_write(stack, word(stop)+b''.join(word(a) for a in args))
    x.reg_write(UC_X86_REG_ESP, stack)
    x.emu_start(address, stop, count=2000000000)
    assert x.reg_read(UC_X86_REG_EIP) == stop
    assert x.reg_read(UC_X86_REG_ESP) == stack+4
    return x.reg_read(UC_X86_REG_EAX)

rng = random.Random(0x56bb80)
fixtures = [[(r['path'], (int(r['looping'])<<30)|r['loop_start'], 0x10000000) for r in original.rows]]
for count in (0,1,2,7,8,9,16,31,128,4096):
    fixtures.append([(f'folder{i%3}\\Sound{rng.randrange(7)}.wav',i,0x10000000 if i%5 else 0) for i in range(count)])
fixtures.append([('same.wav',i,0x10000000) for i in range(4096)])
queries_total = 0
for fixture in fixtures:
    compact = b''.join(name.encode().ljust(120,b'\0')+word(a)+word(b) for name,a,b in fixture)
    records = bytearray(4096*180)
    for i,(name,a,b) in enumerate(fixture):
        records[i*180:i*180+120] = name.encode().ljust(120,b'\0')
        records[i*180+164:i*180+180] = word(i+1)+word(a)+word(b)+word(0)
    original.m.mem_write(original.table, bytes(records))
    original.call(0x5749fa, original.table, 4096, 180, 0x56bb80, limit=2000000000)
    expected = b''.join(struct.pack('<H',original.read(original.table+i*180+164)-1 & 0xffff) for i in range(4096))
    original.m.mem_write(0x1fce720,b'\x01')
    names = list(dict.fromkeys(name.rsplit('\\',1)[-1] for name,_,_ in fixture))+['','absent.wav','prefix/Sound0.wav','prefix\\sOuNd0.WAV']
    indexes = []
    for name in names:
        original.m.mem_write(original.base,name.encode()+b'\0')
        pointer = original.call(0x56baa0,original.base)
        indexes.append(original.read(pointer+164)-1 if pointer else -1)
    request = word(len(fixture))+compact+word(len(names))+b''.join(name.encode().ljust(120,b'\0') for name in names)
    actual = subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--sound-metadata'],input=request)
    assert actual == word(0)+expected+b''.join(word(i) for i in indexes), len(fixture)
    if compact:x.mem_write(base,compact)
    before=bytes(x.mem_read(base,len(compact)))
    assert native(entries[0],base,len(fixture),order_address)==0
    assert bytes(x.mem_read(order_address,8192))==expected,len(fixture)
    assert bytes(x.mem_read(base,len(compact)))==before
    for name,index in zip(names,indexes):
        x.mem_write(base+0x98000,name.encode()+b'\0')
        pointer=native(entries[1],base,order_address,base+0x98000)
        assert pointer == (base+index*128 if index>=0 else 0),(name,index,pointer)
    queries_total+=len(names)

# Preflight must preserve every order byte, including a bad final row.
for bad_name in (b'x'*120,b'\xff'+bytes(119)):
    compact=bytes(128)+bad_name+bytes(8)
    request=word(2)+compact+word(0)
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--sound-metadata'],input=request)
    assert actual[:4]!=word(0) and actual[4:]==b'\x55'*8192
    x.mem_write(base,compact);x.mem_write(order_address,b'\x55'*8192)
    assert native(entries[0],base,2,order_address)!=0
    assert bytes(x.mem_read(order_address,8192))==b'\x55'*8192
x.mem_write(order_address,b'\x55'*8192)
assert native(entries[0],base,4097,order_address)!=0
assert bytes(x.mem_read(order_address,8192))==b'\x55'*8192
report=dict(result='PASS',fixtures=len(fixtures),lookups_per_backend=queries_total,invalid_cases=3,
            scope='Exact4096-index permutation and lookup identities vs unhooked original; '
                  'PC and compiled NXDK, authored and duplicate-heavy data, empty/short/full '
                  'tables, invalid-name preflight. No text parser or emulator device execution.')
(root/'artifacts/sound-metadata-port.json').write_text(json.dumps(report,indent=2));print(report)

