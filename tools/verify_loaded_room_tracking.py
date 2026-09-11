"""Complete original cached-room policy and shared NXDK world/emitter adapter."""
import hashlib,json,re,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cross=runpy.run_path(str(root/'tools/verify_loaded_room_crossing.py'))
ev=cross['ev'];u,x=ev['u'],ev['x'];w=ev['w']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
symbols=(root/'build/xbox/main.map').read_text()
entries=[int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16) for name in ('rf_geometry_collision_world_track','rf_geometry_collision_world_track_emitter')]
points,output,stack,stop=(cross[k] for k in ('points','output','stack','stop'))
count=retained=missing=0;commands=[];expected_pc=[]
for index,row in enumerate(cross['results']):
    start,end=row['start'],row['end']
    for mode in range(3):
        old=0xffffffff if mode==0 else (row['preferred'] if row['preferred']!=0xffffffff else ev['primary'][index%ev['primary_count']])
        wire=struct.pack('<6f',*start,*(start if mode==2 else end))
        flags=(0,1,256)[index%3]
        u.mem_write(points,wire);x.mem_write(points,wire)
        u.mem_write(stack,w(stop,0 if old==0xffffffff else ev['roomptrs'][old],points,points+12,flags))
        u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,ev['solid']);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x4cd970,stop,count=10000000)
        assert u.reg_read(UC_X86_REG_EIP)==stop,('original limit',index,mode)
        pointer=u.reg_read(UC_X86_REG_EAX)
        wanted=ev['roomptrs'].index(pointer) if pointer else 0xffffffff
        commands.append(wire+w(old,flags));expected_pc.append(w(0,wanted,0,0 if wanted==0xffffffff else wanted+1))
        for adapter,entry in enumerate(entries):
            previous=(0 if old==0xffffffff else old+1) if adapter else old
            expected=(0 if wanted==0xffffffff else wanted+1) if adapter else wanted
            x.mem_write(output,w(0xa5a5a5a5));x.mem_write(stack,w(stop,ev['world'],previous,points,points+12,flags,output))
            x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
            x.emu_start(entry,stop,count=10000000)
            assert x.reg_read(UC_X86_REG_EIP)==stop,('NXDK limit',index,mode,adapter)
            actual=struct.unpack('<I',x.mem_read(output,4))[0]
            assert x.reg_read(UC_X86_REG_EAX)==0 and actual==expected,('tracking',index,mode,adapter,actual,expected)
        count+=1;retained+=wanted==old and old!=0xffffffff;missing+=wanted==0xffffffff
actual_pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--world-track',str(root/'Installed_Game'/ev['archive']),ev['level']],input=b''.join(commands))
assert actual_pc==b''.join(expected_pc),('PC tracking',next(i for i,(a,b) in enumerate(zip(actual_pc,b''.join(expected_pc))) if a!=b))
report=dict(result='PASS',level=ev['level'],original_cases=count,pc_cases=count*2,nxdk_cases=count*2,retained=retained,missing=missing,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original4cd970 with actual crossing and locator on retained level geometry vs PC and linked NXDK world tracking and emitter room-token adapter. Missing/cached/stationary cases and ignored flags. Original loader, live emitter movement and native XEMU execution not covered.')
(root/('artifacts/loaded-room-tracking-'+ev['level']+'.json')).write_text(json.dumps(report,indent=2)+'\n');print(report)
