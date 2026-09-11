"""Full original ordinary-SP player detach with real helper and phase queries."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;actor=b+0x2000;registry=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,im);m.mem_map(b,65536);return m
def run(m,a,args):
    m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(a,stop,count=10000)
    assert m.reg_read(UC_X86_REG_EIP)==stop;return m.reg_read(UC_X86_REG_EAX)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary);mapping=(root/'build/xbox/main.map').read_text()
addresses=[int(re.search(r'\s_'+s+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for s in ('rf_player_detach_sp','rf_player_is_dead','rf_player_is_dying')]
u.mem_write(0x64ecb9,b'\0');u.mem_write(0x6fc4d8,b'\0')
rng=random.Random(0x4a6d50);inputs=[];outputs=[]
for case in range(4096):
    handle=rng.choice((0,1023,0x10007,0x80000007,0xffffffff));flags=rng.getrandbits(32);activity=rng.getrandbits(32)
    owner=bytearray(rng.randbytes(0x1300));owner[0x14:0x18]=w(handle);owner[0xfb0:0xfb4]=w(activity)
    body=bytearray(rng.randbytes(0x1500));body[0x24:0x28]=w(0);body[0x2c:0x30]=w(handle);body[0x810:0x814]=w(flags)
    u.mem_write(b,bytes(owner));u.mem_write(actor,bytes(body));u.mem_write(0x7394cc,bytes(4096))
    x.mem_write(b,w(handle,activity));x.mem_write(actor,w(handle,0,0,0,flags)+bytes(36));x.mem_write(registry,bytes(4096))
    if handle!=0xffffffff:
        u.mem_write(0x7394cc+4*(handle&0xffff),w(actor));x.mem_write(registry+4*(handle&0xffff),w(actor))
    old_table=bytes(u.mem_read(0x7394cc,4096));old_xactors=bytes(x.mem_read(actor,0x4000))
    before=[run(u,a,[b])&255 for a in (0x4a4920,0x4a4940)]
    assert before==[run(x,a,[registry,b]) for a in addresses[1:]]
    run(u,0x4a6d50,[b]);run(x,addresses[0],[b,b+4])
    owner[0x14:0x18]=w(-1);owner[0xfb0]=0
    assert bytes(u.mem_read(b,len(owner)))==owner
    assert bytes(u.mem_read(actor,len(body)))==body and bytes(u.mem_read(0x7394cc,4096))==old_table
    assert bytes(x.mem_read(actor,0x4000))==old_xactors
    final=w(-1,activity&0xffffff00);assert bytes(x.mem_read(b,8))==final
    assert [run(u,a,[b])&255 for a in (0x4a4920,0x4a4940)]==[1,0]
    assert [run(x,a,[registry,b]) for a in addresses[1:]]==[1,0]
    inputs.append(w(handle,activity));outputs.append(final)
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--player-detach'],input=b''.join(inputs))==b''.join(outputs)
report=dict(result='PASS',cases=4096,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full4a6d50 ordinary SP with real4a6e00 and original dead/dying handle queries, no replaced callees. PC/NXDK handle and byte writes match; adjacent bytes, retained actor and registry remain unchanged. Post-detach link is dead/not dying. Alternate mode, multiplayer, camera and live finalization integration excluded.')
(root/'artifacts/player-detach.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
