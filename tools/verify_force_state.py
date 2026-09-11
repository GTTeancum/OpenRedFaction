"""Original force state handlers vs PC/NXDK, all callees unchanged."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_physics_forces_set_state\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x4b9330);commands=bytearray();expected=bytearray()
for case in range(1024):
    count=case%4;uid_count=(case//4)%5;action=(case//20)%2
    records=[]
    for i in range(3):
        record=bytearray(rng.randbytes(108));record[4:8]=w((0,42,42,0xffffffff)[(case//40+i)%4]);records.append(bytes(record))
    data=b''.join(records);uids=[(0,42,0xffffffff,777)[(case//80+i)%4] for i in range(4)]
    u.mem_write(base,data);u.mem_write(base+0x1000,w(*uids));u.mem_write(base+0x2000,w(base,base+108,base+216))
    u.mem_write(0x6460bc,w(count,3,base+0x2000));u.mem_write(base+0x329c,w(uid_count,4,base+0x1000))
    u.mem_write(stack,w(stop,base+0x3000));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x4b9330 if action else 0x4ba130,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    result=bytes(u.mem_read(base,len(data)))
    # Independent write-set check: first duplicate only, every other byte preserved.
    oracle=bytearray(data)
    for uid in uids[:uid_count]:
        for i in range(count):
            if struct.unpack_from('<I',data,i*108+4)[0]==uid:
                oracle[i*108+104]=action;break
    assert result==oracle,case
    assert bytes(u.mem_read(base+0x1000,16))==w(*uids)
    commands.extend(data+w(*uids,count,uid_count,action));expected.extend(w(0)+result)
    x.mem_write(base,data);x.mem_write(base+0x1000,w(*uids))
    x.mem_write(stack,w(stop,base,count,base+0x1000,uid_count,action));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(base,len(data)))==result,case
    assert bytes(x.mem_read(base+0x1000,16))==w(*uids)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-state'],input=commands)
assert actual==expected,'PC force state differs'
report=dict(result='PASS',cases=1024,original_sha256=digest,scope='Complete original4b9330/4ba130 including45d6d0 UID lookup and list callees unchanged. PC/NXDK entire records agree; only first matching low activation byte changes. Empty collections/links, duplicates, missing IDs, zero and UINT32_MAX IDs, padded activation bytes. Event dispatch integration remains separate.')
(root/'artifacts/force-state.json').write_text(json.dumps(report,indent=2));print(report)
