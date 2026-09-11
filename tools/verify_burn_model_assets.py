"""Burn bone binding over installed V3C BONE sections, PC and NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(0x30000000,65536);return u
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe');b=0x30000000;stack=b+0xe000;stop=b+0xf000
queries=[bytes(u.mem_read(a,32)).split(b'\0')[0] for a in (0x595f64,0x595f70,0x595f84,0x595f90,0x595fa4,0x595fac,0x595fb4,0x595fc4,0x595fd4)];groups=(queries[:2],queries[2:4],queries[4:8],queries[8:])
entry=int(re.search(r'_rf_burn_resolve_bones\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
folder=root/'artifacts/burn-model-assets';folder.mkdir(exist_ok=True);payload_path=folder/'payload.bin';records=[]
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for item in archive.get('vpp',{}).get('entries',[]):
    if not item['name'].lower().endswith('.v3c'):continue
    with (root/'Installed_Game'/archive['path']).open('rb') as file:file.seek(item['offset']);data=file.read(item['size'])
    for section in inspect(data)['sections']:
        if section['type']!='0x424f4e45':continue
        start=section['offset'];size,count=struct.unpack_from('<2I',data,start+4);assert size==4+count*56 and count<=256
        payload=data[start+8:start+8+size];payload_path.write_bytes(payload);names=[payload[4+j*56:28+j*56].split(b'\0')[0] for j in range(count)]
        u.mem_write(b+0x48,w(count))
        for j,name in enumerate(names):u.mem_write(b+0x4c+j*0x4c,name+b'\0')
        indices=[]
        for group in groups:
            found=0xffffffff
            for query in group:
                u.mem_write(b+0x9000,query+b'\0');u.mem_write(stack,w(stop,b+0x9000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b);u.emu_start(0x51d690,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
                found=u.reg_read(UC_X86_REG_EAX)
                if found!=0xffffffff:break
            indices.append(found)
        want=w(-3 if 0xffffffff in indices else 0,*indices)
        actual=subprocess.check_output([str(root/'build/pc/Release/rf_bone_probe.exe'),str(payload_path),'--burn-bones']);assert actual==want,('PC',item['name'])
        for j,name in enumerate(names):x.mem_write(b+j*8,w(b+0x2000+j*32,len(name)));x.mem_write(b+0x2000+j*32,name+b'\0')
        x.mem_write(b+0x5000,w(-9,-9,-9,-9));x.mem_write(stack,w(stop,b,count,b+0x5000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
        got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x5000,16));assert got==want,('NXDK',item['name'])
        records.append(dict(archive=archive['path'],model=item['name'],bone_count=count,payload_sha256=hashlib.sha256(payload).hexdigest(),indices=[-1 if i==0xffffffff else i for i in indices],matched_names=[None if i==0xffffffff else names[i].decode('cp1252') for i in indices],complete=0xffffffff not in indices))
assert records
miner=[r for r in records if r['model'].lower()=='miner.v3c'];assert miner and all(r['complete'] for r in miner)
report=dict(result='PASS',models=len(records),complete=sum(r['complete'] for r in records),missing_required_bones=sum(not r['complete'] for r in records),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Installed V3C BONE names: shared PC payload decoder/burn resolver and actual NXDK resolver against original51d690 search with verified fallback priority. Missing required bones are valid rejection outcomes. No dynamic model ownership, pose placement, exclusions or rendered fire.',records=records)
(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='records'});print('Miner:',miner)
