"""Compare the complete shared SP weapon-drop orchestration to the original oracle."""
import copy,hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
subprocess.run([sys.executable,str(root/'tools/verify_weapon_drop_original.py')],cwd=root,check=True,stdout=subprocess.DEVNULL)
reference=json.loads((root/'artifacts/weapon-drop-original.json').read_text())
# Resource-failure cases retain prior RNG/inventory changes and expose partial items.
for failure in (2,3):
 row=copy.deepcopy(next(r for r in reference['records'] if r['result'] and 'remove' in r['events']))
 row['input']['allocation']=failure;row['status']=-1
 if failure==2:
  row['events']=row['events'][:row['events'].index('query')+1];row['creation']=None;row['result']=False;row['item_after']=None
 else:
  row['item_after']['position']=row['creation']['position'][:];row['item_after']['base_position']=row['creation']['position'][:]
 reference['records'].append(row)
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v));f=lambda *v:struct.pack('<'+'f'*len(v),*v)
events_map={'pose':0,'map':1,'remote_replacement':2,'remove':3,'query':4,'create':5,'notify':6,'bounds':7}
inputs=[];expected=[]
for r in reference['records']:
 c=r['input'];source=w(c['current'],c['flags'],c['handle'],c['notification_owner'])+f(*c['position'],c['extent'])
 inv=bytearray(bytes([1])*64+bytes(384));defs=bytearray(512)
 if c['current']>=0:
  inv[64:68]=w(c['reserve']);inv[192+4*c['current']:196+4*c['current']]=w(c['loaded']);defs[c['current']*8:c['current']*8+8]=w(0,c['default_count'])
 payload=source+inv+defs+w(c['excluded'],c['parameter'],c['seed'])+f(*c['pose_position'],*c['pose_basis'])+w(c['pose_handled'],c['mapped_item'],c['remote'],c['replacement_item'])+w(c['hit_count'])+f(*c['hit_point'],*c['normal'])+w(c['allocation'],c['item_flags'])+f(c['bound_x'])
 assert len(payload)==1108;inputs.append(payload)
 owned=bytearray(bytes([1])*64)
 if 'remove' in r['events']:owned[c['current']]=0
 out=w(r.get('status',0),r['current_after'],r['random'],r['result'])+owned
 q=r['query'];out+=f(*q['start'],*q['delta'])+w(q['current']) if q else bytes(28)
 cr=r['creation'];out+=w(cr['index'],cr['quantity'],c['handle'])+f(*cr['position'],*cr['basis']) if cr else bytes(60)
 it=r['item_after'];out+=w(it['flags'],0x12345678)+f(*it['position'],*it['base_position']) if it else bytes(32)
 ev=[events_map[e] for e in r['events']];out+=w(len(ev),*ev)+bytes((16-len(ev))*4)
 out+=w(c['notification_owner'],cr['index'])+f(*cr['position']) if 'notify' in r['events'] else bytes(20)
 assert len(out)==288;expected.append(out)
