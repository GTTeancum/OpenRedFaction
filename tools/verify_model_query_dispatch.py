"""Audit original503120/5031f0 model dispatch, supplying only geometry callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));data=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(data)+4095)//4096*4096);u.mem_write(0x400000,data)
b=0x30000000;u.mem_map(b,0x10000)
model=b+0x1000;primary=b+0x2000;owner=b+0x3000;parts=b+0x6000;query=b+0x7000;hit=b+0x8000;stack=b+0xe000;stop=b+0xf000
w=lambda *a:struct.pack('<'+'I'*len(a),*(v&0xffffffff for v in a))
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[];expected_call=None;expected_input=None;fixture=None;return_word=0
def geometry(cpu,address,size,context):
    if address not in (0x54e140,0x54e000,0x54daa0):return
    sp=cpu.reg_read(UC_X86_REG_ESP);count=3 if address==0x54e000 else 4
    args=tuple(struct.unpack('<'+'I'*count,cpu.mem_read(sp+4,count*4)))
    observed=(address,cpu.reg_read(UC_X86_REG_ECX),args)
    assert observed==expected_call,('callee ABI',observed,expected_call)
    assert bytes(cpu.mem_read(hit,32))==expected_input,'reset must precede callee'
    trace.append(observed);cpu.mem_write(hit,fixture);cpu.reg_write(UC_X86_REG_EAX,return_word)
    cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+count*4)
u.hook_add(UC_HOOK_CODE,geometry)
xp=pefile.PE(str(root/'build/xbox/main.exe'));xd=xp.get_memory_mapped_image();xo=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xo,(len(xd)+4095)//4096*4096);x.mem_write(xo,xd);x.mem_map(b,0x10000)
mapping=(root/'build/xbox/main.map').read_text()
symbol=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry=symbol('rf_collision_model_query');all_entry=symbol('rf_collision_model_query_all')
poses=b+0x9000;backend=b+0xa000;callback=b+0xb000;native_trace=[];commands=[];answers=[]
def native_geometry(cpu,address,size,context):
    if address!=callback:return
    sp=cpu.reg_read(UC_X86_REG_ESP);args=tuple(struct.unpack('<8I',cpu.mem_read(sp+4,32)))
    assert args==expected_native_args,('native callback',args,expected_native_args)
    assert bytes(cpu.mem_read(hit,32))==expected_input
    native_trace.append(args);cpu.mem_write(hit,fixture);cpu.reg_write(UC_X86_REG_EAX,return_word)
    cpu.reg_write(UC_X86_REG_EIP,struct.unpack('<I',cpu.mem_read(sp,4))[0]);cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,native_geometry)
rng=random.Random(0x503120);calls={hex(a):0 for a in (0x54e140,0x54e000,0x54daa0)};resets=0;rejections=0;wrappers=0
for case in range(4096):
    kind=(0,1,2,3,4,0xffffffff)[case%6];reset=(0,1,0x100,0x101,2,255,0xffffffff)[case//6%7]
    wrapper=(case//42)%2==0;part=-1 if wrapper else (-1,0,1,3)[case//2%4]
    pose_count=1+case%4;part_count=(-1,0,1,4)[case//4%4]
    seed=bytes(rng.getrandbits(8) for _ in range(32));query_data=bytes(rng.getrandbits(8) for _ in range(84))
    fixture=bytes(rng.getrandbits(8) for _ in range(32));return_word=(0,1,0x100,0x101,0xdeadbeef)[case//3%5]
    u.mem_write(model,w(kind,primary,owner));u.mem_write(primary,b'\x5a'*32)
    u.mem_write(owner,b'\xa5'*0x2000);u.mem_write(owner+0x19bc,w(pose_count));u.mem_write(owner+0x48,w(parts,part_count))
    u.mem_write(parts,b'\x35'*0x800)
    for n in range(4):u.mem_write(parts+n*0x124+0x114,bytes([(case>>(n+1))&255]))
    u.mem_write(query,query_data);u.mem_write(hit,seed)
    expected_input=bytearray(seed)
    if reset&255==1:
        expected_input[:4]=w(0x3f800000);expected_input[28:32]=w(0);resets+=1
    expected_input=bytes(expected_input);expected_call=None
    if kind==2:expected_call=(0x54e140,owner+0x19c0+(pose_count-1)*148,(primary,query,hit,reset))
    elif kind==1:
        expected_call=(0x54e000,primary,(query,hit,reset)) if part==-1 else (0x54daa0,primary,(part,query,hit,reset))
    trace.clear();before=bytes(u.mem_read(model,0x6800))
    args=(model,query,hit,reset) if wrapper else (model,part,query,hit,reset)
    u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x5031f0 if wrapper else 0x503120,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
    if expected_call:
        assert trace==[expected_call] and u.reg_read(UC_X86_REG_EAX)==return_word
        assert bytes(u.mem_read(hit,32))==fixture;calls[hex(expected_call[0])]+=1
    else:
        assert not trace and u.reg_read(UC_X86_REG_EAX)&255==0
        assert bytes(u.mem_read(hit,32))==expected_input;rejections+=1
    assert bytes(u.mem_read(model,0x6800))==before,'model, pose, parts or query mutated'
    assert bytes(u.mem_read(query,84))==query_data
    wrappers+=wrapper
    commands.append(w(kind,part,pose_count,reset,return_word,int(wrapper))+query_data+seed+fixture)
    expected_native_args=None;call_trace=bytes(56)
    if expected_call:
        op=2 if kind==2 else (0 if part==-1 else 1)
        selected=pose_count-1 if kind==2 else 0xffffffff
        forwarded_part=-1 if kind==2 else part
        expected_native_args=(0,op,primary,poses+selected*148 if kind==2 else 0,forwarded_part&0xffffffff,query,hit,reset)
        call_trace=w(1,op,selected,forwarded_part,reset,1)+expected_input
    want=w(return_word if expected_call else 0)+call_trace+(fixture if expected_call else expected_input)
    answers.append(want)
    x.mem_write(model,w(kind,primary,poses,pose_count));x.mem_write(poses,b'\xa5'*(4*148))
    x.mem_write(backend,w(callback,0));x.mem_write(query,query_data);x.mem_write(hit,seed)
    native_trace.clear();args=(model,query,hit,reset,backend) if wrapper else (model,part,query,hit,reset,backend)
    x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(all_entry if wrapper else entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
    assert native_trace==([expected_native_args] if expected_call else [])
    assert x.reg_read(UC_X86_REG_EAX)==(return_word if expected_call else 0)
    assert bytes(x.mem_read(hit,32))==want[-32:]
    assert bytes(x.mem_read(query,84))==query_data and bytes(x.mem_read(poses,4*148))==b'\xa5'*(4*148)
    assert bytes(x.mem_read(model,16))==w(kind,primary,poses,pose_count)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-query-dispatch'],input=b''.join(commands))
assert actual==b''.join(answers),'PC dispatch mismatch' 
report=dict(result='PASS',cases=4096,wrapper_cases=wrappers,reset_cases=resets,rejections=rejections,geometry_calls=calls,original_sha256=sha,
    scope='Original503120 and5031f0, only geometry54e000/54daa0/54e140 supplied. Callee ECX/stack ABI, final-pose selection, part forwarding, full callback return, low-byte rejection, reset-before-callback, untouched model/pose/part/query bytes and hit fields verified. Type3 intentionally returns no hit after reading part metadata. Shared PC/NXDK callback arguments, pose selection, returns and all hit bytes match; type3 inert metadata reads are omitted. No geometry implementation or XEMU integration is claimed.')
(root/'artifacts/model-query-dispatch.json').write_text(json.dumps(report,indent=2));print(report)
