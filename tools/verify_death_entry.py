"""Compare death-entry state prefix with original instructions, PC and NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096)
    m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary)
u.mem_map(0,4096);u.mem_write(0x64ecb9,bytes(2))
address=int(re.search(r'\s_rf_entity_death_entry_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
falling=0;calls=0;entered=0
def boundary(m,a,size,data):
    global calls,entered
    if a==0x42a020:
        calls+=1
        # Supplied predicate boundary: preserve all original prefix instructions.
        sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
        assert struct.unpack('<I',m.mem_read(sp+4,4))[0]==b
        assert bytes(m.mem_read(b+0x714,12))==bytes(12)
        m.reg_write(UC_X86_REG_EAX,falling);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
    elif a==0x48c9f0:
        entered=1;m.emu_stop()
u.hook_add(UC_HOOK_CODE,boundary)
rng=random.Random(0x41fdc0);commands=[];expected=[];counts=[0,0]
for case in range(4096):
    words=[rng.getrandbits(32) for _ in range(11)]
    falling=rng.choice((0,1,2,255,256,0x80000000,0xffffffff));calls=0;entered=0
    body=bytearray(rng.randbytes(0x1500));offsets=[0x810,0x1a8,0x714,0x718,0x71c,0x144,0x148,0x14c,0x150,0x154,0x158]
    for off,value in zip(offsets,words):body[off:off+4]=w(value)
    u.mem_write(b,bytes(body));u.mem_write(0,w(0xffffffff));u.mem_write(stack,w(stop,b));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x41fdc0,stop,count=1000)
    assert u.reg_read(UC_X86_REG_EIP) in (stop,0x48c9f0)
    assert calls==entered and entered==int(not(words[0]&1))
    after=bytes(u.mem_read(b,len(body)));result=b''.join(after[o:o+4] for o in offsets)
    unchanged=bytearray(after)
    for off in offsets:unchanged[off:off+4]=body[off:off+4]
    assert unchanged==body
    x.mem_write(b,w(*words));x.mem_write(stack,w(stop,b,falling));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(address,stop,count=1000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert x.reg_read(UC_X86_REG_EAX)==entered and bytes(x.mem_read(b,44))==result,(case,words,falling)
    commands.append(w(*words,falling));expected.append(w(entered)+result);counts[entered]+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--death-entry'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',cases=4096,already_dying=counts[0],entered=counts[1],original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Original SP41fdc0 entry including SEH and actual4fad00, stopping before48c9f0 collision teardown;42a020 falling result supplied at predicate boundary, including noncanonical upper bytes. Full actor bytes outside selected fields unchanged. Exact PC/NXDK state and normalized entry result. No complete death owner or live activation.')
(root/'artifacts/death-entry.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
