"""Complete42eb20 bone fallback sequence with real51d690 search vs PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(0x30000000,65536);return u
u=machine(original);x=machine(root/'build/xbox/main.exe');b=0x30000000;model=b+0x1000;stack=b+0xe000;stop=b+0xf000;thunk=b+0x3000
map_text=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'_rf_burn_resolve_bones\s+([0-9a-fA-F]+)',map_text)[1],16);lookup=int(re.search(r'_rf_model_find_bone_substring\s+([0-9a-fA-F]+)',map_text)[1],16)
query_pointer=0;original_trace=[];compiled_trace=[];return_thunks={}
def string(m,p):return bytes(m.mem_read(p,32)).split(b'\0')[0]
def hook(m,address,size,context):
    global query_pointer
    if address not in (0x4ff3d0,0x42ec80):return
    sp=m.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',m.mem_read(sp,8))
    if address==0x4ff3d0:
        query_pointer=arg;m.reg_write(UC_X86_REG_EAX,m.reg_read(UC_X86_REG_ECX));m.reg_write(UC_X86_REG_ESP,sp+8);m.reg_write(UC_X86_REG_EIP,ret)
    else:
        assert arg==0x12345678;original_trace.append(string(m,query_pointer))
        #51d690 ret4 vs replaced42ec80 cdecl return: restore the caller's stack.
        target_thunk=return_thunks.setdefault(ret,thunk+len(return_thunks)*16)
        m.mem_write(target_thunk,b'\x83\xec\x04\x68'+w(ret)+b'\xc3');m.mem_write(sp,w(target_thunk,query_pointer));m.reg_write(UC_X86_REG_ECX,model);m.reg_write(UC_X86_REG_EIP,0x51d690)
u.hook_add(UC_HOOK_CODE,hook)
def compiled_hook(m,address,size,context):
    if address==lookup:
        sp=m.reg_read(UC_X86_REG_ESP);ptr,length=struct.unpack('<2I',m.mem_read(sp+12,8));compiled_trace.append(bytes(m.mem_read(ptr,length)))
x.hook_add(UC_HOOK_CODE,compiled_hook)
queries=[b'lowerleg-l',b'tech- leg-l-lower',b'lowerleg-r',b'tech- leg-r-lower',b'spine01',b'spine03',b'tech- 1spine',b'tech- 1spine01',b'head'];groups=(queries[:2],queries[2:4],queries[4:8],queries[8:]);cases=[];expected=[];specs=[];traces=[]
for mask in range(512):
 for mode in range(4):
    names=[name for j,name in enumerate(queries) if mask&(1<<j)]
    if mode==1:names=[b'xx_'+name+b'_y' for name in reversed(names)]
    if mode==2:names=[name.upper() for name in names]
    if mode==3:names=[b'xheadx']+names+[b'head']
    u.mem_write(model+0x48,w(len(names)))
    for j,name in enumerate(names):u.mem_write(model+0x4c+j*0x4c,name+b'\0')
    u.mem_write(b,bytes(64));u.mem_write(stack,w(stop,b,0x12345678));u.reg_write(UC_X86_REG_ESP,stack);original_trace=[];u.emu_start(0x42eb20,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop,(mask,mode,hex(u.reg_read(UC_X86_REG_EIP)),original_trace[-12:])
    indices=struct.unpack('<4i',u.mem_read(b+20,16));found=u.reg_read(UC_X86_REG_EAX)&255;want=[];wanted_trace=[]
    for group in groups:
        index=-1
        for query in group:
            wanted_trace.append(query);index=next((j for j,name in enumerate(names) if query in name),-1)
            if index!=-1:break
        want.append(index)
    assert list(indices)==want and found==int(-1 not in want) and original_trace==wanted_trace,(mask,mode,indices,want,found,original_trace,wanted_trace)
    cases.append(w(len(names),0,0)+b''.join(name.ljust(32,b'\0') for name in names).ljust(1536,b'\0')+bytes(32));expected.append(w(0 if found else -3,*indices));specs.append(names);traces.append(original_trace)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--burn-bones'],input=b''.join(cases));assert len(actual)==len(cases)*20
for i,(names,want) in enumerate(zip(specs,expected)):
    assert actual[i*20:(i+1)*20]==want,('PC',i)
    for j,name in enumerate(names):x.mem_write(b+j*8,w(b+0x1000+j*32,len(name)));x.mem_write(b+0x1000+j*32,name+b'\0')
    x.mem_write(b+0x2100,w(-9,-9,-9,-9));x.mem_write(stack,w(stop,b,len(names),b+0x2100));x.reg_write(UC_X86_REG_ESP,stack);compiled_trace=[];x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x2100,16));assert got==want and compiled_trace==traces[i],('NXDK',i,compiled_trace,traces[i])
report=dict(result='PASS',cases=len(cases),original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original42eb20 fallback orchestration with actual51d690/strstr search vs PC/NXDK; all subsets of9 names, reversed prefixed names, uppercase and duplicate head. Exact indices/success and NXDK query order. String construction/model-resolution42ec80 supplied; no allocation, pose evaluation or live binding.')
(root/'artifacts/burn-bones.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
