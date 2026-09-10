"""Exhaust static VBM words and round-trip through original 55dd20 packing."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
out=root/'artifacts/vbm-tests';out.mkdir(exist_ok=True)
probe=root/'build/pc/Release/rf_image_probe.exe';raw=out/'pixels.rgba'
def check(data,budget=262144):
 size=4096+(len(data)+2047)//2048*2048;b=bytearray(size)
 struct.pack_into('<4I',b,0,0x51890ace,1,1,size);b[2048:2056]=b'test.vbm'
 struct.pack_into('<I',b,2108,len(data));b[4096:4096+len(data)]=data
 path=out/'fixture.vpp';path.write_bytes(b)
 return subprocess.run([str(probe),str(path),'test.vbm',str(raw),str(budget)],capture_output=True,text=True)
def header(fmt=2,w=256,h=256,frames=1,mips=0,version=1):
 return b'.vbm'+struct.pack('<7I',version,w,h,fmt,15,frames,mips)
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;u.mem_map(base,0x100000);stack=base+0xf0000;stop=base+0xff000
# Execute the linked NXDK decoder with archive I/O, allocation and stack probing supplied.
pe=pefile.PE(str(root/'build/xbox/main.exe'));xb=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x100000)
mapping=(root/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
chk=symbol('_chkstk');read=symbol('rf_vpp_read');alloc=symbol('malloc');release=symbol('free');decode=symbol('rf_image_vbm');data=b''
def hook(cpu,address,size,context):
 if address==chk:
  sp=cpu.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',cpu.mem_read(sp,4))[0]
  cpu.reg_write(UC_X86_REG_ESP,sp+4-cpu.reg_read(UC_X86_REG_EAX));cpu.reg_write(UC_X86_REG_EIP,ret);return
 if address not in (read,alloc,release):return
 sp=cpu.reg_read(UC_X86_REG_ESP);args=struct.unpack('<6I',cpu.mem_read(sp,24));ret=args[0]
 if address==read:
  offset,dest,count=args[3:6];assert offset+count<=len(data);cpu.mem_write(dest,data[offset:offset+count]);value=0
 elif address==alloc:
  assert args[1]<=262144;value=base+0x50000
 else:value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook)
def xbox(payload,budget=262144):
 global data
 data=payload;x.mem_write(base,bytes(4096));x.mem_write(base+0x200+68,struct.pack('<I',len(data)))
 x.mem_write(stack,struct.pack('<5I',stop,base,base+0x100,base+0x200,budget));x.reg_write(UC_X86_REG_ESP,stack)
 try:x.emu_start(decode,stop,count=20000000)
 except Exception:
  print('NXDK fault',hex(x.reg_read(UC_X86_REG_EIP)),hex(x.reg_read(UC_X86_REG_ESP)));raise
 assert x.reg_read(UC_X86_REG_EIP)==stop
 result=x.reg_read(UC_X86_REG_EAX);return result if result<0x80000000 else result-0x100000000
pixels=struct.pack('<65536H',*range(65536))
for version,fmt,engine in [(v,f,e) for v in (1,2) for f,e in [(0,5),(1,4),(2,3)]]:
 r=check(header(fmt,version=version)+pixels);assert r.returncode==0,r.stdout
 rgba=raw.read_bytes();assert len(rgba)==262144
 assert xbox(header(fmt,version=version)+pixels)==0
 assert bytes(x.mem_read(base+0x50000,262144))==rgba
 bgra=bytearray(rgba);bgra[0::4]=rgba[2::4];bgra[2::4]=rgba[0::4]
 u.mem_write(base,bytes(bgra));u.mem_write(stack,struct.pack('<6I',stop,base+0x50000,engine,base,7,65536))
 u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x55dd20,stop,count=10000000)
 wanted=pixels
 if version==1 and fmt==0:
  u.mem_write(base,pixels);u.mem_write(stack,struct.pack('<3I',stop,base,65536));u.reg_write(UC_X86_REG_ESP,stack)
  u.emu_start(0x511410,stop,count=1000000);wanted=bytes(u.mem_read(base,len(pixels)))
 assert bytes(u.mem_read(base+0x50000,len(pixels)))==wanted,(version,fmt)
 # Exact normalized-channel values (not just invertibility).
 for index in range(65536):
  v=index^(0x8000 if version==1 and fmt==0 else 0)
  if fmt==1:want=bytes(((v>>8&15)*17,(v>>4&15)*17,(v&15)*17,(v>>12)*17))
  else:want=bytes(((v>>(11 if fmt==2 else 10)&31)*255//31,(v>>5&(63 if fmt==2 else 31))*255//(63 if fmt==2 else 31),(v&31)*255//31,255 if fmt==2 or v&32768 else 0))
  assert rgba[index*4:index*4+4]==want
bad=[header()+pixels[:-1],header()+pixels+b'x',header(frames=2)+pixels,header(version=3)+pixels,header(fmt=3)+pixels,header(w=0)+pixels,header(mips=13)+pixels,header(mips=1)+pixels]
for payload in bad:
 assert check(payload).stdout.strip()=='-2'
 assert xbox(payload)==-2
assert check(header()+pixels,262143).stdout.strip()=='-4'
assert xbox(header()+pixels,262143)==-4
assert check(header(mips=1)+pixels+bytes(128*128*2)).returncode==0
assert xbox(header(mips=1)+pixels+bytes(128*128*2))==0
assert check(header(w=1,h=1,mips=1)+bytes(4)).stdout.strip()=='-2'
report=dict(result='PASS',original_packer_words=6*65536,malformed_or_budget_cases=len(bad)+2,valid_mip_cases=1,scope='PC/NXDK decoders and original 55dd20 packing; static frames only')
(out/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))

