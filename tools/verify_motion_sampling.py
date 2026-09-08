"""Compare full rotation tracks with unhooked 0x539ed0, including easing."""
import hashlib,json,struct,subprocess,sys,random,collections
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
obj,data,stack,stop=0x30000000,0x30100000,0x31000000,0x32000000
for a in (obj,data,stack,stop): u.mem_map(a,65536)
u.mem_write(obj+0x78,struct.pack('<I',data))
tracks=[]; ease=collections.Counter()
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    entries=[e for e in archive.get('vpp',{}).get('entries',[]) if e['name'].lower().endswith('.rfa')]
    if not entries: continue
    with (root/'Installed_Game'/archive['path']).open('rb') as f:
        for entry in entries:
            f.seek(entry['offset']); raw=f.read(entry['size'])
            count,=struct.unpack_from('<I',raw,24)
            for offset in struct.unpack_from('<'+'I'*count,raw,80):
                n,=struct.unpack_from('<H',raw,offset+4)
                track=raw[offset+8:offset+8+n*16]; tracks.append(track)
                for i in range(n): ease.update(struct.unpack_from('<2b',track,i*16+12))
assert min(ease)>=0,ease
asset_tracks=len(tracks)
rng=random.Random(0x539ed0)
for _ in range(300):
    tracks.append(b''.join(struct.pack('<i4h2b2x',i*100,1000+i*100,2000,3000,15000,rng.randrange(128),rng.randrange(128)) for i in range(4)))
cases=[]
for track in tracks:
    count=len(track)//16; assert count<=32767, count
    ticks=[struct.unpack_from('<i',track,i*16)[0] for i in range(count)]
    assert all(a<b for a,b in zip(ticks,ticks[1:])),ticks
    # Single-key samples at/before its tick read outside the key in the original;
    # those are intentionally covered by bounded C tests, not this unsafe oracle.
    times=([ticks[0]+1] if count==1 else sorted(set([ticks[0]-1,ticks[0],ticks[-1],ticks[-1]+1]+[(a+b)//2 for a,b in zip(ticks,ticks[1:])]))) if count else [0]
    for tick in times:
        cases.append((track,tick))
print('Checking',len(cases),'samples;',asset_tracks,'asset tracks; easing values',sorted(ease),flush=True)
output=bytearray()
for start in range(0,len(cases),8192):
    batch=cases[start:start+8192]
    payload=b''.join(struct.pack('<Ii',len(track)//16,tick)+track for track,tick in batch)
    run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--sample-rotation'],input=payload,capture_output=True,check=True)
    assert len(run.stdout)==len(batch)*20,(start,len(run.stdout),len(batch)*20)
    output.extend(run.stdout)
failures=[]
for i,(track,tick) in enumerate(cases):
    count=len(track)//16
    u.mem_write(data+80,struct.pack('<I',84))
    u.mem_write(data+84,struct.pack('<fHH',1,count,0)+track)
    u.mem_write(stack+64000,struct.pack('<4I',stop,obj+256,0,tick&0xffffffff))
    u.reg_write(UC_X86_REG_ESP,stack+64000); u.reg_write(UC_X86_REG_ECX,obj); u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x539ed0,stop,count=30000)
    assert u.reg_read(UC_X86_REG_EIP)==stop,hex(u.reg_read(UC_X86_REG_EIP))
    want=bytes(u.mem_read(obj+256,16)); status,=struct.unpack_from('<i',output,i*20)
    got=output[i*20+4:(i+1)*20]
    if status or got!=want: failures.append(dict(index=i,tick=tick,count=count,status=status,expected=want.hex(),actual=got.hex()))
report=dict(result='PASS' if not failures else 'FAIL',samples=len(cases),asset_tracks=asset_tracks,synthetic_tracks=300,failures=len(failures),examples=failures[:20],single_key_boundary='At/before first tick excluded: original reads beyond key; C decodes directly')
(root/'artifacts/motion-rotation-sampling.json').write_text(json.dumps(report,indent=2)); print(report)
assert not failures
