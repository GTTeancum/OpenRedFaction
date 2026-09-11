"""Compare complete original4cd9e0 and NXDK crossings on retained level trees.

Reuse the locator's real-data materialization and its independent loader checks.
The original loader is not executed. This is a CPU harness, not an XEMU run.
"""
import hashlib,json,re,runpy,struct
from pathlib import Path

root=Path(__file__).resolve().parents[1]
ev=runpy.run_path(str(root/'tools/verify_loaded_room_locator.py'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_FPCW
u,x=ev['u'],ev['x'];w=ev['w'];rooms=ev['rooms']
points=ev['base']+61*1024*1024;output=points+32
stack,stop=ev['stack'],ev['stop']
entry=int(re.search(r'_rf_collision_cross_rooms\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
selected=[]
def observe(m,address,size,context):
    if address==0x4cdc41:selected.append(m.reg_read(UC_X86_REG_ESI))
u.hook_add(UC_HOOK_CODE,observe)
faces=[(r,i,face) for r,(_,_,fs) in enumerate(rooms) for i,face in enumerate(fs) if face[1]>=3]
# Deterministically span the level instead of concentrating on its first room.
sample=[faces[i] for i in sorted({j*(len(faces)-1)//min(119,len(faces)-1) for j in range(min(120,len(faces)))})] if len(faces)>1 else faces
results=[];oblique=0
for r,i,(geometry,count,flags,source,vertices) in sample:
    normal=struct.unpack('<3f',geometry[:12])
    polygon=list(struct.iter_unpack('<3f',vertices))
    center=[sum(v[a] for v in polygon)/count for a in range(3)]
    oblique+=sum(abs(v)>1e-5 for v in normal)>1
    for distance in (0.00005,0.1,4.0):
        start=[center[a]-normal[a]*distance for a in range(3)]
        end=[center[a]+normal[a]*distance for a in range(3)]
        for preferred in (0xffffffff,r):
            wire=struct.pack('<6f',*start,*end)
            u.mem_write(points,wire);x.mem_write(points,wire);selected.clear()
            u.mem_write(stack,w(stop,ev['solid'],0 if preferred==0xffffffff else ev['roomptrs'][r],points,points+12))
            u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
            u.emu_start(0x4cd9e0,stop,count=10000000)
            assert u.reg_read(UC_X86_REG_EIP)==stop,('original limit',r,i)
            hit=bool(u.reg_read(UC_X86_REG_EAX)&255)
            assert bool(selected)==hit
            wanted=(0xffffffff,0xffffffff)
            if hit:
                face=selected[-1];rp=struct.unpack('<I',u.mem_read(face+0x44,4))[0]
                wanted=(ev['roomptrs'].index(rp),ev['face_ids'][face])
            x.mem_write(output,bytes([0xa5])*8)
            x.mem_write(stack,w(stop,ev['views'],ev['room_count'],ev['plist'],ev['primary_count'],preferred,points,points+12,output))
            x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
            x.emu_start(entry,stop,count=10000000)
            assert x.reg_read(UC_X86_REG_EIP)==stop,('NXDK limit',r,i)
            assert x.reg_read(UC_X86_REG_EAX)==0,('NXDK status',r,i,x.reg_read(UC_X86_REG_EAX))
            room,face=struct.unpack('<2I',x.mem_read(output,8))
            actual=(room,rooms[room][2][face][3]) if room!=0xffffffff else (room,face)
            assert actual==wanted,('crossing',r,i,distance,preferred,actual,wanted)
            results.append(dict(source=source,preferred=preferred,distance=distance,room=room,face=actual[1]))
report=dict(result='PASS',level=ev['level'],rooms=len(rooms),primary=ev['primary_count'],sampled_faces=len(sample),oblique_faces=oblique,queries=len(results),hits=sum(v['room']!=0xffffffff for v in results),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete original4cd9e0 vs NXDK over retained real-level trees; compare exact first room/source face. Short and long normal crossings, all primary roots and preferred room. Loader reconstruction remains shared input; no PC crossing execution or native XEMU gameplay.',results=results)
(root/('artifacts/loaded-room-crossing-'+ev['level']+'.json')).write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k!='results'})
