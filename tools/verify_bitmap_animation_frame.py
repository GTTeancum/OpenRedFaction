"""Original50f9cd rate conversion,50f500 clock and normal50f440 auto-binding."""
import runpy,struct,random,re,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBP
entry=int(re.search(r'_rf_bitmap_animation_frame\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
table=base+0x1000;slot=7;u.mem_write(0x17c80c4,w(table));u.mem_write(0x5a4554,w(1000));u.mem_write(table+slot*108+56,w(1))
fixtures=[]
for fps in (0,1,15,24,30,60,255):
 for count in (1,2,5,16,255):
  for loop in (0,1,2,3,0xffffffff):
   for elapsed in (0,1,66,67,133,134,333,334,1000,65535):fixtures.append((elapsed,0,fps,count,loop))
rng=random.Random(0x50f500)
for i in range(1024):
 started=rng.getrandbits(32);delta=rng.randrange(1000000);fixtures.append(((started+delta)&0xffffffff,started,rng.randrange(1,256),rng.randrange(1,256),i%4))
commands=bytearray();results=bytearray();auto=0;frames=set()
for values in fixtures:
 now,started,fps,count,loop=values;command=w(*values);commands.extend(command)
 # Execute the loader's original integer-fps to float frames/ms conversion.
 u.mem_write(stack+0x34,w(fps));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBP,table+slot*108)
 u.emu_start(0x50f9cd,0x50f9da,count=100);rate=bytes(u.mem_read(table+slot*108+76,4))
 u.mem_write(table+slot*108+67,bytes([count]));u.mem_write(0x1754610,w(now))
 u.mem_write(stack,w(stop,slot,started)+rate+w(loop));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x50f500,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 raw=u.reg_read(UC_X86_REG_EAX);frame=-1 if raw==0xffffffff else raw-slot-1;result=struct.pack('<ii',0,frame);results.extend(result);frames.add(frame)
 if started==0 and loop==1:
  u.mem_write(stack,w(stop,slot));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x50f440,stop,count=10000)
  assert u.reg_read(UC_X86_REG_EAX)==raw;auto+=1
 x.mem_write(base+256,w(0x12345678));x.mem_write(stack,w(stop,*values,base+256));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+256,4));assert actual==result,(values,actual,result)
actual=subprocess.check_output([str(c['probe']),'--bitmap-animation-frame'],input=commands);assert actual==results
bad=[(0,0,15,0,1),(0,0,15,256,1),(0,0,0x80000000,5,1),(0xffffffff,0,1000,5,1)]
for values in bad:
 x.mem_write(base+256,w(0x12345678));x.mem_write(stack,w(stop,*values,base+256));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(base+256,4))==w(0x12345678)
actual=subprocess.check_output([str(c['probe']),'--bitmap-animation-frame'],input=b''.join(w(*v) for v in bad));assert actual==(w(0xfffffffc,0x12345678))*len(bad)
report=dict(result='PASS',cases=len(fixtures),auto_bindings=auto,guards=len(bad),distinct_frames=len(frames),scope='Unhooked original rate conversion and full50f500, plus normal-handle50f440 auto selection. Exact PC/NXDK zero-based frame and completion; separate clock-instance handles, GPU binding and global clock scheduling excluded.')
(root/'artifacts/bitmap-animation-frame.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
