"""Compiled NXDK companion to full original clutter factory oracle."""
import re,struct,subprocess
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
ROOT=Path(__file__).resolve().parents[1]
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image()
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096)
x.mem_write(p.OPTIONAL_HEADER.ImageBase,im);B=0x30000000;x.mem_map(B,0x200000)
sym=(ROOT/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_clutter_create\s+([0-9a-fA-F]+)',sym)[1],16)
C,S,L,N,BE,OUT,POS,MATRIX,STACK,STOP,ALLOC,CALL=B+0x1000,B+0x2000,B+0x3000,B+0x4000,B+0x5000,B+0x5100,B+0x6000,B+0x6100,B+0xe000,B+0xf000,B+0xf100,B+0xf200
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
def text(a):
    b=bytearray()
    while x.mem_read(a,1)!=b'\0':b+=x.mem_read(a,1);a+=1
    return b.decode()
def setat(buf,offset,data):buf[offset:offset+len(data)]=data
mode=None
def hook(cpu,address,length,context):
    global emitted
    if address not in (ALLOC,CALL):return
    sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+i*4);status=0
    if mode=='allocation' and address==ALLOC:
        cpu.reg_write(UC_X86_REG_EAX,0xffffffff);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
    if address==ALLOC:
        d=arg(1);expected=w(B+0x7100,g['kind'],g['material'],0x20 if initialflags&6 else 0,0x100000 if g['shield'] else 0,123)+g['position']+g['matrix']+g['radius']
        assert bytes(cpu.mem_read(d,76))==expected,('descriptor',g['case'])
        wiretrace.extend(w(-1)+expected)
        cpu.mem_write(arg(2),w(0 if g['fail'] else S));trace.append(('allocate',))
    else:
        assert arg(1)==S;op,q,out=arg(2),arg(3),arg(4)
        v=list(struct.unpack('<5I',cpu.mem_read(q,20)));txt=r(q+20);pos=r(q+24);result=0
        wiretrace.extend(w(op,*v)+(text(txt).encode() if txt else b'').ljust(32,b'\0')+(bytes(cpu.mem_read(pos,12)) if pos else b'\0'*12)+b'\0'*12)
        if mode=='tag' and op==4:
            cpu.reg_write(UC_X86_REG_EAX,0xffffffff);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
        if op==0:
            assert v[:3]==[g['sound'],0x3f800000,0] and pos==S+24
            trace.append(('sound',g['sound']));result=0x7654
        elif op==1:assert v[0]==0x7654;trace.append(('sound-handle',));result=0x4567
        elif op==2:
            assert v[0]==55 and v[2:4]==[0x12345678,1] and pos==S+24
            index=v[1];trace.append(('emitter',index))
            if index%2==0:result=B+0x8000+emitted*0x200;emitted+=1
        elif op==3:
            assert v[0]==B+0x8000+(emitted-1)*0x200 and v[1]==(B+0x8000+(emitted-2)*0x200 if emitted>1 else 0)
            assert r(S+80)==v[0]
        elif op==4:
            assert v[0]==g['model'];name=text(txt);trace.append(('tag',name))
            if name.startswith('corona_') and name[7:].isdigit():
                n=int(name[7:]);result=100+n if n<=g['coronas'] else -1
            elif name=='corona_rod1':result=51 if g['rod'] else -1
            elif name=='corona_rod2':result=52
            elif name=='light_prop':result=77
            else:raise AssertionError(name)
        elif op==5:
            assert v[0]==55 and v[2:4]==[g['glare']&0xffffffff,0];trace.append(('glare',v[1]))
        elif op==6:assert v==[55,g['rodclass'],51,52,0xffffffff];trace.append(('rod',))
        elif op==7:assert v==[55,0xffffffff,64,32,1];trace.append(('screen',))
        elif op==8:assert v[0]==g['explosion'];trace.append(('explosion',))
        elif op==9:
            assert v[0]==g['OBJ'] and r(L+8)==7 and r(L+4)==L;trace.append(('collision',))
        elif op==10:
            assert v[:2]==[g['slot'],1] and r(L+8)==8 and r(L+4)==S+100;trace.append(('slot',g['slot']))
        else:raise AssertionError(op)
        if mode=='slot' and op==10:status=0xffffffff
        if mode=='rod' and op==4 and text(txt)=='corona_rod2':result=-1
        cpu.mem_write(out,w(result))
    cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook)
