"""Original debris actor-loop admission/math; damage/effects/physics services recorded."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
import itertools

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();rows=[]
    w=lambda *x:struct.pack('<'+'I'*len(x),*[v&0xffffffff for v in x])
    f=lambda *x:struct.pack('<'+'f'*len(x),*x)
    for flags,distance,room_match,actors,velocity in itertools.product([0,1,2,3],[1,1.5,1.5000001192092896],[False,True],[1,2],[(3,4,0),(0,0,0),(-1.7,2.3,4.9)]):
        u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
        b=0x30000000;u.mem_map(b,65536);chunk=b;actor=b+0x1000;entry=b+0x5000;stack=b+0xe000;stub=b+0xf000
        word=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
        u.mem_write(stub,b'\xd9\xee\xc3') # Supplied damage result ST0=0, natural cdecl return.
        u.mem_write(chunk+8,f(0,0,0));u.mem_write(chunk+0x38,f(.5));u.mem_write(chunk+0x48,f(*velocity));u.mem_write(chunk+0x6c,w(123));u.mem_write(chunk+0x78,w(flags))
        u.mem_write(0x7c7634,w(actors));u.mem_write(0x872114,w(91))
        for i in range(actors):
            a=actor+i*0x1800;e=entry+i*32
            u.mem_write(0x7c75e4+i*4,w(e));u.mem_write(e+0x14,w(10+i))
            u.mem_write(a,w(123 if room_match else 124));u.mem_write(a+0x2c,w(20+i));u.mem_write(a+0x3c,f(distance,0,0));u.mem_write(a+0x78,f(1));u.mem_write(a+0x1430,w(b+0x6000+i*256))
        calls=[]
        def ret(value=0):
            sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
        def hook(cpu,a,size,context):
            sp=cpu.reg_read(UC_X86_REG_ESP)
            if a in (0x48f7a5,0x48f7a9):cpu.emu_stop()
            elif a==0x426fc0:ret(actor+(word(sp+4)-10)*0x1800)
            elif a==0x4892c0:
                calls.append(dict(service='damage',args=list(struct.unpack('<8I',cpu.mem_read(sp+4,32)))))
                cpu.reg_write(UC_X86_REG_EIP,stub)
            elif a in (0x42e3d0,0x4a5a20,0x4a5af0):
                calls.append(dict(service=hex(a)));ret(b+0x7000 if a==0x4a5a20 else 0)
        u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_X86_REG_ESI,chunk);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
        try:u.emu_start(0x48f678,0x48f7b9,count=10000)
        except Exception as e:raise RuntimeError(hex(u.reg_read(UC_X86_REG_EIP))) from e
        assert u.reg_read(UC_X86_REG_EIP) in (0x48f7a5,0x48f7a9)
        hits=[c for c in calls if c['service']=='damage'];expected=actors if not flags&2 and room_match and distance<1.5 else 0
        assert len(hits)==expected,(flags,distance,room_match,actors,calls)
        assert word(chunk+0x78)==flags|(2 if expected else 0)
        for i,c in enumerate(hits):
            assert c['args'][0]==20+i and c['args'][2:]==[0xffffffff,91,1,0,0xffffffff,0],c
            if velocity==(3,4,0):assert c['args'][1]==0x3fa00000
        rows.append(dict(flags=flags,distance=distance,velocity=velocity,room_match=room_match,actors=actors,output_flags=word(chunk+0x78),calls=calls))
    out=ROOT/'artifacts/debris-motion/actor-original.json'
    out.write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,scope='Actual48f678..48f7a5 admission, overlap, speed and scalar math. Actor resolver supplied; damage/effect/physics services recorded. No live actor damage integration.'),indent=2)+'\n')
    fixtures=[]
    for r in rows:
        if r['flags'] or not r['room_match'] or r['actors']!=1:continue
        inputs=struct.unpack('<11I',f(0,0,0,*r['velocity'],.5,r['distance'],0,0,1))
        hit=next((c for c in r['calls'] if c['service']=='damage'),None)
        fixtures.append(' {{'+','.join(str(x)+'u' for x in inputs)+'},'+str(int(hit is not None))+'u,'+str(hit['args'][1] if hit else 0)+'u},')
    (ROOT/'tests/fixtures/debris_actor.inc').write_text('/* Original actor-loop results; tools/probe_debris_actor.py. */\n'+'\n'.join(fixtures)+'\n')
    print('PASS',len(rows),'actor admission cases: strict tangent exclusion, room/flag gates, multiple actors, damage routing')

if __name__=='__main__':main()
