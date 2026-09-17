"""Execute complete original490900 and verify its only possible write is age."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
import itertools
import argparse
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--live-log',type=Path);args=parser.parse_args()
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
    b=0x30000000;u.mem_map(b,65536);node=b+0x100;stack=b+0xe000;stop=b+0xff00
    f=lambda *v:struct.pack('<'+'f'*len(v),*v)
    w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);rows=[]
    inputs=[]
    for center,offset,radius,gate in itertools.product([(0,0,0),(13.25,-8.5,6.125)],
        [(0,0,0),(1.999,0,0),(2,0,0),(2.001,0,0),(1,1,1),(-1.25,.5,-.75),(1.2,1.2,1.2)],
        [0,2,-2,1.75],[-1,0,1]):
        position=[center[i]+offset[i] for i in range(3)];inputs.append((gate,f(*position,*center,radius,.625,2.75),None))
    if args.live_log:
        inputs=[]
        for line in args.live_log.read_text().splitlines():
            if not line.startswith('DEBRIS_CLEANUP_SAMPLE '):continue
            frame,slot,gate,*words=map(int,line.split()[1:]);assert len(words)==10
            inputs.append((gate,w(*words[:9]),words[9]))
        assert inputs,'No live cleanup samples'
    for gate,source,expected in inputs:
        before=bytearray([0xa5]*0x80);before[:4]=w(0x75eed8);before[8:20]=source[:12]
        before[0x64:0x68]=w(gate);before[0x70:0x78]=source[28:36]
        u.mem_write(node,bytes(before));u.mem_write(0x75eed8,w(node));u.mem_write(b,source[12:24])
        u.mem_write(stack,w(stop,b)+source[24:28]);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x490900,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
        after=bytes(u.mem_read(node,0x80));assert before[:0x70]==after[:0x70] and before[0x74:]==after[0x74:]
        if expected is not None:assert after[0x70:0x74]==w(expected)
        rows.append(dict(inputs=list(struct.unpack('<9I',source)),bounces=gate,output=struct.unpack_from('<I',after,0x70)[0]))
    out=ROOT/'artifacts/debris-motion';out.mkdir(parents=True,exist_ok=True)
    if args.live_log:
        changes=sum(r['inputs'][7]!=r['output'] for r in rows)
        (out/'postedit-live-original.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,
            log_sha256=hashlib.sha256(args.live_log.read_bytes()).hexdigest(),samples=len(rows),age_changes=changes),indent=2)+'\n')
        print('PASS',len(rows),'live cleanup samples,',changes,'age changes match original490900')
        return
    (out/'postedit-original.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows),indent=2)+'\n')
    (ROOT/'tests/fixtures/debris_postedit.inc').write_text('/* Full original490900; unchanged bytes checked by generator. */\n'+''.join(
        ' {{'+','.join(str(x)+'u' for x in r['inputs'])+'},'+str(r['bounces'])+','+str(r['output'])+'u},\n' for r in rows))
    print('PASS',len(rows),'original settled-debris cleanup cases')

if __name__=='__main__':main()
