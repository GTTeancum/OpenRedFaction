"""Compare the compiled model-name helper with unchanged original x86 code."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EFLAGS
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
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--compiled-name'],input=b''.join(records))
assert len(actual)==len(records)*68
verified=guards=0
for i,name in enumerate(names):
    # The original has no capacity guard. Give it a full 260-byte output and
    # compare the port only where its documented 64-byte policy accepts input.
    u.mem_write(source,records[i]);u.mem_write(destination,b'\xa5'*260)
    u.mem_write(esp,struct.pack('<4I',stop,destination,source,0x5a4e7c))
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EFLAGS,2)
    u.emu_start(0x5142d0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    reference=bytes(u.mem_read(destination,260));length=reference.index(0)
    status=struct.unpack_from('<i',actual,i*68)[0];observed=actual[i*68+4:(i+1)*68]
    if length<64:
        assert status==0 and observed==reference[:64],(i,name,status,observed,reference[:64]);verified+=1
    else:
        assert status!=0 and observed==b'\xa5'*64,(i,name);guards+=1
assert struct.unpack_from('<i',actual,len(names)*68)[0]!=0 and actual[-64:]==b'\xa5'*64
inventory=json.loads((root/'artifacts/inventory.json').read_text())
archive=next(a for a in inventory['files'] if a['path']=='meshes.vpp')
available={e['name'].lower() for e in archive['vpp']['entries']}
resolved=[];missing=[];other_types=[]
for asset in table['assets']:
    name=asset['model']
    if not name:continue
    if not name.lower().endswith('.vcm'):
        other_types.append(asset['name']);continue
    n=names.index(name);compiled=actual[n*68+4:(n+1)*68].split(b'\0',1)[0].decode()
    row=dict(name=asset['name'],authored=name,compiled=compiled)
    (resolved if compiled.lower() in available else missing).append(row)
report=dict(result='PASS',original_cases=verified,capacity_guards=guards+1,resolved_skeletal_classes=len(resolved),missing=missing,other_model_types=other_types,
    scope='Unchanged 0x5142d0, 0x514330 and original last-dot CRT callee; .v3c specialization used by loader 0x51ce60; all 64 output bytes exact for accepted inputs; no original capacity-guard or complete model-loader equivalence claim')
(root/'artifacts/entity-model-filename-verification.json').write_text(json.dumps(report,indent=2));print(report)
