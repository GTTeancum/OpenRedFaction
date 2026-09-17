"""Execute complete original490900 and verify its only possible write is age."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
import itertools

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
    b=0x30000000;u.mem_map(b,65536);node=b+0x100;stack=b+0xe000;stop=b+0xff00
    f=lambda *v:struct.pack('<'+'f'*len(v),*v)
    w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);rows=[]
    for center,offset,radius,gate in itertools.product([(0,0,0),(13.25,-8.5,6.125)],
        [(0,0,0),(1.999,0,0),(2,0,0),(2.001,0,0),(1,1,1),(-1.25,.5,-.75),(1.2,1.2,1.2)],
        [0,2,-2,1.75],[-1,0,1]):
        position=[center[i]+offset[i] for i in range(3)];source=f(*position,*center,radius,.625,2.75)
        before=bytearray([0xa5]*0x80);before[:4]=w(0x75eed8);before[8:20]=source[:12]
        before[0x64:0x68]=w(gate);before[0x70:0x78]=source[28:36]
        u.mem_write(node,bytes(before));u.mem_write(0x75eed8,w(node));u.mem_write(b,source[12:24])
        u.mem_write(stack,w(stop,b)+source[24:28]);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x490900,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
        after=bytes(u.mem_read(node,0x80));assert before[:0x70]==after[:0x70] and before[0x74:]==after[0x74:]
        rows.append(dict(inputs=list(struct.unpack('<9I',source)),bounces=gate,output=struct.unpack_from('<I',after,0x70)[0]))
    out=ROOT/'artifacts/debris-motion';out.mkdir(parents=True,exist_ok=True)
    (out/'postedit-original.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows),indent=2)+'\n')
    (ROOT/'tests/fixtures/debris_postedit.inc').write_text('/* Full original490900; unchanged bytes checked by generator. */\n'+''.join(
        ' {{'+','.join(str(x)+'u' for x in r['inputs'])+'},'+str(r['bounces'])+','+str(r['output'])+'u},\n' for r in rows))
    print('PASS',len(rows),'original settled-debris cleanup cases')

if __name__=='__main__':main()
