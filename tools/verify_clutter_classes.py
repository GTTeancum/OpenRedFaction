"""Compact clutter class ownership on PC/compiled NXDK; resources are supplied IDs."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
word=lambda b,offset:struct.unpack_from('<I',b,offset)[0]
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x800000);D=B+0x1000;BD=B+0x200000;IDS=B+0x210000;O=B+0x300000;A=B+0x400000;S=B+0x700000;STOP=S+0x1000
sym=(ROOT/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',sym)[1],16)
entry,close,malloc,free=map(symbol,('rf_clutter_classes_open','rf_clutter_classes_close','malloc','free'))
r=lambda a:word(x.mem_read(a,4),0)
live=False;fail=False;allocations=0
def hook(cpu,address,length,context):
    global live,allocations
    if address not in (malloc,free):return
    sp=cpu.reg_read(UC_X86_REG_ESP);value=0
    if address==malloc:
        assert not live;allocations+=1
        assert r(sp+4)==payload
        if not fail:cpu.mem_write(A,b'\xa5'*payload);live=True;value=A
    elif r(sp+4):assert live and r(sp+4)==A;live=False
    cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook)
def call(function,*args):
    x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(function,STOP,count=10000000)
    assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def cstr(a):
    out=bytearray()
    while x.mem_read(a,1)!=b'\0':out+=x.mem_read(a,1);a+=1
    return bytes(out)
exe=str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe');tests=0
def check(definitions,label):
    global payload,fail,allocations,tests
    count=len(definitions);bindings=[];expected_rows=[];namebytes=emitterbytes=0
    for i,d in enumerate(definitions):
        names=[d[k*64:(k+1)*64].split(b'\0')[0] for k in range(3)]
        n=word(d,1536);ids=[-1 if j&1 else i*16+j for j in range(n)]
        resources=[i%10]+[i*16+j if d[(j+4)*64] else -1 for j in range(4)]
        bindings.append((resources,ids));namebytes+=sum(len(s)+1 for s in names);emitterbytes+=n*4
        scalar=w(n,word(d,1548),word(d,1540),word(d,1552),word(d,1556),resources[0],word(d,1544),*resources[1:],0,0,0,0,0,0,-1,word(d,1560),word(d,1564))
        assert len(scalar)==80
        expected_rows.append(scalar+b''.join(w(len(s))+s for s in names)+w(*ids))
    payload=count*96+namebytes+emitterbytes;budget=16+payload
    data=b''.join(definitions);bindingwire=b''.join(w(*a)+w(*b) for a,b in bindings)
    for trial in ('exact','short','allocation-failure'):
        x.mem_write(D,data or b'\0');x.mem_write(O,bytes(16));allocations=0;fail=trial=='allocation-failure'
        for i,(fields,ids) in enumerate(bindings):
            x.mem_write(BD+i*24,w(*fields,IDS+i*64));x.mem_write(IDS+i*64,w(*ids) or b'\0')
        limit=budget-(trial=='short');status=call(entry,D,BD,count,limit,O)
        expected_status=0xfffffffc if trial=='short' else 0xffffffff if fail and count else 0
        assert status==expected_status,(label,trial,hex(status));assert allocations==(0 if trial=='short' or not count else 1)
        if status:assert bytes(x.mem_read(O,16))==bytes(16) and not live
        else:
            assert r(O+12)==budget and r(O+8)==count
            x.mem_write(D,b'\xcc'*len(data) or b'\0');x.mem_write(BD,b'\xcc'*(count*24) or b'\0');x.mem_write(IDS,b'\xcc'*(count*64) or b'\0')
            rows=[]
            for i in range(count):
                c=r(O+4)+i*96;assert A<=c<A+payload
                names=[cstr(r(c+k*4)) for k in range(3)]
                for k in range(3):assert A<=r(c+k*4)<A+payload
                ids=bytes(x.mem_read(r(c+12),r(c+16)*4)) if r(c+16) else b''
                rows.append(bytes(x.mem_read(c+16,80))+b''.join(w(len(s))+s for s in names)+ids)
            assert rows==expected_rows,(label,'owned content')
        if trial!='allocation-failure':
            pc=subprocess.check_output([exe,'--clutter-classes'],input=w(count,limit)+data+bindingwire)
            wanted=w(status,0,0) if status else w(0,budget,count)+b''.join(expected_rows)
            assert pc==wanted,(label,trial,'PC')
        call(close,O);call(close,O);assert not live and bytes(x.mem_read(O,16))==bytes(16);tests+=1
    return budget

inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
entrydata=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='clutter.tbl')
with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entrydata['offset']);raw=f.read(entrydata['size'])
path=ROOT/'artifacts/clutter-class-owner-input.tbl';path.write_bytes(raw)
clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'))
names=list(dict.fromkeys(n.upper() for n in re.findall(r'(?im)^\s*\$Class Name:\s*"([^"]+)"',clean)))
defs=[]
for name in names:
    result=subprocess.check_output([exe,'--clutter-definition',str(path),name]);assert result[:4]==w(0) and len(result)==1572
    defs.append(result[4:])
budget=check(defs,'installed classes')
check([],'empty');check([defs[0],defs[0]],'duplicate class order')
for label in ('emitter-storage','model-termination','material','life','flags','busy-owner','empty-name','model-kind'):
    fail=False;allocations=0;x.mem_write(D,defs[0]);x.mem_write(BD,w(2,-1,-1,-1,-1,IDS));x.mem_write(O,bytes(16))
    if label=='emitter-storage':x.mem_write(D+1536,w(1));x.mem_write(BD+20,w(0))
    elif label=='model-termination':x.mem_write(D+64,b'm'*64)
    elif label=='material':x.mem_write(BD,w(256))
    elif label=='life':x.mem_write(D+1552,w(0x7fc00000))
    elif label=='flags':x.mem_write(D+1544,w(512))
    elif label=='busy-owner':x.mem_write(O,w(B))
    elif label=='empty-name':x.mem_write(D,b'\0')
    else:x.mem_write(D+1540,w(2))
    before=bytes(x.mem_read(O,16));assert call(entry,D,BD,1,10000,O)==0xfffffffc,label
    assert not allocations and not live and bytes(x.mem_read(O,16))==before,label
report=dict(result='PASS',classes=len(defs),metadata_bytes=len(defs)*1568,owned_bytes=budget,cases=tests,nxdk_guard_cases=8,
 scope='PC and compiled NXDK compact ownership, exact/short budgets, allocation failure, input destruction, duplicate order, repeated close. Parsed inputs separately verified; supplied material/effect/emitter IDs, no resource lookup or loading, no original allocator or native XEMU equivalence claim.')
(ROOT/'artifacts/clutter-classes.json').write_text(json.dumps(report,indent=2));print(report)
