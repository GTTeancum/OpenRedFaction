"""Compare position sampling with unhooked original 0x53a130."""
import hashlib, json, random, struct, subprocess, sys
from pathlib import Path
import pefile
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_FPCW
exe = root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image)+4095)//4096*4096); u.mem_write(0x400000, image)
obj, data, stack, stop = 0x30000000, 0x30100000, 0x31000000, 0x32000000
for a,n in [(obj,4096),(data,65536),(stack,8192),(stop,4096)]: u.mem_map(a,n)
u.mem_write(obj+0x78, struct.pack('<I',data))
tracks=[]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
for archive in inventory['files']:
    for entry in archive.get('vpp',{}).get('entries',[]):
        if entry['name'].lower() not in ('ult2_stand.rfa','ult2_crouch.rfa'): continue
        with (root/'Installed_Game'/archive['path']).open('rb') as f:
            f.seek(entry['offset']); raw=f.read(entry['size'])
        count,=struct.unpack_from('<I',raw,24)
        for offset in struct.unpack_from('<'+'I'*count,raw,80):
            rotations, positions=struct.unpack_from('<2H',raw,offset+4)
            start=offset+8+rotations*16
            tracks.append(raw[start:start+positions*40])
assert len(tracks)==50
rng=random.Random(0x53a130)
for n in range(101):
    count=n%9
    tracks.append(b''.join(struct.pack('<i9f',i*100,*(rng.uniform(-20,20) for _ in range(9))) for i in range(count)))
inputs=[]; expected=[]
for track in tracks:
    count=len(track)//40
    first=struct.unpack_from('<i',track)[0] if count else 0
    last=struct.unpack_from('<i',track,(count-1)*40)[0] if count else 0
    for tick in (first-1,first,(first+last)//2,last,last+1):
        inputs.append(struct.pack('<Ii',count,tick)+track)
        u.mem_write(data+80,struct.pack('<I',84))
        u.mem_write(data+84,struct.pack('<fHH',1,0,count)+track)
        u.mem_write(stack+8000,struct.pack('<4I',stop,obj+256,0,tick & 0xffffffff))
        u.reg_write(UC_X86_REG_ESP,stack+8000); u.reg_write(UC_X86_REG_ECX,obj)
        u.reg_write(UC_X86_REG_FPCW,0x37f)
        u.emu_start(0x53a130,stop,count=10000)
        assert u.reg_read(UC_X86_REG_EIP)==stop
        expected.append(bytes(u.mem_read(obj+256,12)))
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe')],input=b''.join(inputs),capture_output=True,check=True)
assert len(run.stdout)==len(inputs)*16
for i,want in enumerate(expected):
    status,=struct.unpack_from('<i',run.stdout,i*16)
    assert status==0 and run.stdout[i*16+4:(i+1)*16]==want,(i,status)
report=dict(result='PASS',samples=len(inputs),asset_tracks=50,synthetic_tracks=101,
            scope='Position sampling only; stand/crouch plus synthetic tracks, unhooked original 0x53a130')
(root/'artifacts/motion-position-verification.json').write_text(json.dumps(report,indent=2))
print(report)
