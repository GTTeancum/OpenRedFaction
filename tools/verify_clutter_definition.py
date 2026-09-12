"""Factory-facing class metadata versus independent installed declarations."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x400000);T=B;N=B+0x200000;O=B+0x201000;S=B+0x300000;STOP=B+0x301000
entry=int(re.search(r'\s_rf_clutter_definition_read\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
SIZE=1572;w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v));path=ROOT/'artifacts/clutter-definition-input.tbl'
flag_names=['collectable','collide_weapon','collide_object','is_screen','shatters','has_alpha','is_switch','can_carry','is_clock']
fields=['Class Name','V3D Filename','Corpse Class Name','Material','Sound','Explode Anim','Glare','Rod Glare']
def expected(block):
    block=re.split(r'(?im)^\s*\$Skin:',block)[0]
    def value(key,default=''):
        m=re.search(r'(?im)^\s*\$'+re.escape(key)+r':\s*([^\r\n]*)',block)
        return m[1].strip() if m else default
    names=[value(k).strip('"') for k in fields]
    emitters=re.findall(r'(?im)^\s*\$Emitter:\s*"([^"]*)"',block)
    flags=0
    for name in re.findall(r'"([^"]*)"',value('Flags')):flags|=1<<flag_names.index(name.lower())
    pad=lambda s:s.encode('cp1252').ljust(64,b'\0')
    out=b''.join(map(pad,names))+b''.join(map(pad,emitters))+b'\0'*64*(16-len(emitters))
    out+=w(len(emitters),3 if names[1].lower().endswith('.vfx') else 1,flags)
    out+=struct.pack('<3f2I',float(value('Emitter Life','-1')),float(value('Life')),float(value('Radius','-1')),int(value('Screen Width','64')),int(value('Screen Height','64')))
    present=sum(1<<i for i,k in enumerate(['Sound','Explode Anim','Glare','Rod Glare']) if re.search(r'(?im)^\s*\$'+re.escape(k)+r':',block))
    out+=w(present);assert len(out)==SIZE;return out
cases=0
def check(data,name,wanted):
    global cases
    path.write_bytes(data);pc=subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--clutter-definition',str(path),name])
    x.mem_write(T,data+b'\0');x.mem_write(N,name.encode()+b'\0');x.mem_write(O,b'\xa5'*SIZE)
    x.mem_write(S,w(STOP,T,len(data),N,O));x.reg_write(UC_X86_REG_ESP,S)
    x.emu_start(entry,STOP,count=100000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
    actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(O,SIZE));assert actual==pc,(name,'PC/NXDK')
    if wanted is None:assert actual[:4]!=w(0) and actual[4:]==b'\xa5'*SIZE,(name,'error')
    else:assert actual==w(0)+wanted,(name,'declarations',[(i,a,b) for i,(a,b) in enumerate(zip(actual[4:],wanted)) if a!=b][:10])
    cases+=1
inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
row=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name'].lower()=='clutter.tbl')
with (ROOT/'Installed_Game/tables.vpp').open('rb') as f:f.seek(row['offset']);raw=f.read(row['size'])
clean=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw.decode('cp1252'))
markers=list(re.finditer(r'(?im)^\s*\$Class Name:\s*"([^"]+)"',clean));seen=set()
for i,m in enumerate(markers):
    name=m[1].upper()
    if name in seen:continue
    seen.add(name);block=clean[m.start():markers[i+1].start() if i+1<len(markers) else len(clean)]
    check(raw,name,expected(block))
base='$Class Name: "test"\n$V3D Filename: "a.VFX"\n$Material: "metal"\n$Life: -1\n$Flags: ("has_alpha")\n'
for suffix in ('','$Rod Glare: "rod"\n$Screen Width: 0\n$Screen Height: 32\n$Radius: 0.125\n$Emitter Life: 2\n',
               '$Glare: "base"\n$Emitter: "one"\n$Emitter: "two"\n$Skin: "a" ("a.tga")\n$Glare: "skin"\n'):
    data=base+suffix;check(data.encode(),'TEST',expected(data))
for data in (base.replace('$Life: -1\n',''),base+'$Life: 2\n',base+'$Radius: nan\n',
             base+'$Emitter: "x"\n'*17,base+'$Sound: "unterminated',base.replace('"metal"','"'+('m'*64)+'"')):
    check(data.encode(),'test',None)
check(base.encode(),'absent',None)
result=dict(result='PASS',selectable_classes=len(seen),declarations=len(markers),synthetic_cases=cases-len(seen),record_bytes=SIZE,
    scope='Owned factory-facing metadata on PC/compiled NXDK versus independent installed declarations and synthetic boundaries. Defaults separately checked by original parser-prefix oracle. No full original class parser, resource resolution, compact runtime owner or native XEMU claim.')
(ROOT/'artifacts/clutter-definition.json').write_text(json.dumps(result,indent=2));print(result)
