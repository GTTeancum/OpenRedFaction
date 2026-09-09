"""Compare complete original 0x539d00/0x539be0 and CRT callees for ASCII names."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EFLAGS
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;source=base+256;stop=base+4096;cache=0x1c459e8
rng=random.Random(0x539be0);requests=[];expected=[];hits=new=guards=0
names=['','ult2_stand.mvf','ULT2_STAND.RFA','one.two.mvf','one.two.rfa','one.mvf','dir.ext/file','dir.ext/other','.hidden','no_extension','name.','x'*59]
for n in range(1600):
    capacity=8;records=bytearray(rng.randbytes(8*124));labels=[]
    for i in range(8):
        name=rng.choice(names);labels.append(name);b=name.encode()+b'\0';records[i*124:i*124+len(b)]=b
    name=rng.choice(names) if n%3 else 'missing_'+str(n)+'.mvf'
    raw=name.encode().ljust(64,b'\0')
    if n%41==0:raw=b'x'*64
    if n%43==0:raw=b'\x80'+bytes(63)
    if n%53==0:raw=b'z'*60+bytes(4)
    if n%47==0:capacity=0
    if n%11==0:
        for i in range(8):struct.pack_into('<I',records,i*124+0x70,0xffffffff)
    requests.append(struct.pack('<I',capacity)+raw+records)
    def stem(s):return s.rsplit('.',1)[0].lower() if '.' in s else s.lower()
    match=next((i for i,s in enumerate(labels[:capacity]) if s and stem(s)==stem(name)),None)
    first=next((i for i,s in enumerate(labels[:capacity]) if not s),None)
    if b'\0' not in raw or raw.find(b'\0')>=60 or raw[0]>127 or (match is None and first is None):
        expected.append(struct.pack('<iI',-4 if raw[0]<128 else -2,0xa5a5a5a5)+records);guards+=1;continue
    u.mem_write(cache,bytes(800*124));u.mem_write(cache,bytes(records));u.mem_write(source,raw)
    u.mem_write(esp,struct.pack('<2I',stop,source));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EFLAGS,2)
    u.emu_start(0x539d00,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    pointer=u.reg_read(UC_X86_REG_EAX);index=(pointer-cache)//124
    assert pointer==cache+index*124 and index<capacity
    expected.append(struct.pack('<iI',0,index)+bytes(u.mem_read(cache,8*124)))
    hits+=match is not None;new+=match is None
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--motion-cache'],input=b''.join(requests))
assert len(actual)==len(expected)*1000
for n,want in enumerate(expected):assert actual[n*1000:(n+1)*1000]==want,(n,actual[n*1000:n*1000+8].hex(),want[:8].hex())
report=dict(result='PASS',original_cases=hits+new,reused=hits,initialized=new,port_guards=guards,
    scope='Unchanged complete 0x539d00, 0x539be0 and CRT callees for ASCII names; selected identity and all eight 124-byte records exact, including preserved bytes and reference wrap. Guard cases are port policy; lazy payload loading/release excluded.')
(root/'artifacts/motion-cache-verification.json').write_text(json.dumps(report,indent=2));print(report)
