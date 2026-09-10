"""Persistent graph adjacency against actual original portal constructor."""
import runpy,json,struct,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,base,stack,stop,root=(c[k] for k in ('u','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
u.mem_map(base+0x10000,0x200000)
inventory=json.loads((root/'artifacts/geometry-portals-verification.json').read_text())['results']
archives={r['file']:r['archive'] for r in json.loads((root/'artifacts/levels.json').read_text())}
def put(a,*v):u.mem_write(a,struct.pack('<'+'I'*len(v),*v))
def run(level,budget):
    return subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(root/'Installed_Game'/archives[level]),level,'--portal-graph',str(budget)],text=True)
results=[]
for level in inventory:
    raw=run(level['level'],1024*1024);lines=raw.splitlines();assert lines[0]=='0'
    rooms,count,resident=map(int,lines[1].split());assert (rooms,count)==(level['rooms'],level['portals'])
    assert resident==28+(rooms+1)*4+count*40
    world=base+0x1000;room_base=base+0x50000;portal_base=base+0x10000;array=base+0x110000
    put(world+0xb4,0,max(1,count),base+0x100000)
    position=0
    for i,want in enumerate(level['adjacency']):
        put(room_base+i*64+0x30,0,max(1,len(want)),array+position*4);position+=max(1,len(want))
    for i,record in enumerate(level['records']):
        portal=portal_base+i*56;a,b=record['rooms'];u.mem_write(portal,bytes(56))
        put(stack,stop,world,room_base+a*64,room_base+b*64);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,portal)
        u.emu_start(0x4f9890,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
        assert struct.unpack('<3I',u.mem_read(portal,8)+u.mem_read(portal+0x20,4))==(room_base+a*64,room_base+b*64,i)
    for i,want in enumerate(level['adjacency']):
        n,_,ptr=struct.unpack('<3I',u.mem_read(room_base+i*64+0x30,12))
        original=[(p-portal_base)//56 for p in struct.unpack('<'+'I'*n,u.mem_read(ptr,n*4))]
        shared=list(map(int,lines[2+i].split()));assert shared==[n,*original] and original==want
    for i,record in enumerate(level['records']):
        packet=struct.pack('<8I',*(int(w,16) for w in lines[2+rooms+i].split()))
        assert packet==struct.pack('<2I6f',*record['rooms'],*record['minimum'],*record['maximum'])
    assert run(level['level'],resident)==raw and run(level['level'],resident-1)=='-4\n'
    results.append(dict(level=level['level'],rooms=rooms,portals=count,resident_bytes=resident))
report=dict(result='PASS',levels=len(results),portals=sum(r['portals'] for r in results),maximum_bytes=max(r['resident_bytes'] for r in results),
    l1s1_bytes=next(r['resident_bytes'] for r in results if r['level']=='L1S1.rfl'),scope='Actual 4f9890 constructor and append callees with preallocated vectors versus persistent PC graph, all authored endpoint lists in order. Bounds after input close, exact budgets and cleanup checked by probe. Original allocation growth, native NXDK memory and renderer integration excluded.')
(root/'artifacts/portal-graph-verification.json').write_text(json.dumps(dict(report=report,results=results),indent=2)+'\n');print(report)
