"""Archive-backed clutter name catalogs, PC and compiled NXDK ownership."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x800000);O=B+0x1000;F=B+0x2000;A=B+0x100000;S=B+0x700000;STOP=S+0x1000
sym=(ROOT/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',sym)[1],16)
entry,close,find,read,malloc,free=map(symbol,('rf_clutter_catalogs_open','rf_clutter_catalogs_close','rf_vpp_find','rf_vpp_read','malloc','free'))
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
def text(a):
    out=bytearray()
    while x.mem_read(a,1)!=b'\0':out+=x.mem_read(a,1);a+=1
    return bytes(out)
files=['emitters.tbl','effects.tbl','vclip.tbl'];inventory=json.loads((ROOT/'artifacts/inventory.json').read_text());original={}
for name in files:
    row=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==name)
    with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:f.seek(row['offset']);original[name]=f.read(row['size'])
data=original;allocations=reads=0;fail_alloc=fail_read=0;live={};allocation_sizes=[]
def hook(cpu,address,length,context):
    global allocations,reads
    if address not in (find,read,malloc,free):return
    sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);result=0
    if address==find:
        assert arg(0)==B;name=text(arg(1)).decode();assert name in data
        cpu.mem_write(arg(2),name.encode().ljust(64,b'\0')+w(files.index(name),len(data[name])))
    elif address==read:
        assert arg(0)==B and arg(2)==0;name=text(arg(1)).decode();assert arg(4)==len(data[name]);reads+=1
        if reads==fail_read:result=0xffffffff
        else:cpu.mem_write(arg(3),data[name] or b'\0')
    elif address==malloc:
        allocations+=1;n=arg(0);assert 0<n<0x100000;allocation_sizes.append(n)
        if allocations!=fail_alloc:
            result=A+(allocations-1)*0x100000;assert result not in live;live[result]=n;cpu.mem_write(result,b'\xa5'*n)
    elif arg(0):
        assert arg(0) in live;n=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*n)
    cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for address in (find,read,malloc,free):
    x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
def call(function,*args):
    x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(function,STOP,count=100000000)
    assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def declarations(raw):
    s=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'))
    return [v.encode('cp1252') for v in re.findall(r'(?im)^\s*\$Name:\s*"([^"]*)"',s)]
tests=0
def check(budget,status_expected=0,pc=False):
    global allocations,reads,tests,allocation_sizes
    assert not live;allocations=reads=0;allocation_sizes=[];x.mem_write(O,bytes(36))
    status=call(entry,B,F,budget,O);assert status==status_expected,(hex(status),hex(status_expected))
    if status:
        assert not live and bytes(x.mem_read(O,36))==bytes(36);wanted=w(status,0,0,0,0)
    else:
        lists=[declarations(data[name]) for name in files];counts=[len(v) for v in lists]
        retained=36+(counts[0]+counts[1]+64)*4+sum(len(n)+1 for v in lists for n in v)
        peak=retained+max(map(len,data.values()))
        assert allocations==2 and reads==6 and allocation_sizes==[peak-retained,retained-36]
        assert live=={A+0x100000:retained-36} and r(O+24)==F and r(O+28)==retained and r(O+32)==peak
        wanted=w(0,retained,peak,counts[0],counts[1])
        assert r(O+8)==counts[0] and r(O+16)==counts[1]
        lists[2]+=[None]*(64-counts[2])
        for k,names in enumerate(lists):
            pointer=r(O+(4,12,20)[k]);assert not names or A+0x100000<=pointer<A+0x100000+retained-36
            for i,name in enumerate(names):
                ptr=r(pointer+4*i)
                if name is None:assert ptr==0;wanted+=w(0,0)
                else:
                    assert A+0x100000<=ptr<A+0x100000+retained-36 and text(ptr)==name
                    wanted+=w(1,len(name))+name
    if pc:
        result=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--clutter-catalogs',str(ROOT/'Installed_Game/tables.vpp'),str(budget)])
        assert result==wanted,'PC/NXDK'
    call(close,O);call(close,O);assert not live and bytes(x.mem_read(O,36))==bytes(36);tests+=1
    return tuple(struct.unpack_from('<II',wanted,4)) if not status else None
retained,peak=check(1024*1024,pc=True);check(peak,pc=True);check(peak-1,0xfffffffc,pc=True)
for fail_alloc in (1,2):check(peak,0xffffffff)
fail_alloc=0
for fail_read in range(1,7):check(peak,0xffffffff)
fail_read=0
data={'emitters.tbl':b'#particle emitter types\n#End','effects.tbl':b'#Glares\n#End','vclip.tbl':b'#Vclips\n#End'}
check(4096)
data=dict(original);data['effects.tbl']=b'#Glares\n$Name: "'+b'g'*64+b'"\n#End';check(1024*1024,0xfffffffc)
data=dict(original);data['effects.tbl']=b'#Glares\n'+b'$Name: "g"\n'*65+b'#End';check(1024*1024,0xfffffffc)
data=dict(original);data['effects.tbl']=b'#Glares\n$Name: "g"';check(1024*1024,0xfffffffe)
report=dict(result='PASS',cases=tests,retained_bytes=retained,peak_bytes=peak,counts={name:len(declarations(original[name])) for name in files},
 scope='PC archive-backed catalog loader and compiled NXDK with supplied archive read/find and allocator boundaries. Exact names/order, unused vclip slots, budgets, input scratch retirement, allocation/read failures and malformed/empty inputs. No full original table parser, effect loading or native XEMU proof.')
(ROOT/'artifacts/clutter-catalogs.json').write_text(json.dumps(report,indent=2));print(report)
