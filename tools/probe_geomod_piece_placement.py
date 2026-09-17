"""Execute original4d1330 vertex/bounds recentering with an empty face list."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_FPCW
import itertools

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();rows=[]
    for center,extent in itertools.product([(0,0,0),(.1,-.2,.3),(1234.5,-987.25,2000),(65536,-32768,16384)],[(1,2,3),(.001,.002,.003),(.25,4,1.5)]):
        u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
        b=0x30000000;u.mem_map(b,65536);f=lambda *v:struct.pack('<'+'f'*len(v),*v);w=lambda *v:struct.pack('<'+'I'*len(v),*v)
        points=[[center[k]+sign[k]*extent[k] for k in range(3)] for sign in itertools.product([-1,1],repeat=3)]
        vertices=[b+0x1000+i*0x40 for i in range(8)];source=f(*[v for p in points for v in p])
        u.mem_write(b+0x78,w(8,8,b+0x800));u.mem_write(b+0x800,w(*vertices));u.mem_write(b+0x70,w(0))
        for i,address in enumerate(vertices):u.mem_write(address,source[i*12:(i+1)*12])
        out=b+0x3000;stack=b+0xe000;stop=b+0xf000;u.mem_write(stack,w(stop,out))
        u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x4d1330,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
        result=bytes(u.mem_read(out,12))+bytes(u.mem_read(b+0x48,28))+b''.join(bytes(u.mem_read(v,12)) for v in vertices)
        assert bytes(u.mem_read(b+0x64,12))==bytes(12)
        rows.append(dict(inputs=list(struct.unpack('<24I',source)),output=list(struct.unpack('<34I',result))))
    (ROOT/'artifacts/geomod-postedit-re/piece-placement.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,
        scope='Whole4d1330 on vertices with no faces; original bounds, offset, translated positions and radius. Plane/lightmap relocation not exercised.'),indent=2)+'\n')
    (ROOT/'tests/fixtures/geomod_piece_placement.inc').write_text('/* Original4d1330 vertex/bounds phase, empty face list. */\n'+''.join(
        ' {{'+','.join(str(v)+'u' for v in r['inputs'])+'},{'+','.join(str(v)+'u' for v in r['output'])+'}},\n' for r in rows))
    print('PASS',len(rows),'original piece placement cases')

if __name__=='__main__':main()
