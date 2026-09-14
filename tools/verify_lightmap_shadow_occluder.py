"""Replay original shadow occluder selection against PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_shadow_occluder\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)
def reject(u,a,size,ctx):u.emu_stop()
def texture(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,excluded);u.reg_write(UC_X86_REG_EIP,struct.unpack('<I',u.mem_read(sp,4))[0]);u.reg_write(UC_X86_REG_ESP,sp+4)
o.hook_add(UC_HOOK_CODE,reject,begin=0x4f5515,end=0x4f5515);o.hook_add(UC_HOOK_CODE,texture,begin=0x510710,end=0x510710)
rng=random.Random(0x4f4f15);inputs=[];responses=[];accepted_total=0
for i in range(4096):
 lightmin=[-10]*3;lightmax=[10]*3;mapmin=[-10]*3;mapmax=[10]*3;mapping=7;mapplane=[0,0,1,0];faceplane=[0,1,0,2];minimum=[-1]*3;maximum=[1]*3;flags=0;facemap=8;portal=0;excluded=0;planes=[[rng.uniform(-1,1) for _ in range(3)]+[-10] for j in range(6)]
 mode=i%16
 if mode==1:lightmin[0]=2
 if mode==2:mapmax[1]=-2
 if mode==3:portal=1
 if mode in (4,5,6):flags=[4,64,8192][mode-4]
 if mode==7:facemap=7
 if mode==8:excluded=1
 if mode==9:faceplane=mapplane[:]
 if mode==10:planes[i%6]=[0,0,0,rng.choice([-.001,-.0010001,-.0009999,0])]
 if mode==11:faceplane=[0,0,rng.choice([.9989,.999,.9991,1]),rng.choice([0,.000999,.001,.001001])]
 if mode==12:planes=[[rng.uniform(-2,2) for _ in range(4)] for j in range(6)]
 if mode==13:portal=-1;facemap=-32768
 if mode==14:lightmin[0]=1;mapmax[1]=-1
 if mode==15:flags=0x80000000;facemap=32767
 if i%17==0:planes[i%6]=list(struct.unpack("<4f",w(*([0xffc00000]*4))))
 view=f(*lightmin,*lightmax,*mapmin,*mapmax,*mapplane,*[q for p in planes for q in p])+w(mapping);face=f(*faceplane,*minimum,*maximum)+w(flags,facemap&0xffffffff,portal&0xffffffff,excluded);data=view+face
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER,w(mapping));o.mem_write(OWNER+0x6c,f(*mapplane));o.mem_write(B+0x1000,bytes(80));o.mem_write(B+0x1000,f(*faceplane,*minimum,*maximum)+w(flags,0,0)+struct.pack('<hh',portal,facemap));o.mem_write(STACK,bytes(4096));o.mem_write(STACK+0x18,w(B+0x1000));o.mem_write(STACK+0x4c,f(*lightmin,*lightmax));o.mem_write(STACK+0xd4,f(*mapmin));o.mem_write(STACK+0xf8,f(*mapmax));o.mem_write(STACK+0x1a0,f(*[q for p in planes for q in p]));o.mem_write(B+0xf00c,w(OWNER))
 for j,p in enumerate(planes):
  bits=0
  for v in p[:3]:bits=bits*2+(v>0)
  o.mem_write(STACK+0x260+j*4,w([4,0,5,1,7,3,6,2][bits]))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBP,B+0xf000);o.reg_write(UC_X86_REG_EAX,B+0x1000);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f4f15,0x4f5031,count=100000);end=o.reg_read(UC_X86_REG_EIP);assert end in (0x4f5031,0x4f5515);expected=int(end==0x4f5031)
 x.mem_write(B,data);x.mem_write(OUT,w(0xa5a5a5a5));assert call(x,entry,[B,B+164,OUT])==0;got=struct.unpack('<I',x.mem_read(OUT,4))[0];assert got==expected,(i,mode,got,expected)
 inputs.append(data);responses.append(w(0,expected));accepted_total+=expected
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-occluder'],input=b''.join(inputs))==b''.join(responses)
for at,value in [(0,f(float('nan'))),(64,f(float('inf'))),(164,f(float('nan'))),(164+44,w(32768)),(164+48,w(32768))]:
 bad=bytearray(data);bad[at:at+4]=value;x.mem_write(B,bytes(bad));x.mem_write(OUT,w(0xa5a5a5a5));status=call(x,entry,[B,B+164,OUT]);assert status!=0 and bytes(x.mem_read(OUT,4))==w(0xa5a5a5a5)
 assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-occluder'],input=bytes(bad))==w(status,0xa5a5a5a5)
report=dict(result='PASS',original_pc_nxdk_faces=len(inputs),accepted=accepted_total,rejected=len(inputs)-accepted_total,pc_nxdk_guards=5,original_sha256=sha,x87_control_word='0x027f',scope='Original4f4f15..4f5031 with actual bounds overlap, signed metadata, coplanar test, support corners and six plane distances. Only510710 texture classification supplied. Canonical indefinite planes and sixteen case groups include equality thresholds, signed IDs and unrelated flags. Retained face decoding/texture ownership and native rendering excluded.')
(root/'artifacts/lightmap-shadow-occluder.json').write_text(json.dumps(report,indent=2));print(report)
