"""Original522683 successful identity assignment versus NXDK port binding.
The original device creation/play operations preceding this block are excluded.
"""
import hashlib,json,re,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
b=0x30000000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_audio_voice_ids_bind\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=0
for slot in range(30):
 for sequence in (0,1,29,65535,0x7ffffffe,0x7fffffff):
  for generation in (1,32767,32768,65534):
   source=(generation<<16)|slot
   u.mem_write(0x1aed358,w(sequence));u.reg_write(UC_X86_REG_ESI,slot*44)
   u.emu_start(0x522683,0x5226a6,count=100)
   original=bytes(u.mem_read(0x1ad7530+slot*44,4))+bytes(u.mem_read(0x1aed358,4))
   raw=w(*([0xffffffff]*30),*([0]*30),sequence);x.mem_write(b,raw)
   x.mem_write(b+0x1000,w(0xabcdef));x.mem_write(b+0xe000,w(b+0xf000,b,source,b+0x1000))
   x.reg_write(UC_X86_REG_ESP,b+0xe000);x.emu_start(entry,b+0xf000,count=10000)
   assert x.reg_read(UC_X86_REG_EAX)==0
   got=bytes(x.mem_read(b+0x1000,4))+bytes(x.mem_read(b+240,4));assert got==original
   want=bytearray(raw);want[slot*4:slot*4+4]=original[:4];want[120+slot*4:124+slot*4]=w(source);want[240:244]=original[4:]
   assert bytes(x.mem_read(b,244))==want
   cases+=1
report=dict(result='PASS',cases=cases,original_sha256=digest,
 scope='Actual original522683..5226a6 success identity assignment versus NXDK-linked bind across all30 slots, sequence wrap and unsigned mixer sign boundary. Full device start/lookup and live campaign excluded. Port identity lifecycle separately tested by audio_device_identity CTest.')
(root/'artifacts/audio-device-identity.json').write_text(json.dumps(report,indent=2));print(report)
