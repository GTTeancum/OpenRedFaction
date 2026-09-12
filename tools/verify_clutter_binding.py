"""Factory class bindings versus original resource lookup, PC and compiled NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
B=0x30000000;N=B+0x90000;F=B+0x91000;D=B+0xa0000;O=B+0xb0000;IDS=B+0xc0000;Q=B+0xd0000;OBJ=Q+0x100;S=B+0xe0000;STOP=B+0xf0000
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
word=lambda b,o:struct.unpack_from('<I',b,o)[0]
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();cpu=Uc(UC_ARCH_X86,UC_MODE_32)
    cpu.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);cpu.mem_write(p.OPTIONAL_HEADER.ImageBase,im);cpu.mem_map(B,0x200000);return cpu
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(ROOT/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_clutter_definition_bind\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
probe=str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe')
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
def table(name):
    row=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==name)
    with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:f.seek(row['offset']);raw=f.read(row['size'])
    clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'));return raw,clean
def run(cpu,address,*args):
    cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cpu.emu_start(address,STOP,count=1000000)
    assert cpu.reg_read(UC_X86_REG_EIP)==STOP;return cpu.reg_read(UC_X86_REG_EAX)
def catalog(en,gn,vn,fn):
    global wire,expected
    assert len(en)<=256 and len(gn)<=64 and len(vn)<=64 and len(fn)<=640
    vn=vn+['']*(64-len(vn));wire=bytearray(w(len(en),len(gn),len(fn)));expected=bytearray()
    for k,names in enumerate((en,gn,vn)):
        pointers=[]
        for i,name in enumerate(names):
            a=B+0x1000+k*0x10000+i*64;pointers.append(a);raw=name.encode();assert len(raw)<64
            for cpu in (u,x):cpu.mem_write(a,raw+b'\0')
            address=(0x7b2870+i*8) if k==0 else (0x5c9e98+i*52) if k==1 else (0x858cb8+i*224)
            u.mem_write(address,w(len(raw),a));wire.extend(raw.ljust(64,b'\0'))
        x.mem_write(B+0x50000+k*0x1000,w(*pointers) or b'\0')
    u.mem_write(0x7bd99c,w(len(en)));u.mem_write(0x5cab98,w(len(gn)));u.mem_write(0x636ef8,w(len(fn)))
    for i,name in enumerate(fn):
        raw=name.encode().ljust(32,b'\0');assert len(raw)==32
        u.mem_write(0x6300f8+i*44,raw+bytes(12));x.mem_write(B+0x80000+i*44,raw+bytes(12));wire.extend(raw)
    x.mem_write(F,w(B+0x80000,0,len(fn),0,0,0));x.mem_write(N,w(B+0x50000,len(en),B+0x51000,len(gn),B+0x52000,F))
def lookup(address,name,material=False):
    u.mem_write(Q,name+b'\0');u.mem_write(OBJ,w(len(name),Q));return run(u,address,OBJ if material else Q)
cases=0
def check(d,capacity=16,error=False):
    global cases
    assert len(d)==1572;x.mem_write(D,d);x.mem_write(O,b'\xa5'*24);x.mem_write(IDS,b'\xa5'*64)
    status=run(x,entry,D,N,IDS,capacity,O)
    actual=bytes(x.mem_read(O,24));ids=bytes(x.mem_read(IDS,64))
    if error:
        assert status!=0 and actual==b'\xa5'*24 and ids==b'\xa5'*64
        result=w(status)+actual+ids
    else:
        assert status==0;count=word(d,1536);present=word(d,1568)
        name=lambda index:d[index*64:(index+1)*64].split(b'\0')[0]
        values=[lookup(0x4686c0,name(3),True)]+[lookup(address,name(4+i)) if present&(1<<i) else 0xffffffff for i,address in enumerate((0x434cb0,0x4c1d00,0x415430,0x415430))]
        wanted_ids=w(*(lookup(0x497550,name(8+i)) for i in range(count)))+b'\xa5'*(64-count*4)
        assert actual==w(*values,IDS if count else 0) and ids==wanted_ids,(cases,values,actual.hex())
        result=w(0,*values,int(count!=0))+ids
    wire.extend(d+w(capacity));expected.extend(result);cases+=1
def compare_pc():assert subprocess.check_output([probe,'--clutter-bind'],input=wire)==expected
names=lambda s:re.findall(r'(?im)^\s*\$Name:\s*"([^"]*)"',s)
en=names(table('emitters.tbl')[1]);gn=names(re.search(r'(?is)#Glares\s*(.*?)#End',table('effects.tbl')[1])[1]);vn=names(table('vclip.tbl')[1]);fn=names(table('foley.tbl')[1])
catalog(en,gn,vn,fn);raw,clean=table('clutter.tbl');path=ROOT/'artifacts/clutter-binding-input.tbl';path.write_bytes(raw)
class_names=list(dict.fromkeys(n.upper() for n in re.findall(r'(?im)^\s*\$Class Name:\s*"([^"]*)"',clean)))
for name in class_names:
    parsed=subprocess.check_output([probe,'--clutter-definition',str(path),name]);assert parsed[:4]==w(0)
    check(parsed[4:])
compare_pc();authored=cases
catalog(['','Emitter','EMITTER'],['','Glare','GLARE'],['Effect','EFFECT'],['Sound','SOUND'])
for present in range(16):
    for variant in ('','missing','mixed'):
        d=bytearray(1572);d[192:198]=b'metal\0';d[1536:1540]=w(3);d[1568:1572]=w(present)
        for index,value in ((4,'sound'),(5,'effect'),(6,'Glare'),(7,'glare'),(8,'emitter'),(9,'missing'),(10,'')):
            s=(value if variant=='mixed' else variant).encode()+b'\0';d[index*64:index*64+len(s)]=s
        check(bytes(d))
check(bytes(d),capacity=2,error=True)
bad=d[:];bad[1568:1572]=w(16);check(bytes(bad),error=True)
bad=d[:];bad[192:256]=b'm'*64;check(bytes(bad),error=True)
compare_pc()
for offset in (0,8,16,20):
    saved=bytes(x.mem_read(N,24));x.mem_write(N+offset,w(0));x.mem_write(O,b'\xa5'*24);x.mem_write(IDS,b'\xa5'*64);x.mem_write(D,bytes(d))
    assert run(x,entry,D,N,IDS,16,O)==0xfffffffc
    assert bytes(x.mem_read(O,24))==b'\xa5'*24 and bytes(x.mem_read(IDS,64))==b'\xa5'*64;x.mem_write(N,saved)
report=dict(result='PASS',authored_classes=authored,synthetic_cases=cases-authored,nxdk_missing_catalog_cases=4,
    scope='Binding composition versus actual original4686c0/434cb0/4c1d00/415430/497550 with supplied authored catalogs; PC and compiled NXDK output matches. Presence/empty/case/duplicate/missing names and transactional errors. No full original class parser, resource loading or native XEMU/live binding claim.')
(ROOT/'artifacts/clutter-binding.json').write_text(json.dumps(report,indent=2));print(report)
