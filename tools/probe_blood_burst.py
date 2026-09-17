"""Execute original authored blood burst loop with real CRT RNG and supplied allocation."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW
import itertools,subprocess,math

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image()
    blob=subprocess.check_output([str(ROOT/'build/pc/Release/rf_effect_probe.exe'),'--vclip-load',str(ROOT/'Installed_Game/tables.vpp'),'bloodsplat','65536'])[4:]
    assert len(blob)==500 and struct.unpack_from('<i',blob,304)[0]==85
    p=list(struct.unpack('<46I',blob[316:]));w=lambda *x:struct.pack('<'+'I'*len(x),*[v&0xffffffff for v in x]);f=lambda *x:struct.pack('<'+'f'*len(x),*x)
    rows=[]
    for scale,seed,allocation in itertools.product([0,.01,.25,.5,1],[1,12345],[0,77]):
        u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
        b=0x30000000;u.mem_map(b,65536);definition=b+0x1000;position=b+0x2000;thread=b+0x3000;room=b+0x4000;stack=b+0xe000
        word=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
        u.mem_write(definition+0x34,w(85));u.mem_write(definition+0x3c,w(*p[:9]))
        u.mem_write(definition+0x60,w(p[10],p[11],p[9],p[23],*p[12:19]))
        u.mem_write(definition+0x9c,w(37,1,p[42],p[43],p[24]))
        u.mem_write(position,f(1,2,3));u.mem_write(stack+0xdc,w(position));u.mem_write(stack+0xe0,f(scale));u.mem_write(stack+0xe4,w(-1));u.mem_write(thread+20,w(seed))
        packets=[];draws=[]
        def ret(value):
            sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
        def hook(cpu,a,size,context):
            if a==0x577eef:draws.append(1);ret(thread)
            elif a==0x4c1cec:cpu.emu_stop()
            elif a==0x496840:
                sp=cpu.reg_read(UC_X86_REG_ESP);args=list(struct.unpack('<7I',cpu.mem_read(sp+4,28)))
                assert args[0]==0 and args[2:]==[room,position,0xffffffff,0,0],args
                packets.append(list(struct.unpack('<19I',cpu.mem_read(args[1],76))));ret(allocation)
        u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,definition);u.reg_write(UC_X86_REG_EBX,room);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x4c1a97,0x4c1cf8,count=1000000)
        assert u.reg_read(UC_X86_REG_EIP)==0x4c1cec
        expected=math.ceil(85*struct.unpack('<f',f(scale))[0]);assert len(packets)==expected
        assert len(draws)==5*expected
        rows.append(dict(scale=scale,seed=seed,allocation=allocation,next_seed=word(thread+20),draws=len(draws),packets=packets))
    report=dict(result='PASS',original_sha256=sha,definition_sha256=hashlib.sha256(blob).hexdigest(),cases=rows,
        scope='Actual4c1a97..4c1cec and RNG/vector helpers; authored definition fields mapped into prepared original layout. Particle allocation supplied; allocation orientation RNG, full vclip/visual rendering and parser execution excluded.')
    (ROOT/'artifacts/debris-motion/blood-burst-original.json').write_text(json.dumps(report,indent=2)+'\n')
    selected=next(r for r in rows if r['scale']==.25 and r['seed']==1 and r['allocation']==0)
    text='/* Original prepared blood burst, scale.25 seed1, allocation supplied. */\n'
    text+='static const uint32_t blood_definition_words[46]={'+','.join(str(x)+'u' for x in p)+'};\n'
    text+='static const uint32_t blood_drop_words[][19]={\n'+''.join('{'+','.join(str(x)+'u' for x in row)+'},\n' for row in selected['packets'])+'};\n'
    text+='static const uint32_t blood_drop_seed='+str(selected['next_seed'])+'u;\n'
    (ROOT/'tests/fixtures/blood_burst.inc').write_text(text)
    print('PASS',len(rows),'burst loops; contact scale0.25 emits22 requests and110 pre-allocation RNG draws')

if __name__=='__main__':main()


