"""Compare original dying-update traces to shared PC and NXDK orchestration."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
b=0x30000000;player=b+0x1000;backend=b+0x2000;callback=b+0x3000;segment_callback=callback+16;stack=b+0xe000;stop=b+0xf000
x.mem_map(b,65536)
entry=int(re.search(r'\s_rf_entity_dying_update\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
read=lambda a,n:list(struct.unpack('<'+'I'*n,x.mem_read(a,4*n)))
trace=[];segment=[];facts=[];inputs=[];outputs=[]
def hook(m,a,size,data):
    if a not in (callback,segment_callback):return
    sp=m.reg_read(UC_X86_REG_ESP);ret=read(sp,1)[0]
    if a==callback:
        _,op,v1,v2=read(sp+4,4);trace.extend((op,v1,v2));value=facts[op]
    else:
        _,start,end,point,radius=read(sp+4,5);trace.extend((14,0,0))
        segment[:]=read(start,3)+read(end,3)+read(point,3)+[radius];value=facts[14]
    m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook)
opcodes=[0x42e3c0,0x42ed20,0x428d10,0x41a830,0x41ae70,0x4fa3f0,0x4892c0,0x40e0b0,0x418f80,0x5001d0,0x4c0e00,0x4c0200,0x4be410,0x4b6760,0x506ae0]
def compare(c):
    global trace,segment,facts
    wire=w(77,c['flags'],c['action'],9,c['burn'],c['special'])+struct.pack('<7f',1,2,3,0,0,1,c['radius'])
    final=bytearray(wire);final[16:20]=c['body'][0x13d8:0x13dc]
    player_wire=w(88,99)+struct.pack('<3f',4,5,6)
    facts=[c['answers'].get(a,0) for a in opcodes]
    expected=[]
    for address,args in c['trace']:
        op=opcodes.index(address)
        if op in (0,1,3,4):pair=(args[0],args[1] if len(args)>1 else 0)
        elif op==2:pair=(args[1],0)
        elif op==6:pair=(args[0],args[2])
        elif op==7:pair=(args[0],args[2])
        elif op in (10,11,12):pair=(args[0],0)
        elif op==13:pair=(c['found_b'],0)
        else:pair=(0,0)
        expected.extend((op,*pair))
    expected_segment=sum(c['segment'][:3],[])+[c['segment'][3]] if c['segment'] else [0]*10
    trace=[];segment=[];x.mem_write(b,wire);x.mem_write(player,player_wire)
    x.mem_write(backend,w(callback,segment_callback,0,player if c['present'] else 0))
    x.mem_write(stack,w(stop,b,backend));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,c['case']
    assert trace==expected,(c['case'],trace,expected)
    assert (segment or [0]*10)==expected_segment,c['case']
    assert bytes(x.mem_read(b,52))==final,c['case']
    inputs.append(wire+player_wire+w(c['present'])+w(*facts))
    outputs.append(w(0)+final+w(len(expected)//3,*expected,*([0]*(48-len(expected))),*expected_segment))
runpy.run_path(str(root/'tools/verify_dying_update_original.py'),init_globals={'observe_case':compare})
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--dying-update'],input=b''.join(inputs))
assert pc==b''.join(outputs),'PC trace/state mismatch'
report=dict(result='PASS',cases=len(inputs),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Complete41ee40 control flow compared with PC/NXDK shared C: callback sequence/arguments, segment coordinates/radius and state writes match original4096 stable-owner cases. Backend effect implementations, reentrant mutations and native scene dispatch remain open.')
(root/'artifacts/dying-update.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
