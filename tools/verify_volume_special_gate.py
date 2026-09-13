"""Original special-owner volume gate and real camera/actor predicates."""
import runpy,struct,re,subprocess,json,itertools
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,B,S,STOP=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(int(v)&0xffffffff for v in v))
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
entry=int(re.search(r'_rf_glare_volume_special_allowed\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
VIEW=B+0x3000;CAM=B+0x4000;ACTOR=B+0x5000;CB=STOP+16;calls={u:0,x:0};case=None
def hook(m,a,size,ctx):
 if m is u and a==STOP:m.emu_stop();return
 if a!=(0x426fc0 if m is u else CB):return
 sp=m.reg_read(UC_X86_REG_ESP);calls[m]+=1
 if m is u:assert r(m,sp+4)==32;result=ACTOR if case[3] else 0
 else:
  assert r(m,sp+4)==77 and r(m,sp+8)==32
  m.mem_write(r(m,sp+12),w(case[3]));m.mem_write(r(m,sp+16),w(case[4]));result=-1 if case[5] else 0
 m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_EIP,r(m,sp));m.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
cases=[[*v,0] for v in itertools.product((0,1,2),(0,1,0xffffffff),(0,2,0xfffffffd),(0,1),(0,1,2))]
cases += [[1,0,0,present,flags,1] for present,flags in itertools.product((0,1),(0,1,2))]
actual=subprocess.check_output([str(c['probe']),'--volume-special-gate'],input=b''.join(w(*v) for v in cases));assert len(actual)==12*len(cases)
for n,case in enumerate(cases):
 calls[u]=calls[x]=0;u.mem_write(B,bytes(768));u.mem_write(B+0x2cc,w(0 if case[0]==0 else VIEW if case[0]==1 else VIEW+0x100));u.mem_write(B+0x7c,w(case[2]));u.mem_write(0x7c763c,w(VIEW));u.mem_write(VIEW+0xc4,w(CAM));u.mem_write(VIEW+0x14,w(32));u.mem_write(CAM+8,w(case[1]));u.mem_write(ACTOR+0x810,w(case[4]));u.mem_write(S,w(STOP,B));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4141a0,0x414200,count=10000)
 end=u.reg_read(UC_X86_REG_EIP);assert end in (STOP,0x414200)
 expected=w(-1 if case[5] else 0,0xa5a5a5a5 if case[5] else end==0x414200,calls[u])
 x.mem_write(B,bytes(528));x.mem_write(B+72,w(case[0]));x.mem_write(B+120,w(case[2]));x.mem_write(B+0x2000,w(0xa5a5a5a5));x.mem_write(S,w(STOP,B,1,case[1],32,CB,77,B+0x2000));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP
 got=w(x.reg_read(UC_X86_REG_EAX),r(x,B+0x2000),calls[x]);assert got==expected and actual[n*12:(n+1)*12]==expected,(n,case,got.hex(),expected.hex())
report=dict(result='PASS',original_cases=len(cases)-6,callback_failures=6,scope='Original4141a0..414200/return, actual40d740 camera-kind read and427020 actor810 predicate; only426fc0 lookup supplied. Exact PC/NXDK allow result and lookup count. Callback failures preserve output. Current view/camera ownership and scene special-owner binding excluded.')
(root/'artifacts/volume-special-gate.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
