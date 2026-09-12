"""Clutter metadata versus independent declarations, PC and compiled NXDK.

This does not execute the original full class parser or resource factory.
"""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP

inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
archive=next(a for a in inventory['files'] if a['path']=='tables.vpp')
entry=next(e for e in archive['vpp']['entries'] if e['name'].lower()=='clutter.tbl')
with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:
    f.seek(entry['offset']);raw=f.read(entry['size'])
path=ROOT/'artifacts/clutter-assets-input.tbl'
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));mapped=p.get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(mapped)+4095)//4096*4096)
u.mem_write(p.OPTIONAL_HEADER.ImageBase,mapped);base=0x30000000;u.mem_map(base,0x400000)
function=int(re.search(r'\s_rf_clutter_assets_read\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
probe=int(re.search(r'\s__chkstk\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
def stack_probe(cpu,address,length,context):
    sp=cpu.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',cpu.mem_read(sp,4))[0]
    cpu.reg_write(UC_X86_REG_ESP,sp+4-cpu.reg_read(UC_X86_REG_EAX));cpu.reg_write(UC_X86_REG_EIP,ret)
# Supply only stack growth on the already mapped fixture stack.
u.hook_add(UC_HOOK_CODE,stack_probe,begin=probe,end=probe)
textptr,nameptr,skinptr,out,stack,stop=[base+x for x in (0,0x200000,0x201000,0x202000,0x300000,0x301000)]
size=64+4096+4;cases=0
def check(data,name,skin,expected=None):
    global cases
    path.write_bytes(data)
    pc=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--clutter-assets',str(path),name,skin])
    u.mem_write(textptr,data);u.mem_write(nameptr,name.encode()+b'\0');u.mem_write(skinptr,skin.encode()+b'\0')
    u.mem_write(out,b'\xa5'*size)
    u.mem_write(stack,struct.pack('<6I',stop,textptr,len(data),nameptr,skinptr,out));u.reg_write(UC_X86_REG_ESP,stack)
    try:u.emu_start(function,stop,count=100000000)
    except Exception:
        print('NXDK stopped at',hex(u.reg_read(UC_X86_REG_EIP)));raise
    assert u.reg_read(UC_X86_REG_EIP)==stop
    xbox=struct.pack('<I',u.reg_read(UC_X86_REG_EAX))+bytes(u.mem_read(out,size))
    assert pc==xbox,(name,skin,'PC/NXDK mismatch')
    status=struct.unpack_from('<i',pc)[0]
    if expected is None:
        assert status and pc[4:]==b'\xa5'*size,(name,skin,status)
    else:
        model,textures=expected
        pad=lambda s:s.encode('cp1252').ljust(64,b'\0')
        wanted=pad(model)+b''.join(map(pad,textures))+b'\0'*(64*(64-len(textures)))+struct.pack('<I',len(textures))
        assert status==0 and pc[4:]==wanted,(name,skin,status)
    cases+=1

text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'))
names=list(re.finditer(r'(?im)^\s*\$Class\s+Name:\s*"([^"]+)"',text))
# Execute original lookup410b60, including500190/57c130, with authored names
# supplied as class string storage. Class parsing/allocation is not supplied evidence.
original=ROOT/'Installed_Game/RF.exe'
assert hashlib.sha256(original.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
op=pefile.PE(str(original));oi=op.get_memory_mapped_image()
o=Uc(UC_ARCH_X86,UC_MODE_32);o.mem_map(op.OPTIONAL_HEADER.ImageBase,(len(oi)+4095)//4096*4096)
o.mem_write(op.OPTIONAL_HEADER.ImageBase,oi);o.mem_map(base,0x400000)
o.mem_write(0x5c97dc,struct.pack('<I',len(names)));first={}
for i,m in enumerate(names):
    value=m[1].encode('cp1252');address=base+i*256
    o.mem_write(address,value+b'\0');o.mem_write(0x5afb88+i*232,struct.pack('<2I',len(value),address))
    first.setdefault(m[1].upper(),i)
for name,index in list(first.items())+[('missing-clutter-fixture',-1)]:
    o.mem_write(nameptr,name.encode()+b'\0');o.mem_write(stack,struct.pack('<2I',stop,nameptr))
    o.reg_write(UC_X86_REG_ESP,stack);o.emu_start(0x410b60,stop,count=10000000)
    assert o.reg_read(UC_X86_REG_EIP)==stop and o.reg_read(UC_X86_REG_EAX)==(index&0xffffffff),(name,index)
seen=set();duplicates=[]
for i,m in enumerate(names):
    key=m[1].upper()
    if key in seen:
        duplicates.append(m[1]);continue
    seen.add(key)
    block=text[m.end():names[i+1].start() if i+1<len(names) else len(text)]
    model=re.search(r'(?i)\$V3D\s+Filename:\s*"([^"]*)"',block)[1]
    skins={}
    for s in re.finditer(r'(?i)\$Skin:\s*"([^"]+)"\s*\(([^)]*)\)',block):
        skins.setdefault(s[1].upper(),re.findall(r'"([^"]*)"',s[2]))
    check(raw,m[1].upper(),'',(model,[]))
    for skin,textures in skins.items():check(raw,m[1].upper(),skin,(model,textures))
authored=cases
synthetic=b'$Class Name: "test" $V3D Filename: "mesh.v3d" $Corpse Class Name: "other" $Skin: "a" ("first.tga") $Skin: "A" ("second.tga") $Class Name: "next" $V3D Filename: "wrong.v3d"'
check(synthetic,'TEST','a',('mesh.v3d',['first.tga']))
check(synthetic,'test','',('mesh.v3d',[]))
check(synthetic,'test','missing');check(synthetic,'missing','')
check(b'$Class Name: "test" $Skin: "a" ("unterminated','test','a')
check(b'$Class Name: "test" $Skin: "a" ('+b'"x" '*65+b')','test','a')
result=dict(result='PASS',declarations=len(names),selected_classes=len(seen),shadowed_classes=duplicates,authored_selections=authored,guard_and_order_cases=cases-authored,
    original_lookup_cases=len(first)+1,targets=['PC','compiled NXDK in Unicorn'],scope='Original410b60 lookup with supplied authored names; bounded model/replacement metadata; no original full parser, factory, glare resolution, or native XEMU integration claim')
(ROOT/'artifacts/clutter-assets-verification.json').write_text(json.dumps(result,indent=2));print(result)
