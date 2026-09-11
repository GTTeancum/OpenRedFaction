"""Real original burn->emitter release/detachment vs shared PC and NXDK."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(i&0xffffffff for i in v))
b=0x30000000;stack=b+0xe0000;stop=stack+0x1000
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,1024*1024);return m
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');voice=cleared=0xffffffff
slots=b+0x10000;records=b+0x20000;lists=b+0x60000;particles=b+0x61000;pool=particles+64;backend=pool+64
symbols=(root/'build/xbox/main.map').read_text()
def xcall(name,*args):
    entry=int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
    x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000000)
    assert x.reg_read(UC_X86_REG_EIP)==stop;return x.reg_read(UC_X86_REG_EAX)
def hook(m,address,size,data):
    global voice,cleared
    if address not in (0x505a40,b+0x70000,b+0x70010):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<3I',m.mem_read(sp,12))
    if address==0x505a40:voice=a[1]
    elif address==b+0x70000:voice=a[2]
    else:cleared=a[2]
    m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook,begin=0x505a40,end=0x505a40);x.hook_add(UC_HOOK_CODE,hook,begin=b+0x70000,end=b+0x70010)
def bad(m,access,address,size,value,data):
    print('INVALID',hex(m.reg_read(UC_X86_REG_EIP)),hex(address),flush=True);return False
u.hook_add(UC_HOOK_MEM_INVALID,bad)
ep=[0x7b2a70+i*344 for i in range(128)];pn=[0x782748+i*120 for i in range(9)]
free,active,detached=0x7bd840,0x7bd6e8,0x7bd670
emap={**{p:i for i,p in enumerate(ep)},free:128,active:129}
pmap={**{p:500+i for i,p in enumerate(pn)},detached:1604,**{p+0xb0:1605+i for i,p in enumerate(ep[:4])}}
def chain(nodes,offset=0):
    for i,p in enumerate(nodes):u.mem_write(p+offset,w(nodes[(i+1)%len(nodes)],nodes[i-1]))
commands=[];expected=[]
for mask,rotation,mode in itertools.product(range(16),range(4),(0,1,256,257)):
    v=(0xffffffff,0,12345)[(mask+rotation)%3];wire=w(mask,rotation,mode,v);commands.append(wire)
    burn=bytearray(524)
    for i in range(8):struct.pack_into('<I',burn,i*64+36,0xffffffff);struct.pack_into('<2I',burn,i*64+56,2 if i==7 else i+2,8 if i==1 else i)
    struct.pack_into('<2I',burn,56,1,1);struct.pack_into('<I',burn,36,v);struct.pack_into('<I',burn,16,123)
    struct.pack_into('<f',burn,40,.75);struct.pack_into('<f',burn,48,7);struct.pack_into('<3I',burn,512,2,1,0xffffffff)
    tokens=[1+(i+rotation)%4 if mask&(1<<i) else 0 for i in range(4)];burn[:16]=w(*tokens)
    raw=bytearray(burn[:512])
    for i in range(8):
        for off in (56,60):n=struct.unpack_from('<I',raw,i*64+off)[0];struct.pack_into('<I',raw,i*64+off,b+(n-1)*64)
    raw[:16]=w(*[ep[t-1] if t else 0 for t in tokens]);u.mem_write(b,bytes(raw))
    u.mem_write(0x62f768,w(-1,b+64,b));u.mem_write(0x5cb2ec,w(0x5cb060));u.mem_write(0x5cae44,w(0x5cabb8));u.mem_write(0x7bd998,w(4));u.mem_write(0x7a3bf8,w(0));u.mem_write(0x7a3cf4,w(9))
    chain([free]+ep[4:],0x148);chain([active]+ep[:4],0x148)
    for i,p in enumerate(ep):u.mem_write(p+0x140,w(0xa5a50001+i if i<4 else 0))
    assert xcall('rf_particle_pool_init',particles,records,lists,133)==0
    assert xcall('rf_emitter_pool_init',pool,slots,particles)==0
    x.mem_write(b,bytes(burn));x.mem_write(pool+8,w(4,127,0,3,4));x.mem_write(slots+4*228+220,w(128))
    x.mem_write(lists+8,w(509,1599));x.mem_write(records+509*120+4,w(1601));x.mem_write(particles+12,w(0,9))
    for i in range(9):
        payload=bytearray(120);struct.pack_into('<I',payload,8,0xffffffff);payload[80]=1;struct.pack_into('<I',payload,88,1);struct.pack_into('<2I',payload,100,1,i//2+1 if i<8 else 0)
        x.mem_write(records+(500+i)*120,bytes(payload));struct.pack_into('<I',payload,104,ep[i//2] if i<8 else 0);u.mem_write(pn[i],bytes(payload))
    chain([detached,pn[8]]);x.mem_write(lists+32,w(508,508));x.mem_write(records+508*120,w(1604,1604))
    for i in range(4):
        chain([ep[i]+0xb0]+pn[i*2:i*2+2]);x.mem_write(lists+(5+i)*8,w(500+i*2,501+i*2))
        x.mem_write(records+(500+i*2)*120,w(501+i*2,1605+i));x.mem_write(records+(501+i*2)*120,w(1605+i,500+i*2))
        x.mem_write(slots+i*228+172,w(0xa5a50001+i));x.mem_write(slots+i*228+216,w(i+1 if i<3 else 129,i-1 if i else 129,1))
    voice=0xffffffff;u.mem_write(stack,w(stop,b,mode));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x42ed20,stop,count=10000000);assert u.reg_read(UC_X86_REG_EIP)==stop
    after=bytearray(u.mem_read(b,512))
    for i in range(8):
        for off in (56,60):ptr=struct.unpack_from('<I',after,i*64+off)[0];struct.pack_into('<I',after,i*64+off,(ptr-b)//64+1 if ptr else 0)
    fh,ah=struct.unpack('<2I',u.mem_read(0x62f76c,8));after+=w((fh-b)//64+1 if fh else 0,(ah-b)//64+1 if ah else 0,-1)
    released={t-1 for t in tokens if t};slotout=b''
    for i,p in enumerate(ep):n,pr=struct.unpack('<2I',u.mem_read(p+0x148,8));slotout+=w(emap[n],emap[pr],int(i<4 and i not in released))+bytes(u.mem_read(p+0x140,4))
    heads=b''
    for p in (free,active):heads+=w(*[emap[n] for n in struct.unpack('<2I',u.mem_read(p+0x148,8))])
    pout=b''
    for p in pn:
        data=bytearray(u.mem_read(p,120));n,pr=struct.unpack_from('<2I',data);data[:8]=w(pmap[n],pmap[pr]);ptr=struct.unpack_from('<I',data,104)[0]
        # Normalize attached emitter pointers; detached particles have zero.
        struct.pack_into('<I',data,104,ep.index(ptr)+1 if ptr else 0)
        pout+=bytes(data)
    lout=b''
    for p in [detached]+[e+0xb0 for e in ep[:4]]:lout+=w(*[pmap[n] for n in struct.unpack('<2I',u.mem_read(p,8))])
    assert bytes(u.mem_read(0x7bd998,4))==w(4-len(released))
    counts=bytes(u.mem_read(0x7a3bf8,4))+bytes(u.mem_read(0x7a3cf4,4));assert counts==w(0,9)
    want=w(0)+bytes(after)+slotout+heads+bytes(u.mem_read(0x7bd998,4))+pout+lout+counts+w(voice,1);expected.append(want)
    voice=cleared=0xffffffff;x.mem_write(backend,w(b+0x70000,b+0x70010,0));status=xcall('rf_burn_release_resolved',b,1,mode,pool,backend)
    got=w(status)+bytes(x.mem_read(b,524))+b''.join(bytes(x.mem_read(slots+i*228+216,12))+bytes(x.mem_read(slots+i*228+172,4)) for i in range(128))+bytes(x.mem_read(pool+8,20))+bytes(x.mem_read(records+500*120,1080))+bytes(x.mem_read(lists+32,40))+bytes(x.mem_read(particles+12,8))+w(voice,cleared)
    assert got==want,('NXDK',mask,rotation,mode,[(i,a,c) for i,(a,c) in enumerate(zip(got,want)) if a!=c][:20])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--burn-resource-release'],input=b''.join(commands))
assert actual==b''.join(expected),('PC',[(i,a,c) for i,(a,c) in enumerate(zip(actual,b''.join(expected))) if a!=c][:20])
report=dict(result='PASS',cases=len(commands),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original42ed20/4973d0/497d80/497230 vs PC/NXDK burn resource adapter. All emitter subsets, rotations and reset low-byte modes. Nine existing particles, ordered detachment, active/free slot lists, enable bytes, burn rings and unchanged particle live counts. Audio stop supplied, original owner lists empty; shared owner-clear callback observed. No actual audio or NPC lifecycle.')
(root/'artifacts/burn-resource-release.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
