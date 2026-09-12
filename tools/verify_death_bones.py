"""Original class death-bone selection vs shared C."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_EAX
from inspect_models import inspect
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,0x10000);owner=b+0x4000;stack=b+0xc000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
rng=random.Random(0x424e47);cases=[]
for i in range(1024):
    count=i%51;names=[rng.choice(['spine','spine1','Spine','spinehead','head','Head','bip_spine','root','tail']) for _ in range(count)]
    cases.append(('synthetic'+str(i),[(name.encode(),rng.randrange(-1,count)) for name in names]))
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
    for e in archive.get('vpp',{}).get('entries',[]):
        if not e['name'].lower().endswith('.v3c'):continue
        with (root/'Installed_Game'/archive['path']).open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
        section=next((s for s in inspect(raw)['sections'] if s['type']=='0x424f4e45'),None)
        if not section:continue
        at=section['offset']+8;count=struct.unpack_from('<I',raw,at)[0]
        assert count<=50
        cases.append((e['name'],[(raw[at+4+i*56:at+28+i*56].split(b'\0')[0],struct.unpack_from('<i',raw,at+56+i*56)[0]) for i in range(count)]))
commands=[];expected=[];installed={}
for label,bones in cases:
    count=len(bones);u.mem_write(b,bytes(0x2000));u.mem_write(b+0x48,w(count));u.mem_write(stack+0x38,w(owner))
    rows=[]
    for i,(name,parent) in enumerate(bones):
        assert len(name)<25
        u.mem_write(b+0x4c+i*76,name+b'\0');u.mem_write(b+0x94+i*76,w(parent))
        rows.append(name.ljust(28,b'\0')+bytes(28)+w(parent))
    u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_EBP,owner);u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x424e47,0x424f00,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x424f00
    result=bytes(u.mem_read(owner+0x13d8,12));commands.append(w(count)+b''.join(rows));expected.append(bytes(4)+result)
    if not label.startswith('synthetic'):installed[label]=list(struct.unpack('<3i',result))
run=subprocess.run([str(root/'build/pc/Release/rf_transform_probe.exe'),'--death-bones'],input=b''.join(commands),capture_output=True,check=True)
assert len(run.stdout)==len(expected)*16
for i,want in enumerate(expected):assert run.stdout[i*16:(i+1)*16]==want,(cases[i],want.hex(),run.stdout[i*16:(i+1)*16].hex())
native=pefile.PE(str(root/'build/xbox/main.exe'));base=native.OPTIONAL_HEADER.ImageBase;ni=native.get_memory_mapped_image()
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(ni)+4095)//4096*4096);x.mem_write(base,ni);x.mem_map(b,0x10000);stop=b+0xff00
entry=int(re.search(r'\s_rf_model_death_bones\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,raw in enumerate(commands):
    count=struct.unpack_from('<I',raw)[0];x.mem_write(b,raw[4:] or b'\0');x.mem_write(owner,bytes(12))
    x.mem_write(stack,w(stop,b,count,owner));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(owner,12))==expected[i][4:],(cases[i][0],'NXDK')
report=dict(result='PASS',cases=len(cases),installed=installed,scope='Unhooked original424e47..424f00 vs PC and NXDK C, case-sensitive substring search, first two spines/first head, parent-order swap and missing-spine descriptor count; no class storage or live death dispatch')
(root/'artifacts/death-bones-verification.json').write_text(json.dumps(report,indent=2));print(dict(result='PASS',cases=len(cases),installed=len(installed)))
