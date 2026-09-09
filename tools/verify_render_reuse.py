"""Compare positive duplicate-vertex processing with the unchanged renderer."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;vertex=base+256;model=base+512;reuse=base+1024;uv=base+2048
rng=random.Random(523);cases=[];expected=[];clipped=0
for n in range(2000):
    records=[]
    for i in range(8):
        record=struct.pack('<6fBB3s3s',*(rng.randint(-1000,1000)/16 for _ in range(6)),rng.choice([0,0,0,1,2,255]),rng.randrange(256),rng.randbytes(3),rng.randbytes(3))
        records.append(record)
        for address,data in [(0x1bf7000+i*12,record[:12]),(0x1bdb2b0+i*12,record[12:24]),(0x1c3d554+i,record[24:25]),(0x1bf29b0+i,record[25:26]),(0x1c3f494+i*3,record[26:29])]:u.mem_write(address,data)
    index=rng.randrange(1,8);distance=rng.randrange(1,index+1);lighting=n%2;color=rng.randbytes(3);alpha=rng.randrange(256)
    scales=struct.pack('<2f',rng.randint(-100,100)/16,rng.randint(-100,100)/16);coords=struct.pack('<2f',rng.randint(-100,100)/16,rng.randint(-100,100)/16)
    cases.append(b''.join(records)+struct.pack('<I3sB',lighting,color,alpha)+scales+struct.pack('<Ii',index,distance)+coords)
    u.mem_write(esp,bytes(1024));u.mem_write(vertex,b'\xa5'*40);u.mem_write(model+8,bytes([alpha]));u.mem_write(model+12,color)
    u.mem_write(reuse,struct.pack('<h',distance));u.mem_write(uv,coords);u.mem_write(0x5a7dd8,scales[:4]);u.mem_write(0x5a7ddc,scales[4:])
    for offset,value in [(0x14,vertex+18),(0x18,index),(0x1c,reuse),(0x28,uv),(0x398,model)]:u.mem_write(esp+offset,struct.pack('<I',value))
    u.mem_write(esp+0x11,bytes([lighting]));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_ESI,index*12);u.reg_write(UC_X86_REG_EBX,index)
    u.emu_start(0x52edac,0x52f3cc,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x52f3cc
    result=[]
    for i in range(8):
        result.append(bytes(u.mem_read(0x1bf7000+i*12,12))+bytes(u.mem_read(0x1bdb2b0+i*12,12))+bytes(u.mem_read(0x1c3d554+i,1))+bytes(u.mem_read(0x1bf29b0+i,1))+bytes(u.mem_read(0x1c3f494+i*3,3))+records[i][29:])
    clipped+=bool(records[index-distance][24]);expected.append(bytes(4)+b''.join(result)+bytes(u.mem_read(vertex,40)))
probe=str(root/'build/pc/Release/rf_model_probe.exe')
actual=subprocess.check_output([probe,'--render-reuse'],input=b''.join(cases))
assert actual==b''.join(expected)
for index,distance in [(8,1),(0,1),(3,0),(3,-1),(3,4)]:
    case=bytearray(cases[0]);struct.pack_into('<Ii',case,272,index,distance)
    assert subprocess.check_output([probe,'--render-reuse'],input=case)==struct.pack('<i',-4)+case[:256]+b'\xa5'*40
report=dict(result='PASS',cases=len(cases),clipped=clipped,visible=len(cases)-clipped,port_bounds_cases=5,
    scope='Unchanged positive-reuse branch 0x52edac through 0x52f3cc with full copy/depth callees; all cache arrays and 40 output bytes exact, including preserved fields; supplied cache state, no fresh projection or draw integration')
(root/'artifacts/render-reuse-verification.json').write_text(json.dumps(report,indent=2));print(report)
