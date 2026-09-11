"""Original collision-pair retirement with all real callees versus PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
b=0x30000000;nodes=b+0x1000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096)
    m.mem_write(base,im);m.mem_map(b,65536);return m
def run(m,address,args):
    m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack)
    m.emu_start(address,stop,count=10000);assert m.reg_read(UC_X86_REG_EIP)==stop
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);x=machine(binary)
entry=int(re.search(r'\s_rf_collision_pairs_retire\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x48c9f0);commands=[];expected=[];retired=0
index=lambda address: (address-nodes)//64 if address else 0xffffffff
for case in range(4096):
    order=list(range(32));rng.shuffle(order);n=case%33;f=rng.randrange(33-n)
    active=order[:n];free=order[n:n+f];actor=rng.choice((0,0x12345678,0xffffffff))
    body=bytearray(rng.randbytes(2048))
    for i in range(32):
        body[i*64:i*64+12]=w(0,rng.choice((actor,0x87654321,0xdeadbeef)),rng.choice((actor,0xabcdef01)))
    for chain in (active,free):
        for k,i in enumerate(chain):body[i*64:i*64+4]=w(nodes+chain[k+1]*64 if k+1<len(chain) else 0)
    # Normal counters plus wraparound fixtures: original performs unsigned updates.
    ac=n if case%4 else rng.choice((0,0xffffffff));fc=f if case%4 else 0xffffffff
    heads=w(nodes+active[0]*64 if active else 0,ac,nodes+free[0]*64 if free else 0,fc)
    def wire(header,payload):
        h=list(struct.unpack('<4I',header));out=[index(h[0]),h[1],index(h[2]),h[3],actor]
        for i in range(32):
            a,c,d=struct.unpack('<3I',payload[i*64:i*64+12]);out.extend((index(a),c,d))
        return w(*out)
    commands.append(wire(heads,body));u.mem_write(nodes,bytes(body));x.mem_write(nodes,bytes(body))
    u.mem_write(0x73db28,heads[:8]);u.mem_write(0x75db30,heads[8:]);x.mem_write(b,heads)
    run(u,0x48c9f0,[actor]);run(x,entry,[b,b+8,actor])
    result=bytes(u.mem_read(nodes,2048));header=bytes(u.mem_read(0x73db28,8))+bytes(u.mem_read(0x75db30,8))
    assert bytes(x.mem_read(nodes,2048))==result and bytes(x.mem_read(b,16))==header,case
    removed=sum(actor in struct.unpack('<2I',body[i*64+4:i*64+12]) for i in active);retired+=removed
    assert struct.unpack('<I',header[4:8])[0]==(ac-removed)&0xffffffff
    assert struct.unpack('<I',header[12:16])[0]==(fc+removed)&0xffffffff
    for i in range(32):assert result[i*64+4:(i+1)*64]==body[i*64+4:(i+1)*64]
    expected.append(wire(header,result))
assert retired>0
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--collision-retire'],input=b''.join(commands))==b''.join(expected)
report=dict(result='PASS',cases=4096,retired=retired,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full original48c9f0 with real40a490/48ccf0/48cc90/48ccb0/48cc70; no substituted callees. Empty, head, consecutive, middle, tail and either-endpoint removals; existing free lists and uint32 counter wrap. Exact PC/NXDK list topology, counts and endpoint identities; NXDK/original full node payload unchanged. No live collision-pair creation or death lifecycle activation.')
(root/'artifacts/collision-retire.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
