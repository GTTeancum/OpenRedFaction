"""Original flag parser513020 versus PC and compiled NXDK weapon flags."""
import hashlib,json,re,struct,subprocess,sys,random
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
B=0x30000000;P=B;T=B+0x1000;O=B+0x20000;S=B+0x30000;STOP=B+0x31000
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x40000)
    return u,p
exe=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,p=machine(exe);x,_=machine(ROOT/'build/xbox/main.exe')
names=[];secondary=0;table=0;count=0
entry=int(re.search(r'\s_rf_weapon_flags_read\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
probe=ROOT/'artifacts/weapon-flags-input.tbl';cases=0
def check(data,expected=None,error=-2):
    global cases
    probe.write_bytes(data)
    pc=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--weapon-flags',str(probe),str(secondary)])
    x.mem_write(T,data+b'\0'*16);x.mem_write(O,w(0xa5a5a5a5,0x5a5a5a5a));x.mem_write(S,w(STOP,T,len(data),secondary,O,O+4))
    x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=1000000)
    assert x.reg_read(UC_X86_REG_EIP)==STOP
    actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(O,8));assert actual==pc
    if expected is None:
        assert actual==w(error,0xa5a5a5a5,0x5a5a5a5a)
    else:
        u.mem_write(P,bytes(272));u.mem_write(P,w(T,T));u.mem_write(P+0x10c,w(len(data)))
        u.mem_write(T,data+b'\0'*16);u.mem_write(S,w(STOP,table,count));u.reg_write(UC_X86_REG_ECX,P);u.reg_write(UC_X86_REG_ESP,S)
        u.emu_start(0x513020,STOP,count=1000000)
        assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==S+12
        consumed=struct.unpack('<I',u.mem_read(P+4,4))[0]-T
        assert u.reg_read(UC_X86_REG_EAX)==expected and actual==w(0,expected,consumed),(data,actual,expected,consumed)
    cases+=1
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
row=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='weapons.tbl')
with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:f.seek(row['offset']);raw=f.read(row['size']).decode('cp1252')
clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw)
reports=[]
for secondary,table,count in ((0,0x5a24d8,23),(1,0x5a2534,11)):
    names=[p.get_string_at_rva(a-0x400000).decode() for a in struct.unpack('<%dI'%count,p.get_data(table-0x400000,count*4))]
    tag='Flags2' if secondary else 'Flags'
    lists=list(re.finditer(r'(?im)^\s*\$'+tag+r':\s*(\([^)]*\))',clean))
    for m in lists:
        flags=0
        for name in re.findall(r'"([^"]*)"',m[1]):flags|=1<<names.index(name.lower())
        check(m[1].encode(),flags)
    rng=random.Random(table);masks=[0,(1<<count)-1]+[1<<i for i in range(count)]+[rng.getrandbits(count) for _ in range(512)]
    for bits in masks:
        selected=[name.upper() for i,name in enumerate(names) if bits&(1<<i)]
        if selected:selected+=selected[:1]
        data=' // prefix\r\n( '+ ' // separator\r\n'.join('"'+s+'"' for s in selected)+' ) trailing'
        check(data.encode(),bits)
    for data in (b'("unknown")',b'("alt_fire"',b'(alt_fire)',b'"alt_fire"',b'("alt_fire\0")'):
        check(data)
    reports.append(dict(secondary=secondary,table=hex(table),names=names,authored_lists=len(lists),valid_masks=len(masks),guards=5))
secondary=2;check(b'()',error=-4)
result=dict(result='PASS',cases=cases,sets=reports,original_sha256=digest,scope='Actual original513020 using4c2dce/4c2e06 weapon vocabularies, unmodified parser/CRT helpers. Exact bits and consumed byte count versus PC/NXDK. Invalid syntax guards exclude original fatal diagnostics. Descriptor initialization/OR and Stop Sound resolution remain separate.')
(ROOT/'artifacts/weapon-flags.json').write_text(json.dumps(result,indent=2));print(result)
