"""Full original4164c0 with real cached animated sphere queries and bounds."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;desc=b+0x4000;handle=b+0x5000;container=b+0x6000;source=b+0x9000;physics=b+0xa000;actor=b+0xb000;out=b+0xc000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
def run(m,a,args):
    m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(a,stop,count=100000);assert m.reg_read(UC_X86_REG_EIP)==stop;return m.reg_read(UC_X86_REG_EAX)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary)
entry=int(re.search(r'\s_rf_model_corpse_spheres_refresh\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x4164c0);inputs=[];outputs=[];preserved=0
for case in range(1024):
    count=case%9;physical_count=count+(case//9)%5;bones=4
    position=f(*[rng.randrange(-100,101)/4 for _ in range(3)])
    matrices=f(*[rng.randrange(-8,9)/4 for _ in range(bones*12)])
    model_rows=[]
    for i in range(count):model_rows.append(bytes(28)+w(rng.choice((-1,0,1,2,3)))+f(*[rng.randrange(-20,21)/4 for _ in range(3)],rng.randrange(1,10)))
    model=b''.join(model_rows);raw=b''.join(row[:24]+row[28:] for row in model_rows)
    physical=b''.join(f(*[rng.randrange(-20,21)/4 for _ in range(3)],rng.randrange(1,10)/8,-1)+w(0xabc00000+i) for i in range(physical_count))
    body=bytearray(rng.randbytes(0x400));body[0x80:0x84]=w(handle);body[0x184:0x190]=w(physical_count,physical_count,physics);body[0xe4:0xf0]=position
    u.mem_write(b,matrices);u.mem_write(b+0x1d50,w(desc));u.mem_write(desc+0x48,w(bones));u.mem_write(handle,w(2,b,container));u.mem_write(container+0x19c0+0x60,w(count,source))
    if raw:u.mem_write(source,raw)
    if physical:u.mem_write(physics,physical)
    u.mem_write(actor,bytes(body));run(u,0x4164c0,[actor])
    posed=bytes(u.mem_read(physics,len(physical)));new_body=bytes(u.mem_read(actor,len(body)))
    for i in range(physical_count):
        assert posed[i*24+12:(i+1)*24]==physical[i*24+12:(i+1)*24]
        if i>=count:assert posed[i*24:(i+1)*24]==physical[i*24:(i+1)*24]
        preserved+=1
    for start,size in ((0x78,4),(0x180,4),(0x190,24)):body[start:start+size]=new_body[start:start+size]
    assert new_body==body
    expected_bounds=new_body[0x180:0x184]+new_body[0x190:0x1a8];expected_radius=new_body[0x78:0x7c]
    assert expected_radius==expected_bounds[:4]
    if model:x.mem_write(source,model)
    if physical:x.mem_write(physics,physical)
    x.mem_write(b,matrices);x.mem_write(actor,position);x.mem_write(out,bytes([0xa5])*32)
    assert run(x,entry,[source,count,b,bones,physics,physical_count,actor,out,out+28])==0,case
    assert bytes(x.mem_read(physics,len(physical)))==posed and bytes(x.mem_read(out,32))==expected_bounds+expected_radius,case
    inputs.append(w(count,physical_count,bones)+position+model+physical+matrices);outputs.append(w(0)+posed+expected_bounds+expected_radius)
assert subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--corpse-spheres'],input=b''.join(inputs))==b''.join(outputs)
report=dict(result='PASS',cases=1024,preserved_sphere_payloads=preserved,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full4164c0 with real503250/503270 animated cached-matrix paths, list helpers and4a0cb0; no replaced callees. PC/NXDK centers, rebuilt bounds and object78 match; physical radii/other words and extra spheres survive. Zero through8 model spheres, up to12 physics spheres, identity/unparented and four cached bones. No uncached evaluation or live corpse model owner.')
(root/'artifacts/corpse-spheres.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
