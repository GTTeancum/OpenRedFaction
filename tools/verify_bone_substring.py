"""Bone-only substring lookup51d690 (real strstr) vs PC and NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(0x30000000,65536);return u
u=machine(original);x=machine(root/'build/xbox/main.exe');b=0x30000000;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'_rf_model_find_bone_substring\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x51d690);cases=[];expected=[];specs=[]
queries=[b'lowerleg-l',b'lowerleg-r',b'tech- leg-l-lower',b'tech- leg-r-lower',b'spine01',b'spine03',b'tech- 1spine',b'tech- 1spine01',b'head',b'',b'a',b'HEAD',b'aaa']
for i in range(2048):
    query=queries[i%len(queries)];names=[rng.choice([b'prefix_'+query,query+b'_suffix',query.upper(),b'other',b'',b'aaab',b'xheadx']) for _ in range(rng.randrange(17))]
    specs.append((names,query));wire=w(len(names),1,1)+b''.join(n.ljust(32,b'\0') for n in names).ljust(512,b'\0')+query.ljust(512,b'\0')*2+query.ljust(32,b'\0');assert len(wire)==1580;cases.append(wire)
    u.mem_write(b+0x48,w(len(names)))
    for j,name in enumerate(names):u.mem_write(b+0x4c+j*0x4c,name+b'\0')
    u.mem_write(b+0x2000,query+b'\0');u.mem_write(stack,w(stop,b+0x2000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b);u.emu_start(0x51d690,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    found=u.reg_read(UC_X86_REG_EAX);want=next((j for j,name in enumerate(names) if query in name),0xffffffff);assert found==want
    expected.append(w(-3 if found==0xffffffff else 0,found))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--bone-substring'],input=b''.join(cases));assert len(actual)==len(cases)*8
for i,((names,query),want) in enumerate(zip(specs,expected)):
    assert actual[i*8:(i+1)*8]==want,('PC',i)
    for j,name in enumerate(names):x.mem_write(b+j*8,w(b+0x1000+j*32,len(name)));x.mem_write(b+0x1000+j*32,name+b'\0')
    x.mem_write(b+0x2000,query+b'\0');x.mem_write(b+0x2100,w(-1));x.mem_write(stack,w(stop,b,len(names),b+0x2000,len(query),b+0x2100));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x2100,4));assert got==want,('NXDK',i)
report=dict(result='PASS',cases=len(cases),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Unchanged51d690 and573930 strstr vs shared PC/NXDK: first bone-only case-sensitive substring, prefix/suffix, duplicates, empty query/name, absent/uppercase names, actual burn query strings. Non-bone groups ignored in PC probe. Model resolution503c00 and42eb20 fallback orchestration excluded.')
(root/'artifacts/bone-substring.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
