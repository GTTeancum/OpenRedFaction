"""Compare live crossing traces with full original48f900 wet branch execution.

Run tools/check_live_wet_debris.py first to produce the current replay trace.
Original solid query is supplied as a miss; effect submission is recorded.
"""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW

def main():
    log=ROOT/'artifacts/wet-debris-live/two-shot.log';lines=log.read_text().splitlines()
    samples=[list(map(int,l.split()[1:])) for l in lines if l.startswith('DEBRIS_CROSS_SAMPLE ')]
    state=list(map(int,next(l for l in lines if l.startswith('DEBRIS_CROSSING ')).split()[1:]))
    assert len(samples)==state[1] and samples and state[5]==0
    original=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(original.read_bytes()).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    pe=pefile.PE(str(original));image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(image)+4095)&~4095);u.mem_write(base,image)
    arena=0x30000000;u.mem_map(arena,65536);room=arena+0x1000;chunk=arena+0x2000;stack=arena+0xe000;stop=arena+0xff00
    w=lambda *x:struct.pack('<'+'I'*len(x),*x)
    word=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
    effects=[];queries=[]
    def ret(value=0,pop=0):
        sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value)
        u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
    def hook(cpu,address,size,context):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if address==0x4df1c0:
            query=word(sp+4);result=word(sp+8)
            assert word(query+0x50)==5 and word(query+0x4c)==0
            queries.append(1);cpu.mem_write(result,bytes(40));ret(pop=12)
        elif address==0x4c16e0:
            args=[word(sp+4+i*4) for i in range(8)]
            assert args[:3]==[37,room,0] and args[4:]==[0x3e4ccccd,0xffffffff,0,1]
            effects.append(list(struct.unpack('<3I',cpu.mem_read(args[3],12))));ret()
    u.hook_add(UC_HOOK_CODE,hook);rows=[]
    for flag,*v in samples:
        assert len(v)==15
        u.mem_write(room,bytes(512));u.mem_write(room+0x184,w(flag));u.mem_write(room+0x188,w(v[7]));u.mem_write(room+12,w(v[8]))
        u.mem_write(chunk,bytes(256));u.mem_write(chunk+8,w(*v[:3]));u.mem_write(chunk+0x48,w(*v[3:6]));u.mem_write(chunk+0x6c,w(room))
        u.mem_write(0x6460e8,w(arena+0x3000));u.mem_write(0x8568b0,w(37));u.mem_write(0x5a4014,w(v[6]));u.mem_write(stack,w(stop,chunk))
        u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);effects.clear();queries.clear()
        u.emu_start(0x48f900,stop,count=10000)
        assert u.reg_read(UC_X86_REG_EIP)==stop and len(queries)==1 and effects==[v[12:15]]
        assert bytes(u.mem_read(chunk+8,12))==w(*v[9:12])
        assert bytes(u.mem_read(chunk+0x48,12))==w(*v[3:6]) and word(chunk+0x6c)==room
        rows.append(dict(flag=flag,words=v))
    out=ROOT/'artifacts/debris-motion/live-crossing.json'
    out.write_text(json.dumps(dict(result='PASS',original_sha256=sha,log_sha256=hashlib.sha256(log.read_bytes()).hexdigest(),
        state=state,cases=rows,scope='Actual live crossings versus original48f900, real liquid predicate/math/48fc10. Solid miss supplied; effect recorded, not rendered or heard.'),indent=2)+'\n')
    print('PASS',len(rows),'live crossing endpoints and ripple request positions bit-exact; size0.2, velocity and room retained')

if __name__=='__main__':main()
