"""Original42f940 through its sound/alternate dispatch boundaries versus C."""
import hashlib,json,struct,subprocess,sys,random,re
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe=pefile.PE(str(exe));im=pe.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,0x30000);entity=base;cls=base+0x2000;desc=base+0x4000;model=base+0x8000;wrapper=base+0xb000;records=base+0xc000;samples=base+0x10000;player=base+0x14000;view=base+0x16000;stack=base+0x28000;stop=base+0x2f000
requests=[];alternate=0
rd=lambda a,n:bytes(u.mem_read(a,n))
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
def hook(uc,address,size,context):
 global alternate
 if address not in (0x48a930,0x42fb20):return
 esp=uc.reg_read(UC_X86_REG_ESP)
 if address==0x42fb20:alternate+=1
 else:
  args=rd(esp+4,32);assert struct.unpack_from('<I',args)[0]==entity
  pointer,count=struct.unpack_from('<Ii',args,16);first=(pointer-samples)//4;group=first//16
  requests.append(struct.pack('<iIi',group,first,count)+args[4:16]+args[24:32])
 ret=struct.unpack('<I',rd(esp,4))[0];uc.reg_write(UC_X86_REG_ESP,esp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x42f940);cases=[]
for k in range(640):
 linked=-1 if k%5 else 7;flags=8 if k%3 else 0;present=k%2;mode=(k//2)%2;surface=(k//4)%10
 pos=[rng.choice([-12.,0.,.125,10.5]) for _ in range(3)];height=rng.choice([0.,.5,1.25]);side=rng.choice([-1.,0.,.25,1.])
 groups=[rng.choice([-1,0,1,2]) for _ in range(10)]
 input=struct.pack('<iIIiI5f10i',linked,flags,present,mode,surface,*pos,height,side,*groups)
 state=bytearray(260);struct.pack_into('<I',state,0,1);struct.pack_into('<iif',state,4,(k//4)%4,160,1);struct.pack_into('<3i',state,196,-1,-1,-1 if k%11==0 else 0);struct.pack_into('<I',state,252,1);struct.pack_into('<I',state,256,k%4)
 names=b''.join(a.encode().ljust(16,b'\0')+b.encode().ljust(16,b'\0') for a,b in [('footstep_left','footstep_right'),('footstep_right','footstep_left'),('footstep_left','footstep_left'),('unrelated','')])
 sound_groups=b''.join(struct.pack('<iI',(k+i)%10,i*16) for i in range(3))
 cases.append(input+state+names+sound_groups)
run=subprocess.run([str(root/'build/pc/Release/rf_motion_file_probe.exe'),'--footsteps'],input=b''.join(cases),capture_output=True,check=True);assert len(run.stdout)==len(cases)*336
counts=dict(alternate=0,sound_requests=0,preserved_right_after_left_failure=0)
for k,raw in enumerate(cases):
 linked,flags,present,mode,surface=struct.unpack_from('<iIIiI',raw);state=raw[80:340];names=raw[340:468];group_data=raw[468:492]
 u.mem_write(entity,bytes(0x2000));u.mem_write(model,bytes(0x2000));put(entity+0x200,'<i',linked);put(entity+0x7c,'<I',flags);put(entity+0x1430,'<I',player if present else 0);put(player+0xc4,'<I',view);put(view+8,'<i',mode)
 put(entity+0x1380,'<I',surface);put(entity+0x294,'<I',cls);u.mem_write(cls+0x178,raw[40:80]);u.mem_write(entity+0x3c,raw[20:32]);u.mem_write(entity+0x180,raw[32:36]);u.mem_write(0x62f980,raw[36:40])
 put(entity+0x80,'<I',wrapper);put(wrapper,'<II',2,model);put(model+0x1d50,'<I',desc);u.mem_write(model+0x12d0,state[:196]);u.mem_write(model+0x1d48,state[204:208]);mask=struct.unpack_from('<I',state,256)[0];u.mem_write(model+0x1d44,bytes([mask&1,mask>>1]))
 for i in range(4):put(desc+0xf5c+i*4,'<I',records+i*256);u.mem_write(records+i*256+0x40,names[i*32:i*32+16]);u.mem_write(records+i*256+0x54,names[i*32+16:i*32+32])
 put(0x636ef8,'<I',3)
 for i in range(3):
  count,first=struct.unpack_from('<iI',group_data,i*8);put(0x63011c+i*44,'<i',count);put(0x630120+i*44,'<I',samples+first*4)
 requests=[];alternate=0;put(stack,'<II',stop,entity);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x42f940,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 expected_state=bytearray(state);bits=rd(model+0x1d44,2);struct.pack_into('<I',expected_state,256,bits[0]|bits[1]<<1)
 plan=struct.pack('<II',alternate,len(requests))+b''.join(requests)+bytes((2-len(requests))*32)
 expected=struct.pack('<i',0)+expected_state+plan;assert run.stdout[k*336:(k+1)*336]==expected,(k,run.stdout[k*336:(k+1)*336].hex(),expected.hex())
 counts['alternate']+=alternate;counts['sound_requests']+=len(requests)
 if mask==3 and bits==bytes([0,1]) and not requests:counts['preserved_right_after_left_failure']+=1
nxpe=pefile.PE(str(root/'build/xbox/main.exe'));nximage=nxpe.get_memory_mapped_image();nx=Uc(UC_ARCH_X86,UC_MODE_32)
nxbase=nxpe.OPTIONAL_HEADER.ImageBase;nx.mem_map(nxbase,(len(nximage)+4095)//4096*4096);nx.mem_write(nxbase,nximage);nx.mem_map(base,0x30000)
entry=int(re.search(r'_rf_entity_plan_footsteps\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for k,raw in enumerate(cases):
 nx.mem_write(entity,raw);nx.mem_write(entity+0x1000,bytes([0xa5])*72)
 nx.mem_write(stack,struct.pack('<8I',stop,entity,entity+80,entity+340,4,entity+468,3,entity+0x1000));nx.reg_write(UC_X86_REG_ESP,stack);nx.reg_write(UC_X86_REG_FPCW,0x27f)
 nx.emu_start(entry,stop,count=100000);assert nx.reg_read(UC_X86_REG_EIP)==stop
 result=struct.pack('<I',nx.reg_read(UC_X86_REG_EAX))+bytes(nx.mem_read(entity+80,260))+bytes(nx.mem_read(entity+0x1000,72))
 assert result==run.stdout[k*336:(k+1)*336],('NXDK',k)
assert counts['alternate']>0 and counts['sound_requests']>50 and counts['preserved_right_after_left_failure']>0,counts
report=dict(result='PASS',original_sha256=digest,cases=len(cases),**counts,scope='Complete original42f940 and actual marker/model/group/vector callees; intercept only alternate42fb20 and sound48a930 dispatch to observe requests. PC and NXDK machine code match event consumption, linked/player-view routing, surface/default fallback, signed half counts, positions and both final scalar arguments. No RNG/audio backend execution.')
(root/'artifacts/entity-footsteps.json').write_text(json.dumps(report,indent=2));print(report)
