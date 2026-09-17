"""Execute original42e3d0 with particle/world-effect submission recorded."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
import itertools
import subprocess

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();rows=[]
    w=lambda *x:struct.pack('<'+'I'*len(x),*[v&0xffffffff for v in x])
    f=lambda *x:struct.pack('<'+'f'*len(x),*x)
    for damage,fill,allocation in itertools.product([0,.001,1.25,400],[0,0xa5],[0,123]):
        u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
        b=0x30000000;u.mem_map(b,65536);stack=b+0xe000;stop=b+0xff00;position=b+0x1000
        word=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
        u.mem_write(stack-512,bytes([fill])*512);u.mem_write(position,f(1,2,3))
        u.mem_write(0x595e0c,w(37,6));u.mem_write(0x62f760,w(42))
        u.mem_write(stack,w(stop,position,71,0)+f(damage));calls=[]
        def ret(value):
            sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
        def hook(cpu,address,size,context):
            sp=cpu.reg_read(UC_X86_REG_ESP)
            if address==0x496840:
                args=list(struct.unpack('<7I',cpu.mem_read(sp+4,28)))
                packet=list(struct.unpack('<19I',cpu.mem_read(args[1],76)))
                assert args[0]==0 and args[2:]==[71,0,0xffffffff,0,0],args
                calls.append(dict(service='particle',args=args,packet=packet));ret(allocation)
            elif address==0x436490:
                args=list(struct.unpack('<7I',cpu.mem_read(sp+4,28)))
                assert args==[42,71,0,position,0x3e800000,0,0],args
                calls.append(dict(service='world_effect',args=args));ret(0)
        u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x42e3d0,stop,count=10000)
        assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
        assert [c['service'] for c in calls]==['particle','world_effect']
        packet=calls[0]['packet'];assert packet[:6]==list(struct.unpack('<6I',f(1,2,3,0,0,0)))
        assert packet[7:9]==[0,0] and packet[10:13]==[0x3f000000,37,6]
        assert packet[13]==0xff7f7f7f and packet[15:17]==[0,0]
        # These three words are untouched by the original caller, not defaults.
        sentinel=fill*0x01010101
        assert packet[9]==packet[17]==packet[18]==sentinel
        rows.append(dict(damage=damage,fill=fill,allocation=allocation,calls=calls))
    blob=subprocess.check_output([str(ROOT/'build/pc/Release/rf_effect_probe.exe'),'--vclip-load',str(ROOT/'Installed_Game/tables.vpp'),'bloodsplat','65536'])
    assert len(blob)==504 and blob[:4]==bytes(4)
    definition=blob[4:];count=struct.unpack_from('<i',definition,304)[0];assert count==85
    report=dict(result='PASS',original_sha256=sha,cases=rows,authored_particle_count=count,definition_sha256=hashlib.sha256(definition).hexdigest(),
        scope='Complete42e3d0 arithmetic/default writes; particle/world-effect services recorded, no allocation/render/world-effect execution. Bitmap37/frames6/clip42 are supplied tokens, not resolved runtime IDs.')
    (ROOT/'artifacts/debris-motion/blood-original.json').write_text(json.dumps(report,indent=2)+'\n')
    fixtures=[]
    for r in rows:
        if r['fill'] or r['allocation']:continue
        bits=struct.unpack('<I',f(r['damage']))[0]
        fixtures.append(' {'+str(bits)+'u,{'+','.join(str(v)+'u' for v in r['calls'][0]['packet'])+'}},')
    (ROOT/'tests/fixtures/debris_blood.inc').write_text('/* Original42e3d0 with zeroed unused stack words. */\n'+'\n'.join(fixtures)+'\n')
    print('PASS',len(rows),'ordered blood requests, failed allocation still dispatches burst; authored85 particles')

if __name__=='__main__':main()
