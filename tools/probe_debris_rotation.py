"""Execute original debris matrix update48f64a..48f678, including both matrix helpers."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_FPCW,UC_X86_REG_EAX,UC_X86_REG_ECX
import itertools
import argparse

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live-log',type=str)
    args=parser.parse_args()
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image);b=0x30000000;u.mem_map(b,65536)
    f=lambda *x:struct.pack('<'+'f'*len(x),*x);words=lambda data:list(struct.unpack('<'+'I'*(len(data)//4),data))
    identity=[1,0,0,0,1,0,0,0,1];axes=[(1,0,0),(0,1,0),(0,0,1),(.3,.4,.8660254),(-1,.25,.5)];rows=[]
    def step(basis,axis,spin,dt):
        source=f(*basis,*axis,spin,dt);u.mem_write(b+0x14,source[:36]);u.mem_write(b+0x54,source[36:52]);u.mem_write(0x5a4014,source[52:])
        u.reg_write(UC_X86_REG_EAX,b+0x54);u.reg_write(UC_X86_REG_ECX,0x7603f8);u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_ESP,b+0xe000);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x48f64a,0x48f678,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x48f678
        output=bytes(u.mem_read(b+0x14,36));rows.append(dict(inputs=words(source),output=words(output)))
        return list(struct.unpack('<9f',output))
    if args.live_log:
        from pathlib import Path
        log=Path(args.live_log);samples=0
        for line in log.read_text().splitlines():
            if not line.startswith('DEBRIS_ROTATION_SAMPLE '):continue
            frame,slot,*values=map(int,line.split()[1:]);assert len(values)==23
            floats=struct.unpack('<14f',struct.pack('<14I',*values[:14]))
            step(floats[:9],floats[9:12],floats[12],floats[13])
            assert rows[-1]['output']==values[14:],(frame,slot,rows[-1]['output'],values[14:])
            samples+=1
        assert samples>0,'No live rotation samples'
        report=dict(result='PASS',original_sha256=sha,log_sha256=hashlib.sha256(log.read_bytes()).hexdigest(),samples=samples,
                    scope='Every traced live incremental matrix matches original48f64a..48f678; world collision and rendering are outside this probe.')
        (ROOT/'artifacts/debris-motion/rotation-live-original.json').write_text(json.dumps(report,indent=2)+'\n')
        print('PASS',samples,'live incremental rotations match original words exactly')
        return
    for basis,axis,spin,dt in itertools.product([identity,[0,0,-1,0,1,0,1,0,0],[1,.1,0,0,.9,.2,.3,0,1.1]],axes,[0,3.1415927,6.2,-2],[0,1/60,.125]):step(basis,axis,spin,dt)
    basis=identity
    for frame in range(90):basis=step(basis,axes[frame//30],3.1415927+frame/100,1/60)
    (ROOT/'artifacts/debris-motion/rotation-original.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,scope='Real48f64a..48f678 with unhooked4fbf30/40ea80/40a3b0. No collision, render or actor effects.'),indent=2)+'\n')
    (ROOT/'tests/fixtures/debris_rotation.inc').write_text('/* Original incremental debris rotation, including a90-step changing-axis sequence. */\n'+''.join(' {{'+','.join(str(v)+'u' for v in r['inputs'])+'},{'+','.join(str(v)+'u' for v in r['output'])+'}},\n' for r in rows))
    print('PASS captured',len(rows),'original incremental orientation cases')

if __name__=='__main__':main()
