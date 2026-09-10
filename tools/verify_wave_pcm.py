"""Bounded PCM view adapter against authored WAV metadata and malformed inputs."""
import io,json,re,struct,subprocess,sys,wave,hashlib
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;x.mem_map(b,2*1024*1024);data=b+4096;output=b+0x1fc000;stack=b+0x1fe000;stop=b+0x1ff000
entry=int(re.search(r'_rf_wave_pcm_parse\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
inv=json.loads((root/'artifacts/inventory.json').read_text());cases=[];resources=[]
for name in ('DoorOpen_07.wav','DoorEnd_07.wav'):
 archive=next(a for a in inv['files'] if a['path']=='audio.vpp');e=next(e for e in archive['vpp']['entries'] if e['name']==name)
 with (root/'Installed_Game/audio.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
 with wave.open(io.BytesIO(raw)) as f:rate=f.getframerate();channels=f.getnchannels();bits=f.getsampwidth()*8;frames=f.getnframes();pcm=f.readframes(frames)
 at=12
 while raw[at:at+4]!=b'data':n=struct.unpack_from('<I',raw,at+4)[0];at+=8+n+(n&1)
 expected=[0,at+8,len(pcm),frames,rate,channels,bits];cases.append((raw,expected))
 resources.append(dict(name=name,file_bytes=len(raw),pcm_bytes=len(pcm),frames=frames,rate=rate,channels=channels,bits=bits,sha256=hashlib.sha256(raw).hexdigest()))
def chunk(tag,payload):return tag+w(len(payload))+payload+(b'\0' if len(payload)&1 else b'')
def riff(chunks):payload=b'WAVE'+chunks;return b'RIFF'+w(len(payload))+payload
for channels in (1,2):
 for bits in (8,16):
  align=channels*bits//8;fmt=struct.pack('<HHIIHH',1,channels,22050,22050*align,align,bits);pcm=bytes(range(3*align))
  raw=riff(chunk(b'JUNK',b'x')+chunk(b'data',pcm)+chunk(b'fmt ',fmt));cases.append((raw,[0,30,len(pcm),3,22050,channels,bits]))
valid=cases[-1][0]
for n in range(len(valid)):cases.append((valid[:n],[-2,0,0,0,0,0,0]))
fmt=struct.pack('<HHIIHH',1,1,11025,22050,2,16)
for offset,raw_value in [(0,3),(2,0),(12,4),(14,24)]:
 bad=bytearray(fmt);struct.pack_into('<H',bad,offset,raw_value);cases.append((riff(chunk(b'fmt ',bad)+chunk(b'data',bytes(2))),[-2,0,0,0,0,0,0]))
for chunks in (chunk(b'data',b'xx'),chunk(b'fmt ',fmt),chunk(b'fmt ',fmt)*2+chunk(b'data',b'xx'),chunk(b'fmt ',fmt)+chunk(b'data',b'xx')*2,chunk(b'fmt ',fmt)+chunk(b'data',b'x')):cases.append((riff(chunks),[-2,0,0,0,0,0,0]))
commands=bytearray();expected=bytearray()
for n,(raw,want) in enumerate(cases):
 commands.extend(w(len(raw))+raw);expected.extend(w(*want));x.mem_write(data,raw);x.mem_write(output,bytes([0xa5])*24)
 x.mem_write(stack,w(stop,data,len(raw),output));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 status=x.reg_read(UC_X86_REG_EAX);view=bytes(x.mem_read(output,24))
 if want[0]:assert status==0xfffffffe and view==bytes([0xa5])*24,n
 else:
  values=list(struct.unpack('<6I',view));values[0]-=data;assert [status]+values==want,(n,values,want)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe')],input=commands);assert pc==expected
loop=[(a['path'],e['name']) for a in inv['files'] if 'vpp' in a for e in a['vpp']['entries'] if e['name'].lower()=='doorloop_2.5.wav'];assert not loop
report=dict(result='PASS',cases=len(cases),resources=resources,pcm_bytes=sum(r['pcm_bytes'] for r in resources),missing_exact_loop_reference='DoorLoop_2.5.wav',scope='PC/NXDK bounded RIFF PCM view, Python wave metadata/bytes for two authored files, synthetic PCM8/16 mono/stereo, odd chunks, data-before-format, truncation and malformed formats. Errors preserve output. No original decoder equivalence, sample-name fallback, mixer, playback or live Xbox audio proof.')
(root/'artifacts/wave-pcm-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