exe=root/'build/pc/Release/rf_weapon_probe.exe';pc=subprocess.check_output([str(exe),'--drop'],input=b''.join(inputs));assert len(pc)==len(expected)*288
for i,want in enumerate(expected):assert pc[i*288:(i+1)*288]==want,('PC',i,[(j,pc[i*288+j:i*288+j+4].hex(),want[j:j+4].hex()) for j in range(0,288,4) if pc[i*288+j:i*288+j+4]!=want[j:j+4]])
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);b=0x30000000;x.mem_map(b,0x20000);stack=b+0x1d000;stop=b+0x1e000
entry=int(re.search(r'\s_rf_weapon_drop_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
remove_entry=int(re.search(r'\s_rf_weapon_remove_owned\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
# cdecl remove(ctx, weapon) adapter executes compiled shared removal with no players.
# Push backend, original weapon argument, inventory; call and clean our arguments.
x.mem_write(b+0x8400,b'\x68'+w(b+0xa000)+b'\xff\x74\x24\x0c\x68'+w(b+0x1000)+b'\xb8'+w(remove_entry)+b'\xff\xd0\x83\xc4\x0c\xc3')
x.mem_write(b+0xa100,w(0,0xffffffff))
x.mem_write(b+0xa000,w(0,b+0xa100,b+0xa104,0,b+0x8000,b+0x8000,0))
get=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
case={};events=[];query=bytes(28);request=bytes(60);notification=bytes(20)
def hook(m,a,n,unused):
 global query,request,notification
 if a<b+0x8000 or a>b+0x8800 or (a-b-0x8000)%256:return
 op=(a-b-0x8000)//256;sp=m.reg_read(UC_X86_REG_ESP);result=0
 if op==0:events.append(0);m.mem_write(get(sp+8),f(*case['pose_position'],*case['pose_basis']));result=case['pose_handled']
 elif op==1:events.append(1);assert get(sp+8)==get(b);result=case['mapped_item']
 elif op==2:assert get(sp+8)==case['mapped_item'];result=int(case['remote'])
 elif op==3:events.append(2);result=case['replacement_item']
 elif op==4:
  events.append(3);assert get(sp+8)==get(b);return # Execute the adapter and native removal.
 elif op==5:
  events.append(4);query=bytes(m.mem_read(get(sp+8),12))+bytes(m.mem_read(get(sp+12),12))+w(get(b))
  m.mem_write(get(sp+16),w(case['hit_count'])+f(*case['hit_point'],*case['normal']));result=0xffffffff if case['allocation']==2 else 0
 elif op==6:
  events.append(5);request=bytes(m.mem_read(get(sp+8),60));point=request[12:24]
  m.mem_write(b+0x6000,w(case['item_flags'],0x12345678)+point+point+w(0));result=b+0x6000 if case['allocation'] else 0
 elif op==7:
  events.append(6);notification=w(get(sp+8),get(sp+12))+bytes(m.mem_read(get(sp+16),12))
 elif op==8:events.append(7);assert get(sp+8)==0x12345678;m.mem_write(get(sp+12),f(case['bound_x']));result=0xffffffff if case['allocation']==3 else 0
 target=get(sp);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_EIP,target)
x.hook_add(UC_HOOK_CODE,hook)
for i,(payload,want,r) in enumerate(zip(inputs,expected,reference['records'])):
 case=r['input'];events=[];query=bytes(28);request=bytes(60);notification=bytes(20)
 x.mem_write(b,payload[:32]);x.mem_write(b+0x1000,payload[32:480]);x.mem_write(b+0x2000,payload[480:992]);x.mem_write(b+0x3000,w(case['seed']))
 x.mem_write(b+0x4000,w(*[b+0x8000+j*256 for j in range(9)],0));x.mem_write(b+0x5000,w(0))
 x.mem_write(stack,w(stop,b,b+0x1000,b+0x2000,case['excluded'],case['parameter'],b+0x3000,b+0x4000,b+0x5000));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 out=w(x.reg_read(UC_X86_REG_EAX),get(b),get(b+0x3000),get(b+0x5000)!=0)+bytes(x.mem_read(b+0x1000,64))+query+request
 out+=bytes(x.mem_read(b+0x6000,32)) if get(b+0x5000) else bytes(32)
 out+=w(len(events),*events)+bytes((16-len(events))*4)+notification
 assert out==want,('NXDK',i,[(j,out[j:j+4].hex(),want[j:j+4].hex()) for j in range(0,288,4) if out[j:j+4]!=want[j:j+4]])
report=dict(result='PASS',cases=len(expected)-2,resource_failure_cases=2,original_sha256=reference['original_sha256'],pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),branches=reference['branches'],scope='Complete shared SP weapon drop vs original42ae10 oracle: exact current/inventory, RNG, query, creation quantity/point/basis, item state, callback ordering and notification. Original4031a0 and shared PC/NXDK removal execute inside the drop with zero active players. Other resource callbacks supplied; no live item allocation or XEMU gameplay.')
(root/'artifacts/weapon-drop-shared.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
