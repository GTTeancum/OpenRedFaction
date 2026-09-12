"""Compare the compiled model-name helper with unchanged original x86 code."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EFLAGS,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;source=base+256;destination=base+512;stop=base+4096
rng=random.Random(0x5142d0)
names=['miner.vcm','Actor.V3D','already.v3c','no_extension','','.hidden','trailing.','two.dots.vcm','dir.ext/model','dir.ext\\model','x'*59,'x'*59+'.abc','x'*60,'x'*63]
table=json.loads((root/'artifacts/entity-assets-verification.json').read_text())
names += [asset['model'] for asset in table['assets'] if asset['model']]
while len(names)<2000:
    names.append(''.join(rng.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-/\\') for _ in range(rng.randrange(64))))
records=[name.encode().ljust(64,b'\0') for name in names]+[b'x'*64]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--static-name'],input=b''.join(records))
assert len(actual)==len(records)*68
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im);x.mem_map(base,65536)
entry=int(re.search(r'\s_rf_model_compiled_filename\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_write(base+768,b'.v3m\0')
verified=guards=0
for i,name in enumerate(names):
    # The original has no capacity guard. Give it a full 260-byte output and
    # compare the port only where its documented 64-byte policy accepts input.
    u.mem_write(source,records[i]);u.mem_write(destination,b'\xa5'*260)
    u.mem_write(esp,struct.pack('<4I',stop,destination,source,0x5a7ad8))
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EFLAGS,2)
    u.emu_start(0x5142d0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    reference=bytes(u.mem_read(destination,260));length=reference.index(0)
    status=struct.unpack_from('<i',actual,i*68)[0];observed=actual[i*68+4:(i+1)*68]
    x.mem_write(source,records[i]);x.mem_write(destination,b'\xa5'*64);x.mem_write(esp,struct.pack('<4I',stop,source,destination,base+768));x.reg_write(UC_X86_REG_ESP,esp)
    x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert x.reg_read(UC_X86_REG_EAX)==status&0xffffffff and bytes(x.mem_read(destination,64))==observed

    if length<64:
        assert status==0 and observed==reference[:64],(i,name,status,observed,reference[:64]);verified+=1
    else:
        assert status!=0 and observed==b'\xa5'*64,(i,name);guards+=1
assert struct.unpack_from('<i',actual,len(names)*68)[0]!=0 and actual[-64:]==b'\xa5'*64
report=dict(result='PASS',original_cases=verified,capacity_guards=guards+1,scope='Original5142d0 with53ab40 literal .v3m versus shared PC/NXDK conversion; full64-byte output including untouched tail, bounded output failures. No full static model resource loader claim.')
(root/'artifacts/static-model-filename.json').write_text(json.dumps(report,indent=2));print(report)
