"""Compare complete original multi-plane clipping and pool lifetime."""
import hashlib,json,random,struct,subprocess,sys,math
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;records=base+256;list_a=base+1024;list_b=base+2048;count_address=base+3072;mask_address=count_address+4;stop=base+4096
def call(address,*args):
    u.mem_write(esp,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(address,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
def identity(pointer):
    if records<=pointer<records+144:return (pointer-records)//48
    assert 0x1d002d0<=pointer<0x1d00bd0
    return 3+(pointer-0x1d002d0)//48
rng=random.Random(0x549e);cases=[];expected=[]
for n in range(1200):
    near=.125 if n>=600 else .1
    far=4;view=struct.pack('<23f5I',*([0]*22),far,1,1,1,1,0);planes=struct.pack('<8f',near,far,*([0]*6));attributes=5
    vertices=[];union=0;common=255
    for i in range(3):
        x,y,z=[rng.randint(-64,64)/8,rng.randint(-64,64)/8,rng.randint(-32 if n>=600 else 1,64)/8]
        mask=(8 if x>z else 0)|(32 if y>z else 0)|(4 if -z>x else 0)|(16 if -z>y else 0)|(2 if z>far else 0)
        if n>=600 and z<near:mask|=1
        union|=mask;common&=mask
        record=bytearray(b'\xa5'*48);struct.pack_into('<3f',record,0,x,y,z);record[24:27]=bytes([mask,0,i]);struct.pack_into('<2f',record,28,rng.randrange(64)/16,rng.randrange(64)/16);record[44:47]=bytes([40,50,60]);vertices.append(bytes(record))
    cases.append(b''.join(vertices)+planes+view+struct.pack('<II4B',0x66,attributes,union,common,0,0))
    u.mem_write(0x1d002d0,b'\xa5'*2304);call(0x549270)
    u.mem_write(records,b''.join(vertices));u.mem_write(list_a,struct.pack('<3I',records,records+48,records+96));u.mem_write(list_b,bytes(200));u.mem_write(count_address,struct.pack('<I',3));u.mem_write(mask_address,bytes([union,common]))
    u.mem_write(0x17c7bcc,struct.pack('<I',0x66));u.mem_write(0x5a4d18,b'\1');u.mem_write(0x5a4d19,b'\1');u.mem_write(0x1818b65,b'\1');u.mem_write(0x1818b6c,struct.pack('<f',far));u.mem_write(0x1818b78,struct.pack('<f',near))
    call(0x549e00,list_a,list_b,count_address,mask_address,attributes)
    count=struct.unpack('<I',u.mem_read(count_address,4))[0];pointer=u.reg_read(UC_X86_REG_EAX)
    ids=[identity(x) for x in struct.unpack('<'+'I'*count,u.mem_read(pointer,count*4))]+[0xffffffff]*(48-count)
    order=[(x-0x1d002d0)//48 for x in struct.unpack('<48I',u.mem_read(0x1d02550,192))]
    expected.append(struct.pack('<iI',0,count)+bytes(u.mem_read(mask_address,2))+struct.pack('<48I',*ids)+bytes(u.mem_read(0x1d02610,4))+struct.pack('<48I',*order)+bytes(u.mem_read(0x1d002d0,2304)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--clip-polygon'],input=b''.join(cases));assert len(actual)==len(cases)*2702
exact=0;maximum=0
for n,reference in enumerate(expected):
    observed=actual[n*2702:(n+1)*2702];assert observed[:398]==reference[:398],(n,observed[:10].hex(),reference[:10].hex())
    exact+=observed==reference
    for i in range(48):
        a=observed[398+i*48:398+(i+1)*48];b=reference[398+i*48:398+(i+1)*48]
        assert a[12:28]==b[12:28] and a[36:]==b[36:],(n,i)
        for offset in (0,4,8,28,32):
            x=struct.unpack_from('<f',a,offset)[0];y=struct.unpack_from('<f',b,offset)[0]
            error=abs(x-y)/max(1,abs(y));maximum=max(maximum,error);assert error<=2e-6,(n,i,offset,x,y)
report=dict(result='PASS',triangles=len(cases),bit_exact_pool_results=exact,max_scaled_error=maximum,
    scope='Complete unchanged 0x549e00 and all callees; 600 positive-Z side/far fixtures plus 600 supplied near-bit fixtures including nonpositive Z, UV and constant RGB; counts/masks/pointer identities/free order exact, float fields within 2e-6 scaled. Near-bit generation is supplied by the fixture, not claimed as original projection behavior; custom plane and general colors excluded.')
(root/'artifacts/model-clip-polygon-verification.json').write_text(json.dumps(report,indent=2));print(report)
