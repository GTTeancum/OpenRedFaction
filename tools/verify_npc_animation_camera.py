"""Animation gate with original camera metric and entity flag predicate intact."""
import json,random,re,struct,subprocess
import verify_npc_animation_gate as gate
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
root=gate.root;base=gate.base;stack=gate.stack;stop=gate.stop;pack=gate.pack
u=gate.load(gate.exe);nx=gate.nx;advanced=False

def hook(uc,address,size,context):
 global advanced
 if address!=0x503360:return
 advanced=True;sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0]
 uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x5479b0);fixtures=[];metric_inputs=[]
for k in range(1200):
 mode=0x66 if k%7 else 0
 point=[rng.randint(-400,400)/8 for _ in range(3)];camera=[rng.randint(-120,120)/8 for _ in range(3)]
 numerator=rng.choice([.5,1.,1.25,2.]);denominator=rng.choice([.5,1.,1.5,2.])
 # This metric is just above45 but rounds to45 in binary32: regression for
 # the premature float conversion in the first gate interface.
 if k%20==0:point=[45.,.001,0.];camera=[0.,0.,0.];numerator=denominator=1.;mode=0x66
 actor_flags=rng.choice([0,1,0x80000000,0x40000000]);state=rng.choice([0,1,2,13]);count=rng.choice([0,2,3,4]);room_flag=rng.randrange(2);flags=rng.choice([0,0x80000000])
 if k%20==0:actor_flags=0;state=0;count=3;room_flag=1;flags=0
 fixtures.append((mode,point,camera,numerator,denominator,actor_flags,state,count,room_flag,flags))
 metric_inputs.append(struct.pack('<3fII4iI8f',10.,30.,60.,3,0,0,0,0,1,mode,*point,*camera,numerator,denominator))
metrics=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--select-lod-camera'],input=b''.join(metric_inputs))
assert len(metrics)==len(fixtures)*16
raws=[]
for k,f in enumerate(fixtures):
 mode,point,camera,num,den,actor_flags,state,count,room_flag,flags=f
 status,_,metric=struct.unpack_from('<iId',metrics,k*16);assert status==0
 raws.append(struct.pack('<5I2iId',1,2,1,room_flag,flags,state,count,actor_flags&1,metric))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--animation-gate'],input=b''.join(raws))
assert len(pc)==len(fixtures)*4
metric_entry=int(re.search(r'\s_rf_model_lod_metric\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
regressions=0;advances=0
for k,f in enumerate(fixtures):
 mode,point,camera,num,den,actor_flags,state,count,room_flag,flags=f
 u.mem_write(base,bytes(0x5000));u.mem_write(base,pack(base+0x2000));u.mem_write(base+0x80,pack(base+0x4000));u.mem_write(base+0x4000,pack(2,base+0x5000))
 u.mem_write(base+0x2160,bytes([room_flag]));u.mem_write(base+0x810,pack(actor_flags,flags));u.mem_write(base+0x520,pack(state));u.mem_write(base+0x294,pack(base+0x3000));u.mem_write(base+0x43b8,pack(count))
 u.mem_write(base+0x3c,struct.pack('<3f',*point));u.mem_write(0x1818680,struct.pack('<3f',*camera));u.mem_write(0x1818b50,struct.pack('<f',num));u.mem_write(0x1818b48,struct.pack('<f',den));u.mem_write(0x17c7bcc,pack(mode));u.mem_write(0x6fc4d8,b'\0')
 advanced=False;u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x41dbea,0x41dd49,count=1000)
 assert u.reg_read(UC_X86_REG_EIP)==0x41dd49 and u.reg_read(UC_X86_REG_ESP)==stack-4
 assert pc[k*4:k*4+4]==pack(advanced),('PC',k)
 nx.mem_write(base,raws[k]);nx.mem_write(base+0x100,struct.pack('<6f',*point,*camera))
 nx.mem_write(stack,pack(stop,mode,base+0x100,base+0x10c)+struct.pack('<2f',num,den)+pack(base+32));nx.reg_write(UC_X86_REG_ESP,stack)
 nx.emu_start(metric_entry,stop,count=1000);assert nx.reg_read(UC_X86_REG_EIP)==stop and nx.reg_read(UC_X86_REG_EAX)==0
 nx.mem_write(stack,pack(stop,base));nx.reg_write(UC_X86_REG_ESP,stack);nx.emu_start(gate.entry,stop,count=500)
 assert nx.reg_read(UC_X86_REG_EIP)==stop and nx.reg_read(UC_X86_REG_EAX)==int(advanced),('NXDK',k)
 if k%20==0:assert not advanced;regressions+=1
 advances+=advanced
report=dict(result='PASS',cases=len(fixtures),advances=advances,float_rounding_regressions=regressions,scope='Prepared41dbea..41dd49 with original5182f0/5479b0/vector distance chain and427020 intact; only503360 dispatch observed. PC/NXDK composed metric and gate match these finite cases. Alternate-view path and live actor ownership remain open; double approximation is not universal x87 equivalence.')
(root/'artifacts/npc-animation-camera.json').write_text(json.dumps(report,indent=2));print(report)
