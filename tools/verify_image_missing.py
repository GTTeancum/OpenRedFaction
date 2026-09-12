"""Original missing-bitmap generator versus owned PC and compiled NXDK images."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
B=0x30000000;PIX=B+0x2000;STACK=B+0xe000;STOP=B+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)&~4095);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,65536);return u
u=machine(exe);u.reg_write(UC_X86_REG_EAX,PIX);u.emu_start(0x50f1d8,0x50f20d,count=100000)
assert u.reg_read(UC_X86_REG_EIP)==0x50f20d
original=bytes(u.mem_read(PIX,3072));rgba=b''.join(bytes((original[i+2],original[i+1],original[i],255)) for i in range(0,3072,3))
# Fallback requests format6; original55dd20 copies its BGR24 bytes unchanged.
def run(cpu,entry,args):
 cpu.mem_write(STACK,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,STACK);cpu.emu_start(entry,STOP,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP;return cpu.reg_read(UC_X86_REG_EAX)
run(u,0x55dd20,(B+0x4000,6,PIX,6,1024))
assert bytes(u.mem_read(B+0x4000,3072))==original
probe=root/'build/pc/Release/rf_image_probe.exe'
for budget in [0,4095,4096,8192]:
 pc=subprocess.check_output([str(probe),'--missing',str(budget)])
 assert pc==(w(0,32,32,4096,6)+rgba if budget>=4096 else w(-4,0,0,0,0))
x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry=symbol('rf_image_missing');close=symbol('rf_image_close')
for name,address in [('MmAllocateContiguousMemoryEx@20',B+0xf100),('MmFreeContiguousMemory@4',B+0xf110)]:
 iat=int(re.search(r'__imp__'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16);x.mem_write(iat,w(address))
fail=False;trace=[]
def kernel(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',cpu.mem_read(sp,4))[0]
 if address==B+0xf100:
  args=struct.unpack('<5I',cpu.mem_read(sp+4,20));assert args==(4096,0,0x03ffb000,0,0x404),args;trace.append('allocate');result=0 if fail else PIX;pop=24
 else:
  assert bytes(cpu.mem_read(sp+4,4))==w(PIX);trace.append('free');result=0;pop=8
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+pop);cpu.reg_write(UC_X86_REG_EIP,ret)
for address in [B+0xf100,B+0xf110]:x.hook_add(UC_HOOK_CODE,kernel,begin=address,end=address)
swizzled=bytearray(4096)
for yy in range(32):
 for xx in range(32):
  index=sum(((xx>>bit)&1)<<(bit*2)|((yy>>bit)&1)<<(bit*2+1) for bit in range(5));swizzled[index*4:index*4+4]=rgba[(yy*32+xx)*4:(yy*32+xx+1)*4]
for budget,fail in [(0,False),(4095,False),(4096,False),(8192,False),(4096,True)]:
 trace=[];x.mem_write(B,b'\xa5'*20);x.mem_write(PIX-4,b'guar');x.mem_write(PIX+4096,b'guard')
 result=run(x,entry,(B,budget));ok=budget>=4096 and not fail
 assert result==(0 if ok else 0xfffffffc)
 assert bytes(x.mem_read(B,20))==(w(32,32,4096,6,PIX) if ok else bytes(20))
 assert trace==(['allocate'] if budget>=4096 else [])
 if ok:assert bytes(x.mem_read(PIX,4096))==swizzled
 assert bytes(x.mem_read(PIX-4,4))==b'guar'
 assert bytes(x.mem_read(PIX+4096,5))==b'guard'
 run(x,close,(B,));assert bytes(x.mem_read(B,20))==bytes(20)
 assert trace==(['allocate','free'] if ok else ['allocate'] if budget>=4096 else [])
assert run(x,entry,(0,4096))==0xfffffffc
report=dict(result='PASS',pixels=1024,pc_cases=4,nxdk_cases=6,bytes=4096,original_sha256=sha,scope='Exact original50f1d8 generator and55dd20 same-format BGR24 copy; normalized opaque RGBA storage follows shared image convention. PC owned pixels and compiled NXDK Morton layout, allocation failure, budget, bounds and release. Kernel allocation/free supplied; no live material fallback binding.')
(root/'artifacts/image-missing.json').write_text(json.dumps(report,indent=2));print(report)
