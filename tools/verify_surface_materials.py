"""Verify authored surface prefixes against complete original 468740 and NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
w=lambda *v:struct.pack('<%dI'%len(v),*v)
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for f in inventory['files'] if f['path']=='tables.vpp' for e in f['vpp']['entries'] if e['name']=='materials.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:
    f.seek(entry['offset']);text=f.read(entry['size']).decode('cp1252')
names=['default','rock','metal','flesh','water','lava','solid','sand','ice','glass']
blocks=re.split(r'(?i)\$name:\s*"([^"]+)"',text.split('#End')[0]);prefixes=[];tractions={}
for i in range(1,len(blocks),2):
    index=names.index(blocks[i].lower());tractions[index]=float(re.search(r'(?i)\$traction:\s*(\S+)',blocks[i+1])[1])
    prefixes.extend((p,index) for p in re.findall(r'(?i)\$bitmap\s+prefix:\s*"([^"]*)"',blocks[i+1]))
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
u.mem_map(base,0x10000);u.mem_write(0x64a208,w(len(prefixes),len(prefixes),base))
palette=bytearray(2548);struct.pack_into('<I',palette,240,len(prefixes))
for i,(prefix,index) in enumerate(prefixes):
    obj=base+0x1000+i*64;u.mem_write(base+i*4,w(obj));u.mem_write(obj,w(len(prefix),obj+12,index)+prefix.encode()+b'\0')
    palette[244+i*36:244+i*36+len(prefix)]=prefix.encode();struct.pack_into('<I',palette,244+i*36+32,index)
pe=pefile.PE(str(root/'build/xbox/main.exe'));xb=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000);x.mem_write(base,bytes(palette))
address=int(re.search(r'_rf_surface_material_lookup\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=['','unknown_test','ice','iceberg_test','path/ice_test','_ice','ice_test_more']
for prefix,index in prefixes:cases.extend([prefix+'_test',prefix.upper()+'_TEST',prefix,prefix+'x_test'])
probe=root/'build/pc/Release/rf_entity_assets_probe.exe'
for name in cases:
    u.mem_write(base+0x4000,name.encode()+b'\0');u.mem_write(stack,w(stop,base+0x4000));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x468740,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=u.reg_read(UC_X86_REG_EAX)
    x.mem_write(base+0x4000,name.encode()+b'\0');x.mem_write(stack,w(stop,base,base+0x4000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(address,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==expected, name
    got=subprocess.check_output([str(probe),'--surface',str(root/'Installed_Game/tables.vpp'),name],text=True).split()
    assert int(got[0])==expected and struct.pack('<f',float(got[1]))==struct.pack('<f',tractions[expected]),(name,got,expected)
folder=root/'artifacts/surface-reader-tests';folder.mkdir(exist_ok=True)
def fixture(payload,name):
    payload=payload.encode();size=4096+((len(payload)+2047)//2048)*2048;data=bytearray(size)
    struct.pack_into('<4I',data,0,0x51890ace,1,1,size);data[2048:2062]=b'materials.tbl\0'
    struct.pack_into('<I',data,2108,len(payload));data[4096:4096+len(payload)]=payload
    path=folder/'fixture.vpp';path.write_bytes(data)
    return subprocess.run([str(probe),'--surface',str(path),name],capture_output=True)
# Duplicate prefixes preserve declaration order; malformed/oversized input must
# fail transactionally (the C probe checks that the result stays untouched).
valid='#Materials '+ ' '.join('$name: "'+n+'" $elasticity: 1 $friction: 1 $density: 1 $bouyancy: 1 $traction: '+str(tractions[i])+' $bitmap prefix: "shared"' for i,n in enumerate(names))+' #End'
assert fixture(valid,'SHARED_test').stdout.decode().split()[0]=='0'
bad=[valid.replace('#End',''),valid.replace('"shared"','"'+('x'*32)+'"',1),valid.replace('$traction: 1.0','$traction: nan',1),valid.replace('prefix:', 'prefix',1),valid.replace('#End',('$bitmap prefix: "extra" '*65)+'#End')]
for payload in bad:assert fixture(payload,'shared_test').returncode==3
report=dict(result='PASS',prefixes=len(prefixes),original_pc_nxdk_cases=len(cases),malformed_cases=len(bad),duplicate_order_cases=1,scope='Complete original 468740 with prepared authored prefix registry; PC parses actual materials.tbl, linked NXDK lookup matches. No replaced callees.')
if len(sys.argv)>1:
    snapshot=json.loads(Path(sys.argv[1]).read_text())['symbols']
    actual=struct.pack('<60I',*snapshot['rf_scene_actor_surface_values']['words'])
    expected=bytearray(240)
    tags=['elasticity','friction','density','bouyancy','traction']
    for i in range(1,len(blocks),2):
        index=names.index(blocks[i].lower())
        values=[float(re.search(r'(?i)\$'+tag+r':\s*(\S+)',blocks[i+1])[1]) for tag in tags]
        expected[index*24:(index+1)*24]=struct.pack('<I5f',index,*values)
    assert actual==expected
    frames=snapshot['rf_scene_actor_surface_frames']['words']
    assert len(frames)==128
    for i in range(0,128,2):assert struct.pack('<I',frames[i+1])==struct.pack('<f',tractions[frames[i]])
    report['guest_material_bytes_match_authored']=240;report['guest_frame_tractions_match_authored']=64
(root/'artifacts/surface-material-verification.json').write_text(json.dumps(report,indent=2));print(report)