def verify(values):
    global g,trace,emitted,initialflags,wiretrace,fixture
    g=values;trace=[];emitted=0;wiretrace=bytearray();u=g['u'];original=g['OBJ'];original_class=g['CLS']
    initialflags=struct.unpack_from('<I',g['class_before'],0x74)[0] if g['fail'] else g['initial_class_flags']
    # The original oracle preserves incoming flags separately because success mutates them.
    cls=bytearray(96);setat(cls,0,w(B+0x7000,B+0x7100,B+0x7200,B+0x7300,len(g['emitters'])))
    setat(cls,20,struct.pack('<fIff',g['lifetime'],g['kind'],g['life'],struct.unpack('<f',g['radius'])[0]))
    setat(cls,36,w(g['material'],initialflags,g['sound'],g['explosion'],g['glare'],g['rodclass']))
    initialcached=[201,202] if initialflags&0x400 else []
    setat(cls,64,w(*initialcached));setat(cls,80,w(len(initialcached),99,64,32))
    x.mem_write(C,bytes(cls));x.mem_write(B+0x7000,b'fixture\0');x.mem_write(B+0x7100,b'fixture.v3d\0')
    x.mem_write(B+0x7200,b'fixture\0' if g['case']&4 else b'\0');x.mem_write(B+0x7300,w(*g['emitters']))
    x.mem_write(B+0x7400,b'\0' if g['case']&8 else b'instance\0')
    state=bytearray(b'\xa5'*108)
    setat(state,0,w(original,0x12345678,55,g['model'],g['objectflags'],g['before'][0x1a8]))
    setat(state,24,g['position']);setat(state,80,w(0));setat(state,100,w(0,0))
    x.mem_write(S,bytes(state));x.mem_write(L,w(L,L,7,7));x.mem_write(N,w(g['slot']))
    x.mem_write(BE,w(ALLOC,CALL,0));x.mem_write(OUT,w(0xa5a5a5a5));x.mem_write(POS,g['position']);x.mem_write(MATRIX,g['matrix'])
    pcinput=bytes(cls)+bytes(state)+g['position']+g['matrix']+w(g['fail'],g['coronas'],g['rod'],g['register'],g['slot'],int(bool(g['case']&8))|(2 if g['shield'] else 0),bool(g['case']&4))+w(*g['emitters'])
    x.mem_write(STACK,w(STOP,C,1,0,0 if g['shield'] else -1,B+0x7400,123,POS,MATRIX,g['register'],12345,N,L,BE,OUT))
    fixture=(bytes(cls),bytes(state),bytes(x.mem_read(STACK,60)))
    x.reg_write(UC_X86_REG_ESP,STACK)
    try:x.emu_start(entry,STOP,count=1000000)
    except Exception:print('NXDK stopped',g['case'],hex(x.reg_read(UC_X86_REG_EIP)),trace);raise
    assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0
    assert trace==g['trace'],(g['case'],'trace',trace,g['trace'])
    pc=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--clutter-create'],input=pcinput)
    wanted=w(0,int(not g['fail']))+bytes(x.mem_read(C,96))+bytes(x.mem_read(S,108))+w(r(N),r(L+8),len(wiretrace)//80)+wiretrace
    assert pc==wanted,(g['case'],'PC/NXDK output and full request trace')
    if g['fail']:
        assert r(OUT)==0 and bytes(x.mem_read(S,108))==state and bytes(x.mem_read(C,96))==cls;return
    assert r(OUT)==S
    expected=state[:];setat(expected,16,u.mem_read(original+0x7c,4));setat(expected,36,u.mem_read(original+0x34,8))
    setat(expected,44,w(B+0x7000 if g['case']&8 else B+0x7400,C,0))
    setat(expected,56,u.mem_read(original+0x29c,8));setat(expected,64,u.mem_read(original+0x2a4,8))
    setat(expected,72,u.mem_read(original+0x2b0,8));setat(expected,80,u.mem_read(original+0x268,4))
    setat(expected,84,u.mem_read(original+0x2b8,8));setat(expected,92,u.mem_read(original+0x2d0,4))
    expected[96]=0;setat(expected,98,u.mem_read(original+0x2d4,2));setat(expected,100,w(L,L))
    actual=bytes(x.mem_read(S,108));assert actual==expected,(g['case'],'state',[(i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b])
    setat(cls,40,u.mem_read(original_class+0x74,4));setat(cls,60,u.mem_read(original_class+0x70,4))
    setat(cls,64,u.mem_read(original_class+0x88,16));setat(cls,80,u.mem_read(original_class+0x84,4));setat(cls,84,u.mem_read(original_class+0x98,4))
    assert bytes(x.mem_read(C,96))==cls,(g['case'],'class')
    assert r(N)==g['r'](0x5afb84) and r(L+8)==8 and r(L)==S+100 and r(L+4)==S+100

def guards():
    global mode,trace,wiretrace,emitted,g
    original_g=g;g=dict(g);g['fail']=False
    def reset():
        global trace,wiretrace,emitted
        trace=[];wiretrace=bytearray();emitted=0
        x.mem_write(C,fixture[0]);x.mem_write(S,fixture[1]);x.mem_write(STACK,fixture[2])
        x.mem_write(OUT,w(0xa5a5a5a5));x.mem_write(N,w(g['slot']));x.mem_write(L,w(L,L,7,7))
        x.mem_write(POS,g['position']);x.mem_write(BE,w(ALLOC,CALL,0))
    def run():
        x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000)
        assert x.reg_read(UC_X86_REG_EIP)==STOP
        return x.reg_read(UC_X86_REG_EAX)
    for label in ('index','position','class-name','backend','time','corona-count'):
        reset()
        if label=='index':x.mem_write(STACK+12,w(1))
        elif label=='position':x.mem_write(POS,w(0x7fc00000))
        elif label=='class-name':x.mem_write(C,w(0))
        elif label=='backend':x.mem_write(BE+4,w(0))
        elif label=='time':x.mem_write(STACK+40,w(0xffffffff))
        else:x.mem_write(C+80,w(5))
        before=bytes(x.mem_read(S,108));assert run()==0xfffffffc,label
        assert r(OUT)==0xa5a5a5a5 and not trace and bytes(x.mem_read(S,108))==before,label
    reset();mode='allocation';assert run()==0xffffffff and r(OUT)==0xa5a5a5a5
    reset();mode='tag';assert run()==0xffffffff and r(OUT)==S and r(L+8)==7
    reset();mode='slot';g['slot']=0;x.mem_write(N,w(0));x.mem_write(STACK+36,w(1))
    assert run()==0xffffffff and r(OUT)==S and r(L+8)==8 and r(N)==1
    assert bytes(x.mem_read(S+98,2))==b'\0\0'
    reset();mode='rod';g['rod']=True
    assert run()==0xfffffffe and r(OUT)==S and r(L+8)==7
    mode=None;g=original_g;return 10
