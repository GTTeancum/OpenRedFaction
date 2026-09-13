"""Unhooked401cc0 versus shared PC/NXDK scalar selection, including override bits."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,0x10000);STACK=B+0xe000;STOP=B+0xf000;STUB=STOP+0x100;OUT=B+0x1000
u.mem_write(STUB,b'\xd9\x1d'+w(OUT)+b'\x83\xc4\x08\xc3')
rng=random.Random(0x401cc0);records=[];commands=[];expected=[];overrides=0;defaults=0
choices=[0,0x80000000,0x3f000000,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc12345]
for case in range(2048):
 weapons=[rng.choice((-7,-1,0,1,2,63)) for _ in range(2)];flag=rng.choice((0,1,2,256,257));mode=rng.choice((0,1,2,256,257));value=rng.choice(choices);table=[rng.choice(choices) for _ in range(64)]
 seed=bytearray(rng.randbytes(0x540));seed[4:12]=w(*weapons);seed[0x528]=flag&255;seed[0x52c:0x530]=w(value);u.mem_write(B,bytes(seed))
 for i,v in enumerate(table):u.mem_write(0x85d21c+i*0x550,w(v))
 u.mem_write(STACK,w(STUB,B,mode,STOP));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x401cc0,STOP,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and bytes(u.mem_read(B,len(seed)))==seed
 result=struct.unpack('<I',u.mem_read(OUT,4))[0];selected=weapons[0 if mode&255 else 1]
 want=value if flag&255==1 else table[selected] if selected>0 else 0x3f000000
 assert result==want,(case,hex(result),hex(want));overrides+=int(flag&255==1);defaults+=int(flag&255!=1 and selected<=0)
 command=w(*weapons,flag,value,mode,64,*table);commands.append(command);expected.append(w(0,want));records.append(command)
# Added bounds checks preserve output, while bypassed malformed table indexes remain irrelevant.
for weapons,flag,mode,count,status in [([64,1],0,1,64,-4),([1,64],0,0,64,-4),([1,1],0,1,0,-4),([64,64],1,0,0,0),([0,0],0,1,0,0)]:
 commands.append(w(*weapons,flag,0x3f800000,mode,count,*([0]*64)));expected.append(w(status,0x12345678 if status else 0x3f800000 if flag else 0x3f000000))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-weapon-limit'],input=b''.join(commands));assert actual==b''.join(expected),'PC'
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im);x.mem_map(B,0x10000)
entry=int(re.search(r'\s_rf_entity_ai_weapon_limit\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);TABLE=B+0x2000
for i,command in enumerate(commands):
 v=struct.unpack('<70I',command);x.mem_write(B,w(*v[:2]));x.mem_write(TABLE,w(*v[6:]));x.mem_write(OUT,w(0x12345678));x.mem_write(STACK,w(STOP,B,v[2],v[3],v[4],TABLE,v[5],OUT));x.reg_write(UC_X86_REG_ESP,STACK)
 x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP;actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,4));assert actual==expected[i],('NXDK',i)
report=dict(result='PASS',original_pc_nxdk_cases=2048,overrides=overrides,defaults=defaults,bounds_guards=5,scope='Full unhooked401cc0; primary/secondary low-byte selection, exact override byte1, fallback for IDs<=0, table stride550 and exact float bits including quiet NaN/infinity/signed zero. Shared scalar column is caller-supplied; authored table retention and occupant-chain4077a0 remain separate.')
(root/'artifacts/ai-weapon-limit.json').write_text(json.dumps(report,indent=2));print(report)
