"""Compare the registry mutation/search block; resource resolution/loading excluded."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EFLAGS
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);obj=base;esp=base+32000
def stop_before_load(uc,address,size,user):
    if address==0x51cc93:uc.emu_stop()
u.hook_add(UC_HOOK_CODE,stop_before_load,begin=0x51cc93,end=0x51cc93)
rng=random.Random(0x51cc42);records=[];expected=[];hits=appends=guards=0
for n in range(2400):
    count=rng.randrange(33);capacity=32;identity=rng.randrange(1,17);flag=rng.randrange(256)
    identities=[rng.randrange(1,17) for _ in range(32)];flags=[rng.randrange(256) for _ in range(32)]
    if n%3==0 and count:
        slot=rng.randrange(count);identity=identities[slot];flag=flags[slot]
    if n%20==0:identity=0
    if n%31==0:capacity=max(0,count-1)
    if n%37==0:capacity=0xffffffff
    records.append(struct.pack('<36I32B',count,capacity,identity,flag,*identities,*flags))
    match=next((i for i in range(count) if identities[i]==identity and flags[i]==flag),None)
    if not identity or count>capacity or capacity>0x7fffffff or (match is None and count==capacity):
        expected.append(struct.pack('<iiiI32I32B',-4,-12345,-12345,count,*identities,*flags));guards+=1;continue
    u.mem_write(obj+0xf58,struct.pack('<I32I',count,*identities));u.mem_write(obj+0x120c,bytes(flags))
    u.mem_write(esp+0x18,struct.pack('<I',flag));u.reg_write(UC_X86_REG_ESP,esp)
    u.reg_write(UC_X86_REG_ESI,obj);u.reg_write(UC_X86_REG_EBP,identity);u.reg_write(UC_X86_REG_EFLAGS,2)
    u.emu_start(0x51cc42,0x51cca2,count=10000)
    endpoint=u.reg_read(UC_X86_REG_EIP);assert endpoint in (0x51cc93,0x51cca2)
    updated=struct.unpack('<I',u.mem_read(obj+0xf58,4))[0]
    added=int(endpoint==0x51cc93);index=updated-1 if added else u.reg_read(UC_X86_REG_EAX)
    appends+=added;hits+=1-added
    expected.append(struct.pack('<iiiI',0,index,added,updated)+bytes(u.mem_read(obj+0xf5c,128))+bytes(u.mem_read(obj+0x120c,32)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--register-motion'],input=b''.join(records))
assert len(actual)==len(expected)*176
for n,want in enumerate(expected):assert actual[n*176:(n+1)*176]==want,(n,actual[n*176:(n+1)*176].hex(),want.hex())
report=dict(result='PASS',original_cases=hits+appends,reused=hits,appended=appends,port_guards=guards,
    scope='Original 0x51cc42 search and append through 0x51cc93; exact arrays/flags/count and reused index. New index inferred as post-load count minus one. Skeleton resolution, lazy load and failure behavior excluded; boundary hook stops before load call without replacing instructions.')
(root/'artifacts/motion-registration-verification.json').write_text(json.dumps(report,indent=2));print(report)
