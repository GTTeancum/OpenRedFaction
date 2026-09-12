"""Full archive-to-runtime clutter class composition, PC and compiled NXDK."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
word=lambda b,o:struct.unpack_from('<I',b,o)[0]
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x800000);N=B+0x90000;F=B+0x91000;O=B+0xa0000;PEAK=O+0x100;A=B+0x200000;S=B+0x700000;STOP=S+0x1000
sym=(ROOT/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',sym)[1],16)
entry,close,find,read,malloc,free,bind=map(symbol,('rf_clutter_classes_load','rf_clutter_classes_close','rf_vpp_find','rf_vpp_read','malloc','free','rf_clutter_definition_bind'))
r=lambda a:word(x.mem_read(a,4),0)
def text(a):
    out=bytearray()
    while x.mem_read(a,1)!=b'\0':out+=x.mem_read(a,1);a+=1
    return bytes(out)
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
def table(name):
    e=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==name)
    with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
    clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'));return raw,clean
names=lambda s:re.findall(r'(?im)^\s*\$Name:\s*"([^"]*)"',s)
en=names(table('emitters.tbl')[1]);gn=names(re.search(r'(?is)#Glares\s*(.*?)#End',table('effects.tbl')[1])[1]);vn=names(table('vclip.tbl')[1]);fn=names(table('foley.tbl')[1])
vn+=['']*(64-len(vn))
for k,rows in enumerate((en,gn,vn)):
    pointers=[]
    for i,name in enumerate(rows):
        a=B+0x1000+k*0x10000+i*64;pointers.append(a);x.mem_write(a,name.encode()+b'\0')
    x.mem_write(B+0x50000+k*0x1000,w(*pointers))
for i,name in enumerate(fn):x.mem_write(B+0x80000+i*44,name.encode().ljust(32,b'\0')+bytes(12))
x.mem_write(F,w(B+0x80000,0,len(fn),0,0,0));x.mem_write(N,w(B+0x50000,len(en),B+0x51000,len(gn),B+0x52000,F))
soundwire=w(len(fn))+b''.join(name.encode().ljust(32,b'\0') for name in fn)
raw,clean=table('clutter.tbl');data=raw;allocs=reads=binds=0;fail_alloc=fail_read=fail_bind=0;live={};sizes=[]
def hook(cpu,address,length,context):
    global allocs,reads,binds
    sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);result=0
    if address==bind:
        binds+=1
        if binds!=fail_bind:return
        result=0xffffffff
    elif address==find:
        assert arg(0)==B and text(arg(1))==b'clutter.tbl'
        cpu.mem_write(arg(2),b'clutter.tbl'.ljust(64,b'\0')+w(0,len(data)))
    elif address==read:
        reads+=1;assert arg(0)==B and arg(2)==0 and arg(4)==len(data)
        if fail_read:result=0xffffffff
        else:cpu.mem_write(arg(3),data)
    elif address==malloc:
        allocs+=1;n=arg(0);assert 0<n<0x100000;sizes.append(n)
        if allocs!=fail_alloc:
            result=A+(allocs-1)*0x100000;assert result not in live;live[result]=n;cpu.mem_write(result,b'\xa5'*n)
    elif address==free and arg(0):
        n=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*n)
    cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for address in (find,read,malloc,free,bind):x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
def call(function,*args):
    x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(function,STOP,count=100000000)
    assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
probe=str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe');path=ROOT/'artifacts/clutter-load-row.tbl'
markers=list(re.finditer(r'(?im)^\s*\$Class Name:\s*"([^"]+)"',clean));expected=[];resident=16+96*len(markers)
materials=['default','rock','metal','flesh','water','lava','solid','sand','ice','glass']
for i,m in enumerate(markers):
    block=clean[m.start():markers[i+1].start() if i+1<len(markers) else len(clean)];path.write_bytes(block.encode('cp1252'))
    result=subprocess.check_output([probe,'--clutter-definition',str(path),m[1]]);assert result[:4]==w(0);d=result[4:];assert len(d)==1572
    string=lambda i:d[i*64:(i+1)*64].split(b'\0')[0]
    lookup=lambda rows,q,exact=False:next((j for j,n in enumerate(rows) if (n if exact else n.lower())==(q if exact else q.lower())),0xffffffff)
    present=word(d,1568);ids=[lookup(en,string(8+j).decode()) for j in range(word(d,1536))]
    fields=[lookup(materials,string(3).decode())];fields[0]=0 if fields[0]==0xffffffff else fields[0]
    for j,rows in enumerate((fn,vn,gn,gn)):
        q=string(4+j).decode();fields.append(lookup(rows,q,j>=2) if present&(1<<j) and (q or j>=2) else 0xffffffff)
    scalar=w(len(ids),word(d,1548),word(d,1540),word(d,1552),word(d,1556),fields[0],word(d,1544),*fields[1:],0,0,0,0,0,0,-1,word(d,1560),word(d,1564));assert len(scalar)==80
    strings=[string(j) for j in range(3)];resident+=sum(len(s)+1 for s in strings)+4*len(ids)
    expected.append(scalar+b''.join(w(len(s))+s for s in strings)+w(*ids))
count=len(expected);scratch=1672+450*8+len(raw);peak=scratch+resident;tests=0
def check(budget,status_expected=0,pc=False,rows=None):
    global allocs,reads,binds,sizes,tests
    assert not live;allocs=reads=binds=0;sizes=[];x.mem_write(O,bytes(16));x.mem_write(PEAK,w(0xa5a5a5a5))
    status=call(entry,B,N,budget,O,PEAK);assert status==status_expected,(hex(status),hex(status_expected),binds)
    if status:
        assert bytes(x.mem_read(O,16))==bytes(16) and r(PEAK)==0xa5a5a5a5 and not live
        wanted=w(status,0,0,0xa5a5a5a5)
    else:
        rows=expected if rows is None else rows
        actual=[]
        for i in range(r(O+8)):
            c=r(O+4)+96*i;strings=[text(r(c+4*j)) for j in range(3)]
            ids=bytes(x.mem_read(r(c+12),r(c+16)*4)) if r(c+16) else b''
            actual.append(bytes(x.mem_read(c+16,80))+b''.join(w(len(s))+s for s in strings)+ids)
        assert actual==rows and binds==2*len(rows)
        assert r(PEAK)==r(O+12)+sizes[0] and sizes[0]==1672+3600+len(data)
        assert len(live)==int(bool(rows)) and reads==1
        wanted=w(0,r(O+12),len(rows),r(PEAK))+b''.join(rows)
    if pc:assert subprocess.check_output([probe,'--clutter-load',str(ROOT/'Installed_Game/tables.vpp'),str(budget)],input=soundwire)==wanted
    call(close,O);call(close,O);assert not live and bytes(x.mem_read(O,16))==bytes(16);tests+=1
check(1024*1024,pc=True);check(peak,pc=True);check(peak-1,0xfffffffc,pc=True)
for fail_alloc in (1,2):check(peak,0xffffffff)
fail_alloc=0;fail_read=1;check(peak,0xffffffff);fail_read=0
for fail_bind in (1,count+2):check(peak,0xffffffff)
fail_bind=0;data=b'#Clutter\n#End';check(65536,rows=[])
data=b'#Clutter\n$Class Name: "x"\n#End';check(65536,0xfffffffe)
data=b'#Clutter\n'+b'$Class Name: "x"\n'*451+b'#End';check(65536,0xfffffffc)
report=dict(result='PASS',cases=tests,authored_classes=count,retained_bytes=resident,peak_bytes=peak,scratch_bytes=scratch,
    scope='PC actual archives/catalog loading versus compiled NXDK with supplied archive I/O/allocator and authored catalogs. Every row and bound ID, duplicate order, one-row scratch, exact/short budgets, allocation/read and both-pass binding failures, malformed/empty/capacity cases. No generic scene objects, full original parser or native XEMU proof.')
(ROOT/'artifacts/clutter-load.json').write_text(json.dumps(report,indent=2));print(report)
