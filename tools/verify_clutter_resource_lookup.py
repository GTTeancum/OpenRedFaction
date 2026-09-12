"""Original material/glare/emitter lookup versus PC and compiled NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
B=0x30000000;N=B+0x1000;Q=B+0x6000;OBJ=B+0x6100;PTRS=B+0x7000;S=B+0xe000;STOP=B+0xf000
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000)
    return u,p
exe=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,p=machine(exe);x,_=machine(ROOT/'build/xbox/main.exe')
sym=(ROOT/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',sym)[1],16)
entries=list(map(symbol,('rf_clutter_material_index','rf_glare_name_lookup','rf_emitter_name_lookup')))
materials=[p.get_string_at_rva(a-0x400000).decode() for a in struct.unpack('<10I',p.get_data(0x19cb10,40))]
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
def table(name):
    row=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==name)
    with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:f.seek(row['offset']);s=f.read(row['size']).decode('cp1252')
    return re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],s)
glares=re.findall(r'(?im)^\s*\$Name:\s*"([^"]*)"',re.search(r'(?is)#Glares\s*(.*?)#End',table('effects.tbl'))[1])
emitters=re.findall(r'(?im)^\s*\$Name:\s*"([^"]*)"',table('emitters.tbl'))
refs=re.findall(r'(?im)^\s*\$(?:Glare|Rod Glare):\s*"([^"]*)"',table('clutter.tbl'))
assert len(glares)<=64
fixtures=[]
for names in [glares]+[emitters[i:i+64] for i in range(0,len(emitters),64)]+[[],[''],['first','DUP','dup',''],['x']*63+['last']]:
    queries=list(dict.fromkeys(names+[n.upper() for n in names]+[n.lower() for n in names]+['','missing','dup','DUP','last']+materials+[n.upper() for n in materials]))
    if names==glares:queries+=refs
    fixtures.extend((names,q) for q in queries)
commands=bytearray();expected=bytearray()
def run(cpu,address,*args):
    cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cpu.emu_start(address,STOP,count=1000000)
    assert cpu.reg_read(UC_X86_REG_EIP)==STOP and cpu.reg_read(UC_X86_REG_ESP)==S+4
    return cpu.reg_read(UC_X86_REG_EAX)
for names,query in fixtures:
    assert len(names)<=64 and len(query)<64
    data=b''.join(n.encode().ljust(64,b'\0') for n in names)
    for cpu in (u,x):cpu.mem_write(N,data or b'\0');cpu.mem_write(Q,query.encode()+b'\0')
    for i,n in enumerate(names):
        u.mem_write(0x5c9e98+i*52,w(len(n),N+i*64));u.mem_write(0x7b2870+i*8,w(len(n),N+i*64))
    u.mem_write(0x5cab98,w(len(names)));u.mem_write(0x7bd99c,w(len(names)));u.mem_write(OBJ,w(len(query),Q))
    x.mem_write(PTRS,w(*(N+i*64 for i in range(len(names)))) or b'\0')
    original=[run(u,0x4686c0,OBJ),run(u,0x415430,Q),run(u,0x497550,Q)]
    shared=[run(x,entries[0],Q),run(x,entries[1],PTRS,len(names),Q),run(x,entries[2],PTRS,len(names),Q)]
    wanted=[next((i for i,n in enumerate(materials) if n.lower()==query.lower()),0),
            next((i for i,n in enumerate(names) if n==query),0xffffffff),
            next((i for i,n in enumerate(names) if n.lower()==query.lower()),0xffffffff)]
    assert original==shared==wanted,(query,original,shared,wanted)
    commands.extend(w(len(names))+data+query.encode().ljust(64,b'\0'));expected.extend(w(*wanted))
pc=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--clutter-resource-lookup'],input=commands)
assert pc==expected
for entry in entries[1:]:
    assert run(x,entry,0,1,Q)==0xffffffff
    assert run(x,entry,PTRS,0xffffffff,Q)==0xffffffff
    assert run(x,entry,PTRS,1,0)==0xffffffff
assert run(x,entries[0],0)==0
missing=sorted(set(q for q in refs if q not in glares));case_only=[q for q in missing if q.lower() in [n.lower() for n in glares]]
report=dict(result='PASS',cases=len(fixtures),nxdk_guards=7,glare_names=len(glares),emitter_names=len(emitters),clutter_glare_references=len(refs),missing_glare_references=missing,case_only_misses=case_only,
    original_sha256=digest,scope='Unmodified original4686c0/415430/497550 and string helpers versus PC/compiled NXDK over supplied authored/synthetic name storage. First match, empty/unknown names, fixed material fallback, exact glare case, insensitive emitter/material names. No resource/table loading or native XEMU proof.')
(ROOT/'artifacts/clutter-resource-lookup.json').write_text(json.dumps(report,indent=2));print(report)
