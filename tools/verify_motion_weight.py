"""Compare blend-weight envelopes with unhooked original 0x539e10."""
import hashlib,json,struct,subprocess,sys,random
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
# Caller spill of the returned x87 value; original routine and callees unhooked.
u.mem_write(stop,b'\xd9\x1d'+struct.pack('<I',obj+256))
envelopes=[]
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    entries=[e for e in archive.get('vpp',{}).get('entries',[]) if e['name'].lower().endswith('.rfa')]
    if not entries: continue
    with (root/'Installed_Game'/archive['path']).open('rb') as f:
        for entry in entries:
            f.seek(entry['offset']); raw=f.read(entry['size'])
            count,=struct.unpack_from('<I',raw,24)
            start,end=struct.unpack_from('<2i',raw,16); fi,fo=struct.unpack_from('<2i',raw,36)
            for offset in struct.unpack_from('<'+'I'*count,raw,80):
                weight,=struct.unpack_from('<f',raw,offset)
                envelopes.append((weight,start,end,fi,fo))
asset_envelopes=len(envelopes)
rng=random.Random(0x539e10)
for i in range(500):
    envelopes.append((rng.choice([-1,0,0.000001,0.00001,1,2,3.7]),-100,900,rng.randrange(2000),rng.randrange(2000)))
inputs=[]
for weight,start,end,fi,fo in envelopes:
    assert fi>=0 and fo>=0 and end>=start
    times=sorted(set([start-1,start,start+fi//2,start+fi,start+fi+1,(start+end)//2,end-fo-1,end-fo,end-fo//2,end,end+1]))
    for tick in times:
        for bypass in (0,1): inputs.append(struct.pack('<f6i',weight,start,end,fi,fo,tick,bypass))
print('Comparing',len(inputs),'weight samples',flush=True)
run=subprocess.run([str(root/'build/pc/Release/rf_motion_probe.exe'),'--sample-weight'],input=b''.join(inputs),capture_output=True,check=True)
assert len(run.stdout)==len(inputs)*8
failures=[]
for i,raw in enumerate(inputs):
    weight,start,end,fi,fo,tick,bypass=struct.unpack('<f6i',raw)
    u.mem_write(data+16,struct.pack('<2i',start,end)); u.mem_write(data+36,struct.pack('<2i',fi,fo))
    u.mem_write(data+80,struct.pack('<I',84)+raw[:4])
    u.mem_write(stack+64000,struct.pack('<4I',stop,0,tick&0xffffffff,bypass))
    u.reg_write(UC_X86_REG_ESP,stack+64000); u.reg_write(UC_X86_REG_ECX,obj); u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x539e10,stop+6,count=1000)
    assert u.reg_read(UC_X86_REG_EIP)==stop+6
    want=bytes(u.mem_read(obj+256,4)); status,=struct.unpack_from('<i',run.stdout,i*8)
    got=run.stdout[i*8+4:(i+1)*8]
    if status or want!=got: failures.append(dict(index=i,input=list(struct.unpack('<f6i',raw)),status=status,expected=want.hex(),actual=got.hex()))
report=dict(result='PASS' if not failures else 'FAIL',samples=len(inputs),asset_envelopes=asset_envelopes,synthetic_envelopes=500,failures=len(failures),examples=failures[:20])
(root/'artifacts/motion-weight-verification.json').write_text(json.dumps(report,indent=2)); print(report)
assert not failures
