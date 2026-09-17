"""Real 466550 shape setup and 4667a0 subdivision admission arithmetic."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EDI,UC_X86_REG_FPCW
import itertools

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    im=pefile.PE(data=raw).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im)
    base=0x30000000;u.mem_map(base,65536);node=base+0x1000;piece=base+0x2000;sp=base+0xe000;stop=base+0xff00
    put=lambda a,v:u.mem_write(a,struct.pack('<I',v))
    def hook(cpu,a,size,data):
        if a in (0x4667f4,0x4669f7):cpu.emu_stop()
    u.hook_add(UC_HOOK_CODE,hook);rows=[]
    dimensions=sorted(set(itertools.permutations((2,4,8))))+[(2,2,2),(2,2,8),(8,8,2),(2,6,4),(10,10,10),(10.01,10.01,10.01)]
    for dims,radius,attempts in itertools.product(dimensions,(1.49,1.5,3),(0,9,10)):
        bounds=struct.pack('<6f',-.1,-.2,-.3,*[dims[i]-[.1,.2,.3][i] for i in range(3)])
        u.mem_write(node,bytes(64));put(node+8,piece);u.mem_write(piece+0x48,bounds)
        u.mem_write(piece+0x60,struct.pack('<f',radius));put(sp,stop);put(sp+4,node)
        u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x466550,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
        shape=bytes(u.mem_read(node+0x18,20))
        put(sp+0x10,attempts);u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EDI,node)
        u.emu_start(0x4667a0,stop,count=10000)
        ip=u.reg_read(UC_X86_REG_EIP);assert ip in (0x4667f4,0x4669f7)
        rows.append(dict(bounds=list(struct.unpack('<6I',bounds)),radius=struct.unpack('<I',struct.pack('<f',radius))[0],attempts=attempts,shape=list(struct.unpack('<5I',shape)),split=int(ip==0x4667f4)))
    (ROOT/'artifacts/geomod-postedit-re/piece-shape.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,scope='Full shape initializer with real vector helpers; original subdivision gate only, no split or body construction.'),indent=2)+'\n')
    (ROOT/'tests/fixtures/geomod_piece_shape.inc').write_text('/* Original466550 and4667a0 gate. */\n'+''.join(' {{'+','.join(str(x)+'u' for x in r['bounds'])+'},'+str(r['radius'])+'u,'+str(r['attempts'])+'u,{'+','.join(str(x)+'u' for x in r['shape'])+'},'+str(r['split'])+'u},\n' for r in rows))
    print('PASS',len(rows),'original piece shapes and subdivision gates')
if __name__=='__main__':main()
