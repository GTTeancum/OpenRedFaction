"""Bounded PC/NXDK supply reader against the original supply-binding audit."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
subprocess.check_call([sys.executable,str(root/'tools/verify_weapon_supply_binding.py')]);audit=json.loads((root/'artifacts/weapon-supply-binding.json').read_text());inventory=json.loads((root/'artifacts/inventory.json').read_text())
def table(name):
 e=next(e for a in inventory['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==name)
 with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
ammo=table('ammo.tbl');weapons=table('weapons.tbl');p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);base=p.OPTIONAL_HEADER.ImageBase;x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im);B=0x30000000;x.mem_map(B,0x100000);STACK=B+0xff000;STOP=B+0xfff00
entry=int(re.search(r'\s_rf_weapon_supply_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def stack_probe(c,at,size,data):
 sp=c.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',c.mem_read(sp,4))[0];c.reg_write(UC_X86_REG_ESP,sp+4-c.reg_read(UC_X86_REG_EAX));c.reg_write(UC_X86_REG_EIP,ret)
# Pre-mapped fixture stack implements NXDK probe ABI; kernel stack growth excluded.
probe=int(re.search(r'\s__chkstk\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.hook_add(UC_HOOK_CODE,stack_probe,begin=probe,end=probe)
def compiled(a,b):
 x.mem_write(B+0x10000,a);x.mem_write(B+0x20000,b);x.mem_write(B,b'\xa5'*4872);x.mem_write(STACK,w(STOP,B+0x10000,len(a),B+0x20000,len(b),B));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=20000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,4872))
expected=bytearray(4872)
for i,row in enumerate(audit['weapons']):
 name=row['name'].encode();expected[i*64:i*64+len(name)]=name;expected[4104+i*12:4116+i*12]=w(row['ammo_index'],row['sp_capacity'],row['sp_magazine'])
expected[4096:4104]=w(44,40);cases=[(ammo,weapons)];outputs=[w(0)+expected];assert compiled(ammo,weapons)==outputs[0]
a=b'#Ammo\n$Name: "shared"\n#End\n';block=b'$Name: "test"\n$Max Ammo: 125 210\n$Ammo Type: "SHARED"\n$Clip Size: 16 64\n';wrap=lambda b:b'#Primary Weapons\n'+b+b'#End\n#Secondary Weapons\n#End\n'
for b in (block.replace(b'$Max Ammo: 125 210\n',b''),block+ b'$Clip Size: 1 2\n',block.replace(b'125 210',b'125 bad'),block.replace(b'"SHARED"',b'SHARED')):
 result=compiled(a,wrap(b));assert result==w(-2)+b'\xa5'*4872;cases.append((a,wrap(b)));outputs.append(result)
for aa,bb in ((a.replace(b'#End',b''),wrap(block)),(b'#Ammo\n'+b'$Name: "a"\n'*33+b'#End\n',wrap(block))):
 result=compiled(aa,bb);assert result==w(-2 if len(aa)<100 else -4)+b'\xa5'*4872;cases.append((aa,bb));outputs.append(result)
for aa,bb,index,clip in ((a,wrap(block),0,16),(a,wrap(block.replace(b'$Clip Size: 16 64\n',b'')),0,0),(a,wrap(block.replace(b'SHARED',b'unknown')),-1,16),(a.replace(b'#End',b'$Name: "SHARED"\n#End'),wrap(block),0,16)):
 result=compiled(aa,bb);out=bytearray(4872);out[:4]=b'test';out[4096:4104]=w(1,1);out[4104:4116]=w(index,125,clip);assert result==w(0)+out;cases.append((aa,bb));outputs.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--weapon-supply'],input=b''.join(w(len(a),len(b))+a+b for a,b in cases));assert pc==b''.join(outputs)
for budget in (len(ammo)+len(weapons),len(ammo)+len(weapons)-1,0):
 result=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--weapon-supply-load',str(root/'Installed_Game/tables.vpp'),str(budget)]);assert result==(outputs[0] if budget==len(ammo)+len(weapons) else w(-4)+b'\xa5'*4872)
report=dict(result='PASS',authored_weapons=44,pc_nxdk_reader_cases=len(cases),archive_budget_cases=3,catalog_bytes=4872,scratch_bytes=len(ammo)+len(weapons),scope='Compiled PC/NXDK parser output matches all original-audited ammo IDs and SP values; malformed/duplicate fields, ammo bound, optional clip, case-insensitive first-match and unknown ammo checked. Archive budget tested on PC. Scene retention and native XEMU excluded.')
(root/'artifacts/weapon-supply-reader.json').write_text(json.dumps(report,indent=2));print(report)
