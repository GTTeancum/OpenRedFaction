"""Compare packed interpolation to unhooked original RF.exe 0x51a000."""
import hashlib,json,random,struct,subprocess,sys,math
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
data,stack,stop=0x30000000,0x31000000,0x32000000
for a in (data,stack,stop): u.mem_map(a,65536)
pairs=set()
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    entries=[e for e in archive.get('vpp',{}).get('entries',[]) if e['name'].lower().endswith('.rfa')]
    if not entries: continue
    with (root/'Installed_Game'/archive['path']).open('rb') as f:
        for entry in entries:
            f.seek(entry['offset']); raw=f.read(entry['size'])
            count,=struct.unpack_from('<I',raw,24)
            for offset in struct.unpack_from('<'+'I'*count,raw,80):
                n,=struct.unpack_from('<H',raw,offset+4)
                keys=[raw[offset+12+i*16:offset+20+i*16] for i in range(n)]
                pairs.update(zip(keys,keys[1:]))
                if len(keys)==1: pairs.add((keys[0],keys[0]))
asset_pairs=len(pairs)
rng=random.Random(0x51a000)
for _ in range(1000):
    def quat():
        v=[rng.uniform(-1,1) for _ in range(4)]; scale=16383/math.sqrt(sum(x*x for x in v))
        return struct.pack('<4h',*(int(x*scale) for x in v))
    pairs.add((quat(),quat()))
inputs=[a+b+struct.pack('<f',t) for a,b in sorted(pairs) for t in (0,.25,.5,.75,1)]
print('Comparing',len(inputs),'samples from',asset_pairs,'installed adjacent key pairs',flush=True)
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--interpolate-rotation'],input=b''.join(inputs),capture_output=True,check=True)
assert len(run.stdout)==len(inputs)*12
failures=[]
for i,raw in enumerate(inputs):
    u.mem_write(data,raw)
    u.mem_write(stack+64000,struct.pack('<4I',stop,data+32,data,data+8)+raw[16:20])
    u.reg_write(UC_X86_REG_ESP,stack+64000); u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x51a000,stop,count=20000)
    assert u.reg_read(UC_X86_REG_EIP)==stop,hex(u.reg_read(UC_X86_REG_EIP))
    want=bytes(u.mem_read(data+32,8)); status,=struct.unpack_from('<i',run.stdout,i*12)
    got=run.stdout[i*12+4:(i+1)*12]
    if status or got!=want:
        failures.append(dict(index=i,input=raw.hex(),expected=want.hex(),actual=got.hex(),status=status))
report=dict(result='PASS' if not failures else 'FAIL',samples=len(inputs),asset_pairs=asset_pairs,failures=len(failures),examples=failures[:20])
(root/'artifacts/motion-interpolation-verification.json').write_text(json.dumps(report,indent=2))
print(report)
assert not failures
