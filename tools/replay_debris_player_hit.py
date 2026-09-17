"""Ordinary rocket/approach replay and original-code verification of actual debris contacts."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_FPCW
import math,os,subprocess,sys
from pathlib import Path

def verify_native(path):
    report=json.loads(Path(path).read_text());assert report['result']=='PASS' and report['disc_restored']
    assert report['input_sha256']=='beb120ef4e7aa4d0fef7ac87e0dc305e8d04ca9169694a9eeb73fffcc913ea2d'
    for platform in ('pc','xbox'):
        checks=report['checks']
        assert checks['DEBRIS_PLAYER_TEST'][platform]==[0]*8
        assert checks['DEBRIS_PLAYER'][platform]==[239,2,2,1060637214,0,2,1110020820,2]
        assert checks['DEBRIS_BLOOD'][platform]==[2,2,44,0,0,496276,1721238655,1517084350]
        assert checks['ROCKET_BLAST'][platform][:5]==[1,1,1,0,1]
    print('PASS ordinary native input, two debris hits/blood effects, correct final health; fixture disabled')

def main():
    if len(sys.argv)==3 and sys.argv[1]=="--native-report":
        verify_native(sys.argv[2]);return
    folder=ROOT/'artifacts/debris-player-live';folder.mkdir(exist_ok=True)
    f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
    dt=f32(1/60);dy=2.5-.384041399;pitch=math.asin(dy/(abs(-4.699+2.75)+abs(dy)));angle=0;rows=[]
    for frame in range(400):
        look=0
        if 130<=frame<220:
            look=f32(max(-1,min(1,(pitch-angle)/((220-frame)*dt))));angle=f32(angle+f32(dt*look))
        rows.append(struct.pack('<5f7I',0,0,.8 if 255<=frame<285 else 0,look,0,0,0,0,int(frame==240),0,int(frame in (10,20,30,40)),0))
    replay=folder/'ordinary.bin';replay.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(rows))
    env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0')
    exe=ROOT/'build/pc/Release/rf_pc_play.exe';log=folder/'ordinary.log'
    with log.open('wb') as output:
        subprocess.run([str(exe),'--spawn-replay',str(ROOT/'Installed_Game'),str(replay),str(folder/'ordinary.ppm')],cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT,check=True,timeout=120)
    lines=log.read_text().splitlines()
    state=lambda label:list(map(int,next(l for l in lines if l.startswith(label+' ')).split()[1:]))
    contacts=[list(map(int,l.split()[1:])) for l in lines if l.startswith('DEBRIS_PLAYER_HIT ')]
    assert len(contacts)==2 and [c[0] for c in contacts]==[287,291] and len({c[1] for c in contacts})==2
    assert all(0<=c[1]<80 and c[3]&2==0 for c in contacts)
    assert state('DEBRIS_PLAYER_TEST')==[0]*8
    assert state('DEBRIS_PLAYER')==[239,2,2,1060637214,0,2,1110020820,2]
    assert state('DEBRIS_BLOOD')[:5]==[2,2,44,0,0]
    assert state('ROCKET_BLAST')[:5]==[1,1,1,0,1]
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();w=lambda *x:struct.pack('<'+'I'*len(x),*[v&0xffffffff for v in x])
    for frame,slot,room,flags,*v in contacts:
        assert len(v)==12
        u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
        b=0x30000000;u.mem_map(b,65536);actor=b+0x1000;entry=b+0x5000;stack=b+0xe000;stub=b+0xf000
        word=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
        u.mem_write(b+8,w(*v[:3]));u.mem_write(b+0x48,w(*v[3:6]));u.mem_write(b+0x38,w(v[6]));u.mem_write(b+0x6c,w(123));u.mem_write(b+0x78,w(flags))
        u.mem_write(actor,w(123));u.mem_write(actor+0x3c,w(*v[7:10]));u.mem_write(actor+0x78,w(v[10]));u.mem_write(actor+0x2c,w(20))
        u.mem_write(0x7c7634,w(1));u.mem_write(0x7c75e4,w(entry));u.mem_write(entry+0x14,w(10));u.mem_write(0x872114,w(91));u.mem_write(stub,b'\xd9\xee\xc3');calls=[]
        def ret(value=0):
            sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
        def hook(cpu,a,size,context):
            sp=cpu.reg_read(UC_X86_REG_ESP)
            if a in (0x48f7a5,0x48f7a9):cpu.emu_stop()
            elif a==0x426fc0:ret(actor)
            elif a==0x4892c0:
                args=list(struct.unpack('<8I',cpu.mem_read(sp+4,32)));calls.append(args)
                assert args==[20,v[11],0xffffffff,91,1,0,0xffffffff,0]
                cpu.reg_write(UC_X86_REG_EIP,stub)
            elif a in (0x42e3d0,0x4a5a20,0x4a5af0):ret()
        u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x48f678,0x48f7b9,count=10000)
        assert u.reg_read(UC_X86_REG_EIP) in (0x48f7a5,0x48f7a9) and len(calls)==1 and word(b+0x78)==flags|2
    report=dict(result='PASS',original_sha256=sha,pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),replay_sha256=hashlib.sha256(replay.read_bytes()).hexdigest(),log_sha256=hashlib.sha256(log.read_bytes()).hexdigest(),contacts=contacts,
        scope='Ordinary input only; no injected contact. Two distinct fragment overlaps/scalars match original player loop with supplied lookup/damage/effect services. Original full-world trajectory/health/render execution not claimed.')
    (folder/'ordinary-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS two ordinary fragment hits, original scalar bits exact, two blood effects; final health42.393387')

if __name__=='__main__':main()
