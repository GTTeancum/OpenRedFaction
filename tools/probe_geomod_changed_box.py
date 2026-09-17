"""Execute original worker466e0a..466ea6 changed-fragment box publication."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EBX,UC_X86_REG_FPCW
import itertools

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    im=pefile.PE(data=raw).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);b=0x30000000;u.mem_map(b,65536)
    f=lambda *v:struct.pack('<'+'f'*len(v),*v);w=lambda v:struct.pack('<I',v);rows=[]
    for bounds,offset,count in itertools.product([(-1,-2,-3,1,2,3),(0,0,0,.01,.02,.03),(-1000,-500,-125,250,750,1000)],
        [(0,0,0),(.1,-.2,.3),(65536.125,-32768.25,16384.5)],[0,31,32]):
        source=f(*bounds,*offset);before=bytes([165])*768;u.mem_write(0x648280,before);u.mem_write(0x649604,w(count))
        u.mem_write(b+0x48,source[:24]);u.mem_write(b+0xe014,source[24:]);u.reg_write(UC_X86_REG_EBX,b)
        u.reg_write(UC_X86_REG_ESP,b+0xe000);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x466e0a,0x466ea6,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x466ea6
        after=bytes(u.mem_read(0x648280,768));new=struct.unpack('<I',u.mem_read(0x649604,4))[0]
        assert new==min(count+1,32)
        if count<32:
            assert after[:count*24]==before[:count*24] and after[(count+1)*24:]==before[(count+1)*24:]
            output=after[count*24:(count+1)*24]
        else:assert after==before;output=bytes(24)
        rows.append(dict(inputs=list(struct.unpack('<9I',source)),count=count,output=list(struct.unpack('<6I',output))))
    out=ROOT/'artifacts/geomod-postedit-re';out.mkdir(exist_ok=True)
    (out/'changed-box-publication.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,
        scope='Real worker arithmetic/copy helpers; fragment bounds and placement supplied. Component extraction not executed.'),indent=2)+'\n')
    (ROOT/'tests/fixtures/geomod_changed_box.inc').write_text('/* Original466e0a..466ea6, real vector helpers. */\n'+''.join(
        ' {{'+','.join(str(v)+'u' for v in r['inputs'])+'},'+str(r['count'])+'u,{'+','.join(str(v)+'u' for v in r['output'])+'}},\n' for r in rows))
    print('PASS',len(rows),'original changed-box publications and capacity preservation')

if __name__=='__main__':main()
