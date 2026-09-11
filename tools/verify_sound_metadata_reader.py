"""Bluebeard reader/owner contracts and compiled NXDK reader equivalence."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
word=lambda v:struct.pack('<I',v&0xffffffff)
source=(root/'Installed_Game/bluebeard.bty').read_bytes()
expected=bytearray()
for block in re.split(rb'(?m)^\$Sound:\s*',source)[1:]:
    name=re.match(rb'"([^"]+)"',block)[1];a=b=0x10000000
    a=0
    if b'+Music Track' in block:b|=0x20000000
    else:
        if b'+Ambient Sound' in block:a|=0x80000000
        b|=int(re.search(rb'\$Keyoff Time:\s*(-?\d+)',block)[1])&0xfffffff
    if b'+Looping Sound' in block:a|=0x40000000|(int(re.search(rb'\+Loop Start:\s*(-?\d+)',block)[1])&0x7ffffff)
    if b'+Preload' in block:b|=0x80000000
    if b'+Preserve Low Frequencies' in block:a|=0x10000000
    elif b'+Preserve Medium Frequencies' in block:a|=0x08000000
    expected.extend(name.ljust(120,b'\0')+word(a)+word(b))
pe=pefile.PE(str(root/'build/xbox/main.exe'));image=pe.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(image)+4095)//4096*4096);x.mem_write(pe.OPTIONAL_HEADER.ImageBase,image)
base=0x30000000;rows=base+0x100000;count=base+0x190000;stack=base+0x1e0000;stop=base+0x1f0000
x.mem_map(base,0x200000)
entry=int(re.search(r'_rf_sound_metadata_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
header=b'$Sound Root: "a"\n$PS2 Sound Root: "b"\n'
row=b'$Sound: "x.wav"\n$Folder: "a"\n$Envelope: 0x1D0F0007\n$Keyoff Time: -1\n+Looping Sound\n+Loop Start: -1\n+Preload\n+Incidental\n+Preserve Low Frequencies\n'
valid=[(source,bytes(expected)),(header,b''),(header+row,b'x.wav'.ljust(120,b'\0')+word(0x57ffffff)+word(0x9fffffff)),
       (header+b'$Sound: "m.wav"\n$Folder: "m"\n+Music Track\n+Preserve Medium Frequencies\n',b'm.wav'.ljust(120,b'\0')+word(0x08000000)+word(0x30000000))]
invalid=[b'',header+row+b'unknown',header+row.replace(b'+Loop Start: -1',b'+Loop Start: 4294967296'),
         header+row.replace(b'$Folder: "a"',b''),header+row.replace(b'x.wav',b'x'*80),
         header+row.replace(b'0x1D0F0007',b'0x'),header+row.replace(b'x.wav',b'x\0.wav'),
         header+row.replace(b'x.wav',b'\xff.wav'),header+row.replace(b'+Loop Start: -1',b''),
         header+row.replace(b'+Preload',b'+Preload\n+Preload'),header+row[:-10]]
sentinel=b'\x55'*(4096*128)
for data,wanted in valid+[(v,None) for v in invalid]:
    budget=1024*1024
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--sound-metadata-read'],input=word(len(data))+word(budget)+data)
    status,n,allocated=struct.unpack_from('<3I',actual)
    if wanted is None:assert status and n==allocated==0
    else:
        assert status==0 and n==len(wanted)//128 and allocated==16+8192+len(wanted),(status,n,allocated)
        assert actual[12:12+len(wanted)]==wanted
    if data:x.mem_write(base,data)
    x.mem_write(rows,sentinel);x.mem_write(count,word(0x55555555))
    x.mem_write(stack,b''.join(word(v) for v in (stop,base,len(data),rows,4096,count)))
    x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=500000000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    result=x.reg_read(UC_X86_REG_EAX)
    if wanted is None:
        assert result and bytes(x.mem_read(rows,len(sentinel)))==sentinel and bytes(x.mem_read(count,4))==word(0x55555555)
    else:
        assert result==0 and bytes(x.mem_read(rows,len(wanted)))==wanted
        assert bytes(x.mem_read(count,4))==word(len(wanted)//128)
        assert bytes(x.mem_read(rows+len(wanted),len(sentinel)-len(wanted)))==sentinel[len(wanted):]
# Exact PC owner budget and one-byte-short rejection; rows outlive input text.
needed=16+8192+len(expected)
for budget in (needed-1,needed):
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--sound-metadata-read'],input=word(len(source))+word(budget)+source)
    status,n,allocated=struct.unpack_from('<3I',actual)
    assert bool(status)==(budget<needed)
    if not status:assert n==2712 and allocated==needed
report=dict(result='PASS',valid_cases=len(valid),invalid_cases=len(invalid),installed_rows=len(expected)//128,
            owner_bytes=needed,budget_cases=2,scope='Independent installed packed fields, PC owner lifetime/repeated close/nonempty rejection, '
            'transactional reader errors and compiled NXDK reader bytes. Not original full-parser or XEMU file-I/O evidence.')
(root/'artifacts/sound-metadata-reader.json').write_text(json.dumps(report,indent=2));print(report)
