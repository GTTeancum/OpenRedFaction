"""Original ambient voice branch vs PC/NXDK with supplied audio boundaries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;slot_address=base+0x2000
def machine(path):
    p=pefile.PE(str(path));image=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(image)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,image)
    m.mem_map(base,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_ambient_voice_update\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda m,p,n=1:struct.unpack('<'+'I'*n,m.mem_read(p,n*4))
trace=[];gain_calls=0
def hook(m,address,size,user):
    global trace,gain_calls
    sp=m.reg_read(UC_X86_REG_ESP);ret=read(m,sp)[0];result=0
    if address==0x505740:
        sample,position,pan_out,gain_out,volume=read(m,sp+4,5)
        assert sample==read(m,0x1754170)[0] and position==0x1754178 and volume==read(m,0x1754184)[0]
        m.mem_write(pan_out,pan);m.mem_write(gain_out,gain)
    elif address in (0x5439d0,0x543a80):
        sample,g,p,z1,z2=read(m,sp+4,5);assert z1==z2==0
        trace=[1,voice&0xffffffff,sample,g,p,int(address==0x543a80),read(m,0x1754174)[0]];result=start_result
    elif address==0x5442b0:
        trace=[2,read(m,sp+4)[0],0,0,0,0,read(m,0x1754174)[0]]
    elif address==0x543c20:
        sample,position,scale=read(m,sp+4,3);assert position==0x1754178 and scale==0x3f800000
        trace=[3,voice&0xffffffff,sample,0,0,1,read(m,0x1754174)[0]]
        m.reg_write(UC_X86_REG_EIP,base+0x1100);return
    elif address==0x544390:
        assert read(m,sp+4,2)==(voice&0xffffffff,0x3ec00000);gain_calls+=1
    else:
        before=read(m,slot_address+4)[0]
        if address==base+0x1000:
            context,sample,g,p,loop=read(m,sp+4,5)
            trace=[1,before,sample,g,p,loop,before];result=start_result
        elif address==base+0x1010:
            context,v=read(m,sp+4,2);trace=[2,v,0,0,0,0,before]
        else:
            context,v,sample,position=read(m,sp+4,4)
            assert bytes(m.mem_read(position,12))==bytes(m.mem_read(slot_address+8,12))
            trace=[3,v,sample,0,0,1,before]
    m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for address in (0x505740,0x5439d0,0x543a80,0x5442b0,0x543c20,0x544390):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
u.mem_write(base+0x1100,b'\xd9\x05'+w(base+0x1200)+b'\xc3');u.mem_write(base+0x1200,f(.375))
for address in (base+0x1000,base+0x1010,base+0x1020):x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
x.mem_write(base+0x2100,w(base+0x1000,base+0x1010,base+0x1020))
rng=random.Random(0x505f75);commands=bytearray();expected=bytearray();counts=[0]*4;rounding_edges=0
threshold=struct.unpack('<f',f(.1))[0]
for case in range(4096):
    sample=(-2,-1,0,1,2599)[case%5];voice=(-2147483648,-2,-1,0,1,99)[case//5%6]
    looping=(0,1,2,255,256,257)[case//30%6];category_value=(.3,.7,1/3,1.,2.,0.,-1.)[case//180%7]
    category=f(category_value);category_value=struct.unpack('<f',category)[0]
    if category_value>0:
        bits=struct.unpack('<I',f(threshold/category_value))[0]+(case//1260%5)-2;gain=w(bits)
    else:gain=f(rng.uniform(-2,2))
    if case%13==0:gain=f(rng.uniform(-2,2))
    pan=f(rng.uniform(-1,1));start_result=(-2147483648,-2,-1,0,123)[case//7%5]
    slot=w(sample,voice)+f(1.,2.,3.,.8)
    u.mem_write(0x1754170,slot);u.mem_write(0x17543d4,w(0));u.mem_write(0x1753c18,category)
    if sample>=0:u.mem_write(0x1cd3be6+sample*64,bytes([looping&255]))
    u.reg_write(UC_X86_REG_ESI,0x1754174);u.reg_write(UC_X86_REG_ESP,stack)
    trace=[0]*7;gain_calls=0;u.emu_start(0x505f75,0x50603c,count=1000)
    assert u.reg_read(UC_X86_REG_EIP)==0x50603c and u.reg_read(UC_X86_REG_ESP)==stack
    original_trace=trace[:];assert gain_calls==int(trace[0]==3)
    after=bytes(u.mem_read(0x1754170,24));expected.extend(after+w(*trace));counts[trace[0]]+=1
    product=struct.unpack('<f',gain)[0]*category_value
    if sample>=0 and struct.unpack('<f',f(product))[0]==threshold and product<threshold:
        rounding_edges+=1;assert trace[0] in (0,2)
    x.mem_write(slot_address,slot);x.mem_write(stack,w(stop,slot_address)+gain+pan+category+w(looping,base+0x2100,0))
    x.reg_write(UC_X86_REG_ESP,stack);trace=[0]*7;x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
    assert bytes(x.mem_read(slot_address,24))==after and trace==original_trace,(case,trace,original_trace)
    commands.extend(slot+gain+pan+category+w(looping,start_result))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--ambient-voice'],input=commands)
assert actual==expected and all(counts) and rounding_edges
report=dict(result='PASS',cases=4096,actions=counts,unrounded_threshold_cases=rounding_edges,original_sha256=digest,
            scope='Original505f75..50603c with spatial/gain/device boundaries supplied; full slot bytes and callback argument/order/state visibility versus PC/NXDK. No actual volume-calculation/device playback proof.')
(root/'artifacts/ambient-voice.json').write_text(json.dumps(report,indent=2));print(report)
