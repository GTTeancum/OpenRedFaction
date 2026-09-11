"""Original ordinary allocator and status callees vs PC/NXDK callback policy."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
base=0x30000000;stack=base+0xe000;stop=base+0xf000
def machine(path):
    p=pefile.PE(str(path));image=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(image)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,image)
    m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_audio_select_ordinary\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda m,p,n=1:struct.unpack('<'+'I'*n,m.mem_read(p,4*n))
trace=[]
def hook(m,address,size,context):
    sp=m.reg_read(UC_X86_REG_ESP);ret=read(m,sp)[0];pop=0;result=0
    if address==base+0x1000:
        obj,out=read(m,sp+4,2);index=(obj-(base+0x2000))//8
        assert 0<=index<30
        trace.extend((1,index));result=facts[index][2];m.mem_write(out,w(facts[index][3]));pop=8
    elif address==0x521930:
        index=read(m,sp+4)[0];trace.extend((2,index))
    elif address==base+0x1010:
        ctx,index,out=read(m,sp+4,3);trace.extend((1,index))
        result=facts[index][2];m.mem_write(out,w(facts[index][3]))
    else:
        ctx,index=read(m,sp+4,2);trace.extend((2,index))
    m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,ret)
u.mem_write(base+0x1800+0x24,w(base+0x1000))
for i in range(55):u.mem_write(base+0x2000+i*8,w(base+0x1800))
for address in (base+0x1000,0x521930):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
for address in (base+0x1010,base+0x1020):x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
x.mem_write(base+0x1800,w(base+0x1010,base+0x1020))
def call(m,address,args):
    m.mem_write(stack,w(stop)+args);m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=10000)
    assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_ESP)==stack+4
    return m.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x522470);commands=bytearray();expected=bytearray();selected=[0]*31;releases=queries=0
for case in range(4096):
    facts=[[1,1,0,1] for _ in range(55)]
    if case<3000:
        target=case%55
        facts[target]=[(0,1,2)[case//55%3],case//165%16,(-1,0,1,-2147483648)[case//7%4],case//11%4]
    else:
        facts=[[rng.randrange(3),rng.getrandbits(8),(-1,0,1)[rng.randrange(3)],rng.getrandbits(4)] for _ in range(55)]
    if case>=4094:
        facts=[[1,0,0,1] for _ in range(55)]
        if case==4094:facts[29][3]=0
    records=bytearray(rng.randbytes(55*44));compact=bytearray()
    for i,(present,flags,status,bits) in enumerate(facts):
        records[i*44:i*44+4]=w(base+0x2000+i*8 if present else 0);records[i*44+40:i*44+44]=w(flags)
        compact.extend(w(present,flags))
    u.mem_write(0x1ad7520,bytes(records));trace=[]
    result=call(u,0x522470,w(case))  # Original ignores the extra caller argument.
    original_trace=trace[:];assert bytes(u.mem_read(0x1ad7520,len(records)))==records
    selected[30 if result==0xffffffff else result]+=1
    queries+=trace[::2].count(1);releases+=trace[::2].count(2)
    expected.extend(w(result,len(trace)//2)+w(*(trace+[0]*(62-len(trace)))))
    commands.extend(b''.join(w(*row) for row in facts))
    x.mem_write(base,bytes(compact));trace=[]
    actual=call(x,entry,w(base,base+0x1800,0))
    assert actual==result and trace==original_trace,(case,actual,result,trace,original_trace)
    assert bytes(x.mem_read(base,len(compact)))==compact
actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--audio-allocation'],input=commands)
assert actual==expected and all(selected)
report=dict(result='PASS',cases=4096,selected_slots_and_failure=selected,status_queries=queries,releases=releases,
            original_sha256=digest,scope='Original522470 and5224d0/522500 execute; only DirectSound GetStatus and521930 release are supplied. '
            'Exact selection/callback order vs PC/NXDK, all30 indexes, ignored remaining25 entries, status HRESULT/bit tests, flags and no mutation. No live allocator integration.')
(root/'artifacts/audio-allocation.json').write_text(json.dumps(report,indent=2));print(report)
