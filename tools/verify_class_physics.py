"""Installed class metadata versus original executable flag-name tables."""
import hashlib,json,re,struct,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image()
def names(address,count):
    values=[]
    for i in range(count):
        p=struct.unpack_from('<I',image,address-0x400000+i*4)[0]-0x400000
        values.append(image[p:p+64].split(bytes(1))[0].decode().lower())
    return values
tables=[names(0x594598,28),names(0x594608,8)]
movement=names(0x596384,16);uses={'vehicle':1,'switch':2,'command':3,'turret':4,'monitor':5,'medic':6,'ai response':9,'play_sound':10}
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for f in inventory['files'] if f['path']=='tables.vpp' for e in f['vpp']['entries'] if e['name']=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:
    f.seek(entry['offset']);text=f.read(entry['size']).decode('cp1252')
text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],text)
blocks=re.split(r'(?i)\$name:\s*"([^"]+)"',text);probe=root/'build/pc/Release/rf_entity_assets_probe.exe';records=[]
for i in range(1,len(blocks),2):
    name,block=blocks[i:i+2]
    mass=float(re.search(r'\$Mass:\s*(\S+)',block,re.I)[1]);material=re.search(r'\$Material:\s*"([^"]+)"',block,re.I)[1]
    flags=[]
    for tag,table in zip(['Flags','Flags2'],tables):
        m=re.search(r'\$'+tag+r':\s*\(([^)]*)\)',block,re.I);value=0
        if m:
            for word in re.findall(r'"([^"]+)"',m[1]):value|=1<<table.index(word.lower())
        flags.append(value)
    mode=movement.index(re.search(r'\$Movemode:\s*"([^"]+)"',block,re.I)[1].lower())
    use=re.search(r'\$Use:\s*"([^"]+)"(?:\s*\+radius:\s*(\S+))?',block,re.I)
    kind=uses.get(use[1].lower(),0) if use else 0;radius=float(use[2]) if kind else 0
    expected=struct.pack('<f64sIIIIf',mass,material.encode(),*flags,mode,kind,radius).hex()
    actual=subprocess.check_output([str(probe),'--class-physics',str(root/'Installed_Game/tables.vpp'),name.upper()],text=True).strip()
    assert actual==expected,name
    records.append(dict(name=name,mass=mass,material=material,flags=flags))
folder=root/'artifacts/class-physics-tests';folder.mkdir(exist_ok=True)
def check(text):
    payload=text.encode();size=4096+((len(payload)+2047)//2048)*2048;data=bytearray(size)
    struct.pack_into('<4I',data,0,0x51890ace,1,1,size);data[2048:2059]=b'entity.tbl\0'
    struct.pack_into('<I',data,2108,len(payload));data[4096:4096+len(payload)]=payload
    path=folder/'fixture.vpp';path.write_bytes(data)
    return subprocess.run([str(probe),'--class-physics',str(path),'actor'],capture_output=True)
valid='$Name: "actor" $Mass: 100 $Material: "flesh" $Flags: ("WALK" "walk") $Movemode: "RUN"'
assert check(valid).stdout.decode().strip()==struct.pack('<f64sIIIIf',100,b'flesh',1,0,1,0,0).hex()
for name,kind in uses.items():
    assert check(valid+' $Use: "'+name.upper()+'" +radius: .5').stdout.decode().strip()==struct.pack('<f64sIIIIf',100,b'flesh',1,0,1,kind,.5).hex()
bad=[valid.replace('100','nan'),valid.replace('100','"100"'),valid.replace('"WALK"','"unknown"'),valid.replace(')', ''),valid.replace('$Mass: 100',''),valid+' $Flags: ()',valid.replace('"flesh"','"'+64*'a'+'"'),valid.replace('"RUN"','"unknown"'),valid+' $Use: "turret"',valid+' $Use: "turret" +radius: nan']
for text in bad:assert check(text).returncode==3
report=dict(result='PASS',classes=len(records),malformed_cases=len(bad),records=records,scope='Authored input comparison using original flag tables; not full original parser execution or post-parse class defaults.')
(root/'artifacts/class-physics-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
