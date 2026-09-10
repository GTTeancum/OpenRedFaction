"""DirectSound units -> linear amplitude adapter checks, not original code equivalence."""
import itertools,json,math,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
p=pefile.PE(str(root/'build/xbox/main.exe'));data=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;x.mem_map(base,65536)
entry=int(re.search(r'_rf_audio_device_gains\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=list(itertools.product([-10000,-9999,-6000,-2000,-1000,-600,-1,0],[-10000,-1000,-600,-1,0,1,600,1000,10000]))+[(1,0),(-10001,0),(0,-10001),(0,10001)]
commands=b''.join(struct.pack('<2i',*v) for v in cases)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--device-gains'],input=commands)
assert len(pc)==len(cases)*12
for i,(volume,pan) in enumerate(cases):
 x.mem_write(base,struct.pack('<2f',123,456));x.mem_write(stack,struct.pack('<IiiI',stop,volume,pan,base))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 xbox=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,8))
 for label,result in [('PC',pc[i*12:i*12+12]),('NXDK',xbox)]:
  status,left,right=struct.unpack('<i2f',result)
  if volume>0 or volume < -10000 or abs(pan)>10000:assert (status,left,right)==(-4,123,456)
  else:
   expected=[10**((volume-max(pan,0))/2000),10**((volume+min(pan,0))/2000)]
   assert status==0 and all(math.isclose(a,b,rel_tol=2e-7,abs_tol=1e-12) for a,b in zip([left,right],expected)),(label,volume,pan,result,expected)
report=dict(result='PASS',cases=len(cases),scope='PC/NXDK mathematical amplitude adapter within2e-7 relative tolerance; invalid ranges preserve output. No original device measurement or native DSP calibration.')
(root/'artifacts/audio-device-gains.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
