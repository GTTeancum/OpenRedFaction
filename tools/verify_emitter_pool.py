"""Shared fixed emitter pool versus the full original lifecycle replay."""
import runpy,struct,re,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_emitter_pool_trace.py')))
x=c['c']['c']['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
symbols=(root/'build/xbox/main.map').read_text()
def entry(name):return int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
def call(name,*args):
    x.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry(name),stop,count=5000000);assert x.reg_read(UC_X86_REG_EIP)==stop
    return x.reg_read(UC_X86_REG_EAX)
x.mem_map(base+0x10000,0x40000)
records=base+0x10000;slots=base+0x40000;pool=base+0x9000;particles=base+0x9100;lists=base+0xa000
assert call('rf_particle_pool_init',particles,records,lists,133)==0
assert call('rf_emitter_pool_init',pool,slots,particles)==0
source=base+0x400;random=base+0x200;output=base+0x204
x.mem_write(source,bytes(c['raw']));x.mem_write(random,struct.pack('<I',123))
free=list(range(128));active=[];created=0;last_particle={};commands=bytearray();expected=bytearray()
def translate_particle(a):
    if 0x782748<=a<0x7a2ae8:return 500+(a-0x782748)//120
    if a==0x7bd670:return 1604
    if a==0x7a3c04:return 1601
    return 1605+(a-0xb0-0x7b2a70)//344
def compact(raw):return raw[4:0x9c]+raw[0x154:0x158]+raw[0x128:0x138]+raw[0x140:0x144]+raw[0x13c:0x140]+raw[0x138:0x13c]
for event in c['events']:
    release=event['operation']=='release';exhausted=event['operation']=='exhausted'
    slot=event.get('slot',0);now=event.get('now',2000)
    template=bytes.fromhex(event['source']) if 'source' in event else bytes(c['raw'])
    x.mem_write(source,template)
    commands.extend(struct.pack('<3I',release,slot,now)+template)
    x.mem_write(output,struct.pack('<I',0xffffffff))
    if release:
        address=slots+slot*228
        x.mem_write(address+148,struct.pack('<I',0x11223344))
        x.mem_write(address+188,struct.pack('<3f',1.25,-2.5,7));x.mem_write(address+176,struct.pack('<2f',0.125,0.75))
        status=call('rf_emitter_pool_release',pool,slot);active.remove(slot);free.append(slot)
    else:
        status=call('rf_emitter_pool_create',pool,source,0xffffffff,0x2468,0,now,0,random,output)
        if not exhausted:
            assert free.pop(0)==slot and struct.unpack('<I',x.mem_read(output,4))[0]==slot
            active.append(slot);created+=1
    assert status==(0xfffffffd if exhausted else 0)
    links=[[0,0] for _ in range(128)];heads=[]
    for sentinel,nodes in ((128,free),(129,active)):
        chain=[sentinel]+nodes
        heads.extend((chain[1] if nodes else sentinel,chain[-1]))
        for i,node in enumerate(chain[1:],1):links[node]=[chain[(i+1)%len(chain)],chain[i-1]]
    assert bytes(x.mem_read(pool+8,20))==struct.pack('<5I',*heads,len(active))
    for i,pair in enumerate(links):assert bytes(x.mem_read(slots+i*228+216,8))==struct.pack('<2I',*pair)
    assert bytes(x.mem_read(particles+12,8))==struct.pack('<2I',0,created)
    slot_bytes=bytes(228);particle=bytes(120)
    if not exhausted:
        raw=bytes.fromhex(event['after']);pair=links[slot]
        slot_bytes=compact(raw)+raw[4:8]+raw[0xa4:0xb0]+raw[0xa0:0xa4]+raw[0x9c:0xa0]+raw[:4]+raw[0x144:0x148]+struct.pack('<3I',*pair,not release)
        actual=bytes(x.mem_read(slots+slot*228,228))
        assert actual==slot_bytes,(event['operation'],slot,[(i,a,b) for i,(a,b) in enumerate(zip(actual,slot_bytes)) if a!=b][:20])
        particle=bytearray.fromhex(event['particle'])
        for offset in (0,4):struct.pack_into('<I',particle,offset,translate_particle(struct.unpack_from('<I',particle,offset)[0]))
        struct.pack_into('<I',particle,0x68,0 if release else slot+1)
        # Original first particle starts at pool-1 head; all creates here allocate one.
        if not release:particle_index=500+created-1
        else:
            # The particle retained in this slot was the most recent allocation for it.
            particle_index=last_particle[slot]
        if not release:
            last_particle[slot]=particle_index
        assert bytes(x.mem_read(records+particle_index*120,120))==particle,(slot,'particle')
    response=struct.pack('<4I',status,0xffffffff if release or exhausted else slot,struct.unpack('<I',x.mem_read(random,4))[0],len(active))+slot_bytes+particle
    response+=b''.join(struct.pack('<2I',*pair) for pair in links)+struct.pack('<6I',*heads,0,created)
    expected.extend(response)
actual=subprocess.check_output([str(c['c']['c']['probe']),'--emitter-pool'],input=commands)
assert len(actual)==len(expected),(len(actual),len(expected))
assert actual==expected,[(i//1412,i%1412,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
report=dict(result='PASS',allocations=320,releases=192,exhaustions=2,slot_bytes=228,slot_storage_bytes=29184,scope='Original full emitter lifecycle versus shared PC/NXDK: FIFO links/counts, retained slot payload, estimated radius, immediate particle payload and detachment. Resolved room, parentless owner; campaign hookup and room traversal excluded.')
(root/'artifacts/emitter-pool-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
