"""Compare complete original static triangle processing with shared composition."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,65536);stack=b+32000;batch=b+1024;triangle=b+2048;face_address=b+2200;reuse_address=b+2300;uv_address=b+2400;colors_address=b+2500;gpu=b+4096;indices=b+8192;material=b+9000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def put(a,v):u.mem_write(a,w(v))
rng=random.Random(0x52e43e);cases=[];expected=[]
for n in range(512):
    verts=[];cache=[];clip=[];reuse=[0,0,0,0];colors=rng.randbytes(12)
    far=4.;near=.125;compute=(n>>1)&1;screen=(n>>2)&1;perspective=n&1
    for i in range(4):
        xyz=[rng.randint(-32,32)/8,rng.randint(-32,32)/8,rng.randint(1,48)/8]
        mask=(8 if xyz[0]>xyz[2] else 0)|(4 if -xyz[2]>xyz[0] else 0)|(32 if xyz[1]>xyz[2] else 0)|(16 if -xyz[2]>xyz[1] else 0)|(2 if xyz[2]>far else 0)
        c=bytearray(b'\xa5'*32);c[24]=mask if compute or screen else 0;c[26:29]=rng.randbytes(3);cache.append(bytes(c));clip.append(struct.pack('<3f',*xyz))
        verts.append(struct.pack('<8f',*xyz,0,0,1,rng.randrange(16)/8,rng.randrange(16)/8)+bytes(8))
        u.mem_write(0x1c0e700+i*12,clip[-1]);u.mem_write(0x1c3d554+i,bytes(c[24:25]));u.mem_write(0x1c3f494+i*3,bytes(c[26:29]));u.mem_write(uv_address+i*8,verts[-1][24:32])
    tri=struct.pack('<4H',0,1,2,32 if n%3 else 0);face=struct.pack('<4f',0,0,-1,1 if n%7 else -1)
    view=struct.pack('<23f5I',0,0,0,1,0,0,0,1,0,0,0,1,1,2,320,-240,320,240,0,0,640,480,far,perspective,compute,1,1,screen)
    planes=struct.pack('<8f',near,far,0,0,0,0,0,0);projection=struct.pack('<2f2ifI',320,240,0,0,0,1);attributes=struct.pack('<I4B2f',1,40,50,60,n%256,.25,2)
    use_colors=(n>>3)&1;index_base=65534 if n%11==0 else 17
    case=b''.join(verts)+struct.pack('<4i',*reuse)+b''.join(cache)+b''.join(clip)+tri+face+view+planes+projection+attributes+w(use_colors)+colors+w(index_base);assert len(case)==580;cases.append(case)
    u.mem_write(batch,bytes(56));put(batch+12,uv_address);put(batch+16,face_address);put(batch+20,triangle);put(batch+24,reuse_address);u.mem_write(batch+40,struct.pack('<2H',4,1));u.mem_write(triangle,tri);u.mem_write(face_address,face);u.mem_write(reuse_address,bytes(8));u.mem_write(colors_address,colors)
    u.mem_write(gpu,b'\xa5'*2560);u.mem_write(indices,b'\xa5'*288);u.mem_write(material+8,bytes([n%256]));u.mem_write(0x1818690,view[:12]);u.mem_write(0x18186e0,view[36:48]);u.mem_write(0x5a4d19,bytes([perspective]));u.mem_write(0x5a4d18,b'\1');u.mem_write(0x1818b65,b'\1');u.mem_write(0x1818b6c,struct.pack('<f',far));u.mem_write(0x1818b78,struct.pack('<f',near));put(0x17c7bcc,0x66)
    u.mem_write(0x1818a5c,struct.pack('<f',320));u.mem_write(0x1818a24,struct.pack('<f',240));u.mem_write(0x17c7bec,bytes(8));u.mem_write(0x1e652e8,bytes(4));u.mem_write(0x5a445a,b'\1');u.mem_write(0x5a7dd8,attributes[8:]);u.mem_write(0x17c7c30,struct.pack('<f',2))
    u.mem_write(stack,bytes(1024));u.mem_write(stack+0x11,bytes([compute,0,screen]))
    for off,value in {0x18:batch,0x24:index_base,0x2c:colors_address if use_colors else 0,0x40:4,0x58:gpu,0x68:indices,0x74:64,0x350:material}.items():put(stack+off,value)
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,batch);u.reg_write(UC_X86_REG_EBP,144)
    u.emu_start(0x52e43e,0x52e842,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x52e842
    expected.append(bytes(4)+bytes(u.mem_read(stack+0x40,4))+bytes(u.mem_read(stack+0x14,4))+bytes(u.mem_read(gpu,2560))+bytes(u.mem_read(indices,288)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--emit-static-batch'],input=b''.join(cases));assert len(actual)==len(cases)*2860
def compare(data,references):
    exact=0;maximum=0;max_pixels=0;max_rgb=0;rgb_differences=0
    for n,reference in enumerate(references):
        observed=data[n*2860:(n+1)*2860];exact+=observed==reference
        assert observed[:12]==reference[:12] and observed[2572:]==reference[2572:],('counts/indices',n)
        count=struct.unpack_from('<I',reference,4)[0]
        assert observed[12:172]==reference[12:172] and observed[12+count*40:2572]==reference[12+count*40:2572],('untouched',n)
        for i in range(4,count):
            left=observed[12+i*40:52+i*40];right=reference[12+i*40:52+i*40]
            assert left[19:24]==right[19:24] and left[32:]==right[32:],('attributes',n,i,left.hex(),right.hex())
            for channel in range(16,19):
                difference=abs(left[channel]-right[channel]);max_rgb=max(max_rgb,difference);rgb_differences+=difference!=0
                assert difference<=1,('rgb',n,i,channel,left[channel],right[channel])
            for off in (0,4,8,12,24,28):
                x=struct.unpack_from('<f',left,off)[0];y=struct.unpack_from('<f',right,off)[0]
                error=abs(x-y)
                if off in (0,4):
                    max_pixels=max(max_pixels,error);assert error<=2e-6*(320 if off==0 else 240),('screen',n,i,off,x,y)
                else:
                    error/=max(1,abs(y));maximum=max(maximum,error);assert error<=2e-6,('float',n,i,off,x,y)
    return dict(bit_exact_batches=exact,max_scaled_error=maximum,max_screen_pixel_error=max_pixels,max_rgb_error=max_rgb,rgb_differences=rgb_differences)
pc_metrics=compare(actual,expected)
# Compare compiled NXDK to both original and PC using the same explicit bounds.
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(b,65536)
entry=int(re.search(r'\s_rf_model_geometry_emit_static_batch\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
g=b+1024;draw=b+1100;buffers=b+1200;output=b+1300;pool=b+14000;sentinel=b+60000
nxdk_results=[]
for n,case in enumerate(cases):
    x.mem_write(b,case);x.mem_write(g,w(draw,b,b+352,b+160,1,4,1,0));x.mem_write(draw,w(0,4,0,1,0));x.mem_write(buffers,w(b+176,b+304,0,gpu,4));x.mem_write(output,w(gpu,indices,4,64,0,144));x.mem_write(pool,bytes(2520));x.mem_write(gpu,b'\xa5'*2560);x.mem_write(indices,b'\xa5'*288)
    args=[g,0,buffers,b+376,b+488,b+520,b+544,struct.unpack_from('<I',case,576)[0],pool,output,b+360,b+564 if struct.unpack_from('<I',case,560)[0] else 0]
    x.mem_write(stack,w(sentinel,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,sentinel,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==sentinel and x.reg_read(UC_X86_REG_EAX)==0,n
    record=bytes(4)+bytes(x.mem_read(output+8,4))+bytes(x.mem_read(output+16,4))+bytes(x.mem_read(gpu,2560))+bytes(x.mem_read(indices,288))
    nxdk_results.append(record)
    assert bytes(x.mem_read(b,580))==case,n
assert b''.join(nxdk_results)==b''.join(expected),'NXDK must retain exact original output'
case=next(c for c,e in zip(cases,expected) if struct.unpack_from('<I',e,4)[0]>4)
for guard in range(4):
    x.mem_write(b,case);x.mem_write(g,w(draw,b,b+352,b+160,1,4,1,0));x.mem_write(draw,w(0,4,0,1,0));x.mem_write(buffers,w(b+176,b+304,0,gpu,4));x.mem_write(output,w(gpu,indices,4,64,0,144));x.mem_write(pool,bytes(2520));x.mem_write(gpu,b'\xa5'*2560);x.mem_write(indices,b'\xa5'*288)
    args=[g,0,buffers,b+376,b+488,b+520,b+544,0,pool,output,b+360,0]
    if guard==0:args[10]=0
    if guard==1:x.mem_write(b+352,struct.pack('<H',4))
    if guard==2:x.mem_write(output+12,w(4))
    if guard==3:x.mem_write(output+20,w(2))
    x.mem_write(stack,w(sentinel,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,sentinel,count=100000)
    assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc,guard
    assert bytes(x.mem_read(gpu,2560))==b'\xa5'*2560 and bytes(x.mem_read(indices,288))==b'\xa5'*288,guard
    assert bytes(x.mem_read(output+8,4))==w(4) and bytes(x.mem_read(output+16,4))==w(0),guard
nxdk_metrics=compare(b''.join(nxdk_results),expected)
shared_metrics=compare(b''.join(nxdk_results),[actual[n*2860:(n+1)*2860] for n in range(len(cases))])
report=dict(result='PASS_WITH_NUMERIC_DIFFERENCES',pc_to_original=pc_metrics,nxdk_to_original=nxdk_metrics,nxdk_to_pc=shared_metrics,batches=len(cases),nxdk_batches=len(cases),nxdk_guards=4,generated=sum(struct.unpack_from('<I',e,4)[0]-4 for e in expected),indices=sum(struct.unpack_from('<I',e,8)[0] for e in expected),scope='Complete original52e43e..52e842 triangle loop, actual facing/clipping/projection/fan callees, supplied prepared caches and stored planes; counts/indices/preserved bytes exact; generated screen tolerance2e-6*axis-scale pixels, UV/depth2e-6 scaled, RGB1 byte step; no GPU submission or native XEMU claim')
(root/'artifacts/model-static-emission.json').write_text(json.dumps(report,indent=2));print(report)
