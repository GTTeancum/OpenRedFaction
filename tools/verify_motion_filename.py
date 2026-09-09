"""Run unchanged motion-loader filename instructions against bounded C output."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EFLAGS
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
assert image[0x1a7a88:0x1a7a8d]==b'.rfa\0'
base=0x30000000;u.mem_map(base,65536);esp=base+32000;source=base+256;destination=esp+0x120
rows=json.loads((root/'artifacts/entity-state-verification.json').read_text())['declarations']
names=['','no_extension','.hidden','trailing.','two.dots.mvf','dir.ext/motion','dir.ext\\motion','x'*59,'x'*60,'x'*63]
names += [r['motion'] for r in rows]
rng=random.Random(0x53a9d6)
while len(names)<2400:names.append(''.join(rng.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-/\\') for _ in range(rng.randrange(64))))
records=[name.encode().ljust(64,b'\0') for name in names]+[b'x'*64]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--motion-name'],input=b''.join(records))
assert len(actual)==len(records)*68
exact=guards=0
for n,name in enumerate(names):
    u.mem_write(source,records[n]);u.mem_write(destination,b'\xa5'*1024)
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EBP,source);u.reg_write(UC_X86_REG_EFLAGS,2)
    u.emu_start(0x53a9d6,0x53aa54,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x53aa54
    expected=bytes(u.mem_read(destination,1024));status=struct.unpack_from('<i',actual,n*68)[0];output=actual[n*68+4:(n+1)*68]
    if expected.index(0)<64:
        assert status==0 and output==expected[:64],(n,name,status,output,expected[:64]);exact+=1
    else:
        assert status==-4 and output==b'\xa5'*64,(n,name);guards+=1
assert struct.unpack_from('<i',actual,len(names)*68)[0]==-4 and actual[-64:]==b'\xa5'*64
report=dict(result='PASS',original_cases=exact,capacity_guards=guards+1,
    scope='Unchanged 0x53a9d6..0x53aa54 including original first-dot CRT callee; all 64 output bytes exact for accepted names. Port capacity guards not original semantics; file I/O and registration excluded.')
(root/'artifacts/motion-filename-verification.json').write_text(json.dumps(report,indent=2));print(report)
