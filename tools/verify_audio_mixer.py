"""Shared fixed-memory output adapter against an independent integer reference."""
import json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);b=0x30000000;x.mem_map(b,65536);stack=b+60000;stop=b+64000
mapping=(root/'build/xbox/main.map').read_text();names=('rf_audio_mixer_init','rf_audio_voice_start','rf_audio_mix','rf_audio_voice_stop')
entries={n:int(re.search('_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for n in names}
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
def call(name,args,expected=0):
 x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[name],stop,count=10000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 if name!='rf_audio_mixer_init':assert x.reg_read(UC_X86_REG_EAX)==(expected&0xffffffff),name
trunc=lambda n,d:-(abs(n)//d) if n<0 else n//d
rng=random.Random(48000);commands=bytearray();expected=bytearray();clipped=0
for case in range(144):
 channels=1+case%2;bits=(8,16)[case//2%2];frames=(1,3,31)[case//4%3];rate=(8000,11025,22050,48000,96000,192000)[case//3%6];gain=(0,12345,32768)[case//5%3];loop=case%2;voices=(1,2,30)[case//7%3];count=257
 values=[rng.randrange(256) if bits==8 else rng.randrange(-32768,32768) for i in range(frames*channels)]
 raw=bytes(values) if bits==8 else struct.pack('<'+'h'*len(values),*values)
 samples=[(v-128)*256 for v in values] if bits==8 else values
 header=[len(raw),frames,rate,channels,bits,gain,loop,count,voices];commands.extend(w(*header)+raw);pcm=bytearray()
 for f in range(count):
  frame,phase=divmod(f*rate,48000)
  for c in range(2):
   if not loop and frame>=frames:v=0
   else:
    frame_index=frame%frames;next_index=(frame_index+1)%frames if loop else min(frame_index+1,frames-1)
    a=samples[frame_index*channels+(c if channels==2 else 0)];z=samples[next_index*channels+(c if channels==2 else 0)]
    v=trunc(a*(48000-phase)+z*phase,48000);v=trunc(v*(gain if c==0 else 32768),32768)*voices
   clipped+=v<-32768 or v>32767;pcm.extend(struct.pack('<h',max(-32768,min(32767,v))))
 expected.extend(pcm);call('rf_audio_mixer_init',[b]);x.mem_write(b+4096,raw);x.mem_write(b+3000,w(b+4096,len(raw),frames,rate,channels,bits))
 for v in range(voices):call('rf_audio_voice_start',[b,b+3000,gain,32768,loop,b+3500])
 call('rf_audio_mix',[b,b+8192,count]);assert bytes(x.mem_read(b+8192,count*4))==pcm,case
 before=bytes(x.mem_read(b,1564));handle=struct.unpack_from('<I',before,24)[0]
 call('rf_audio_voice_stop',[b,handle]);after=bytes(x.mem_read(b,1564))
 assert after==bytes(52)+before[52:],case
 for stale in (0,handle):
  call('rf_audio_voice_stop',[b,stale],-3);assert bytes(x.mem_read(b,1564))==after

actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--mix'],input=commands);assert actual==expected
report=dict(result='PASS',cases=144,stereo_frames=144*257,clipped_samples=clipped,mixer_bytes_x86=1564,scope='Independent integer reference equals PC chunked17-frame and NXDK single-call renders. PCM8/16 mono/stereo, six rates, gains, loops, natural completion and1/2/30 voices. PC/NXDK check stop cleanup after active/natural completion and stale handles; NXDK checks complete borrower clearing and untouched neighbors. PC additionally checks full-pool rejection. Output adapter only; no original Miles equivalence, hardware playback, spatial attenuation or live thread ownership.')
(root/'artifacts/audio-mixer-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
