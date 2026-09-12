"""Original flag parser513020 versus PC and compiled NXDK clutter flags."""
import hashlib,json,re,struct,subprocess,sys
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
names=[p.get_string_at_rva(a-0x400000).decode() for a in struct.unpack('<9I',p.get_data(0x193f20,36))]
entry=int(re.search(r'\s_rf_clutter_flags_read\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
probe=ROOT/'artifacts/clutter-flags-input.tbl';cases=0
def check(data,expected=None):
    global cases
    probe.write_bytes(data)
    pc=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--clutter-flags',str(probe)])
    x.mem_write(T,data+b'\0'*16);x.mem_write(O,w(0xa5a5a5a5,0x5a5a5a5a));x.mem_write(S,w(STOP,T,len(data),O,O+4))
    x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=1000000)
    assert x.reg_read(UC_X86_REG_EIP)==STOP
    actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(O,8));assert actual==pc
    if expected is None:
        assert actual==w(-2,0xa5a5a5a5,0x5a5a5a5a)
    else:
        u.mem_write(P,bytes(272));u.mem_write(P,w(T,T));u.mem_write(P+0x10c,w(len(data)))
        u.mem_write(T,data+b'\0'*16);u.mem_write(S,w(STOP,0x593f20,9));u.reg_write(UC_X86_REG_ECX,P);u.reg_write(UC_X86_REG_ESP,S)
        u.emu_start(0x513020,STOP,count=1000000)
        assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==S+12
        consumed=struct.unpack('<I',u.mem_read(P+4,4))[0]-T
        assert u.reg_read(UC_X86_REG_EAX)==expected and actual==w(0,expected,consumed),(data,actual,expected,consumed)
    cases+=1
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
row=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='clutter.tbl')
with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:f.seek(row['offset']);raw=f.read(row['size']).decode('cp1252')
clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw)
lists=list(re.finditer(r'(?im)^\s*\$Flags:\s*(\([^)]*\))',clean))
for m in lists:
    flags=0
    for name in re.findall(r'"([^"]*)"',m[1]):flags|=1<<names.index(name.lower())
    check(m[1].encode(),flags)
for bits in range(512):
    selected=[name.upper() for i,name in enumerate(names) if bits&(1<<i)]
    if selected:selected+=selected[:1]
    data=' // prefix\r\n( '+ ' // separator\r\n'.join('"'+s+'"' for s in selected)+' ) trailing'
    check(data.encode(),bits)
for data in (b'("unknown")',b'("clock_seconds.tga")',b'("collectable"',b'(collectable)',b'"collectable"',b'("collectable\0")'):
    check(data)
result=dict(result='PASS',authored_lists=len(lists),all_bit_combinations=512,error_cases=6,names=names,
    original_sha256=digest,scope='Actual original513020 with nine-entry593f20 vocabulary, unmodified parser/CRT helpers, output bits and consumed byte count versus PC/compiled NXDK. Errors are port status checks; original fatal diagnostic is not executed. No full class-parser or live integration claim.')
(ROOT/'artifacts/clutter-flags.json').write_text(json.dumps(result,indent=2));print(result)
