"""Owned authored visibility state and original per-view walk on installed levels."""
import runpy,struct,re,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_visibility_view_scale.py')))
u,x,base,stack,stop,root=(c[k] for k in ('u','x','base','stack','stop','root'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
for m in (u,x):m.mem_map(base+0x10000,0x200000)
symbols=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
def invoke(m,address,args=(),this=0):
    m.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.reg_write(UC_X86_REG_ECX,this)
    m.emu_start(address,stop,count=10000000);assert m.reg_read(UC_X86_REG_EIP)==stop
    return m.reg_read(UC_X86_REG_EAX)
def words(m,address,*values):m.mem_write(address,struct.pack('<'+'I'*len(values),*values))
inventory=json.loads((root/'artifacts/inventory.json').read_text())
levels=json.loads((root/'artifacts/levels.json').read_text());results=[]
def run(level,budget):
    return subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(root/'Installed_Game'/level['archive']),level['file'],'--visibility',str(budget)],text=True)
for level in levels:
    section=next(s for s in level['sections'] if s['type']=='0x100')
    archive=next(a for a in inventory['files'] if a['path']==level['archive'])
    entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
    with (root/'Installed_Game'/level['archive']).open('rb') as f:
        f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
    at=6
    def number():
        global at
        n=struct.unpack_from('<I',data,at)[0];at+=4;return n
    def string():
        global at
        n=struct.unpack_from('<H',data,at)[0];at+=2+n
    for _ in range(number()):string()
    n=number();at+=n*12;count=number();flags=[]
    for _ in range(count):
        raw=data[at:at+40];at+=40;flags.append((raw[28],raw[34]));string()
        if raw[32]:at+=8;string();at+=37
        if raw[33]:at+=4
    for _ in range(number()):number();n=number();at+=n*4
    portals=number();records=list(struct.iter_unpack('<2I6f',data[at:at+portals*32]));adj=[[] for _ in range(count)]
    for i,(a,b,*_) in enumerate(records):adj[a].append(i);adj[b].append(i)
    primary=[i for i,(_,detail) in enumerate(flags) if not detail]
    text=run(level,1024*1024);lines=iter(text.splitlines());assert next(lines)=='0'
    rooms,p,n,resident=map(int,next(lines).split());assert (rooms,p,n)==(count,portals,len(primary))
    assert resident==76+(count+1)*4+40*portals+52*count+56*portals+7196
    position=0
    for i in range(count):
        assert list(map(int,next(lines).split()))==[position,len(adj[i]),int(any(flags[i])),0];position+=len(adj[i])
    assert list(map(int,next(lines).split()))==primary
    world=base+0x3000;room_base=base+0x20000;portal_base=base+0x140000;arrays=base+0x180000
    assert count*512<0x120000 and portals*64<0x10000
    u.mem_write(world,bytes(0x378))
    for offset,indices,array in ((0x90,list(range(count)),arrays),(0x9c,primary,arrays+0x4000)):
        words(u,world+offset,len(indices),len(indices),array)
        for i,index in enumerate(indices):words(u,array+i*4,room_base+index*512)
    words(u,world+0xb4,portals,portals,arrays+0x8000)
    position=0
    for i,(skip,detail) in enumerate(flags):
        room=room_base+i*512;u.mem_write(room,bytes(512));u.mem_write(room,bytes((detail,0)))
        invoke(u,0x4f0300,(world,skip),room);assert bytes(u.mem_read(room,2))==bytes((detail,skip))
        words(u,room+0x30,len(adj[i]),len(adj[i]),arrays+0xc000+position*4)
        for portal in adj[i]:words(u,arrays+0xc000+position*4,portal_base+portal*64);position+=1
    for i,(a,b,*bounds) in enumerate(records):
        portal=portal_base+i*64;u.mem_write(portal,bytes(64));words(u,portal,room_base+a*512,room_base+b*512)
        u.mem_write(portal+8,struct.pack('<6f',*bounds));words(u,arrays+0x8000+i*4,portal)
    owner=base+0x1000;xr=base+0x20000;xp=base+0x150000;xc=base+0x140000;xl=base+0x160000;xprimary=base+0x180000;xlinks=base+0x1a0000
    x.mem_write(owner,bytes(76));words(x,owner,1);words(x,owner+16,xlinks);words(x,owner+24,portals)
    words(x,owner+32,xr,base+0x190000,count,0);words(x,owner+48,xl,xprimary,len(primary),xc,xp,base+0x170000,resident)
    x.mem_write(xr,bytes(count*28));x.mem_write(base+0x190000,bytes(count*4))
    for i,index in enumerate(primary):words(x,xprimary+i*4,index)
    position=0
    for i in range(count):
        words(x,xl+i*16,position,len(adj[i]),int(any(flags[i])),0)
        for portal in adj[i]:words(x,xlinks+position*4,portal);position+=1
    for i,(a,b,*bounds) in enumerate(records):
        words(x,xp+i*28,a,b,0,0,0,0,0);x.mem_write(xc+i*28,struct.pack('<6fI',*bounds,0))
    for stage in range(3):
        origin=(8. if stage else 0.,0.,0.);basis=(1.,0.,0.,0.,1.,0.,0.,0.,1.)
        u.mem_write(base,struct.pack('<9f',*basis));u.mem_write(base+64,struct.pack('<3f',*origin))
        words(u,0x17c7bec,0,0,640,480);words(u,0x17c7bc4,640,480);u.mem_write(0x17c7bd8,struct.pack('<f',1.))
        u.mem_write(0x1818b68,struct.pack('<f',100.));u.mem_write(0x1818b74,struct.pack('<f',.1));u.mem_write(0x1818b65,b'\1')
        u.mem_write(0x5a4d18,b'\1');u.mem_write(0x5a445a,b'\1');words(u,0x1e652e8,0);words(u,0x17c7bcc,0x66)
        invoke(u,0x547150,(base,base+64,0x42b40000,0,1));words(u,0x5a3824,1);words(u,0x9bb588,0)
        if stage!=1:invoke(u,0x4d2f80,this=world)
        start=primary[-1 if stage else 0] if primary and stage!=2 else 0xffffffff
        invoke(u,0x4d4760,(world,room_base+start*512 if start!=0xffffffff else 0,0))
        visible=struct.unpack('<I',u.mem_read(0x9bb57c,4))[0]
        order=[(v-room_base)//512 for v in struct.unpack('<'+'I'*visible,u.mem_read(0x9a8548,visible*4))]
        assert list(map(int,next(lines).split()))==[0,visible],(level['file'],stage)
        assert list(map(int,next(lines).split()))==order
        camera=struct.pack('<4i3fI13f3If',640,480,0,0,1.,90.,100.,1,*origin,*basis,.1,1,1,1,0.)
        x.mem_write(base+0x3000,camera);x.mem_write(base+0x2000,bytes(328))
        assert invoke(x,symbol('rf_visibility_camera_setup'),(base+0x3000,base+0x2000))==0
        if stage!=1:assert invoke(x,symbol('rf_level_visibility_begin_render'),(owner,))==0
        assert invoke(x,symbol('rf_level_visibility_view'),(owner,base+0x2000,640,480,start,0xffffffff,0,1))==0
        assert struct.unpack('<I',x.mem_read(owner+44,4))[0]==visible
        assert list(struct.unpack('<'+'I'*visible,x.mem_read(base+0x190000,visible*4)))==order
        for i in range(count):
            raw=bytes(u.mem_read(room_base+i*512,384));expected=struct.pack('<2I',raw[0x160],raw[0x161])+raw[0x164:0x168]+raw[0x16c:0x17c]
            assert struct.pack('<7I',*(int(v,16) for v in next(lines).split()))==expected,(level['file'],stage,i)
            assert bytes(x.mem_read(xr+i*28,28))==expected
        for i in range(portals):
            raw=bytes(u.mem_read(portal_base+i*64,56));line=next(lines).split()
            assert tuple(map(int,line[:2]))==(raw[0x24],raw[0x25])
            assert struct.pack('<4I',*(int(v,16) for v in line[2:]))==raw[0x28:0x38]
            assert struct.unpack('<I',x.mem_read(xc+i*28+24,4))[0]==raw[0x24]
            assert bytes(x.mem_read(xp+i*28+8,20))==struct.pack('<I',raw[0x25])+raw[0x28:0x38]
    assert next(lines,None) is None
    assert run(level,resident)==text and run(level,resident-1)=='-4\n'
    results.append(dict(level=level['file'],rooms=count,primary=len(primary),blocked=sum(any(f) for f in flags),resident_bytes=resident))
report=dict(result='PASS',levels=len(results),views=3*len(results),rooms=sum(r['rooms'] for r in results),maximum_bytes=max(r['resident_bytes'] for r in results),
    l1s1_bytes=next(r['resident_bytes'] for r in results if r['level']=='L1S1.rfl'),scope='Owned PC loading from all installed levels, independent room flag/portal reads, exact budgets and cleanup after input close. Actual original 547150/4d4760/4d2f80 versus PC and NXDK view wrappers: primary reset, portal projection, two-view eligibility accumulation and missing-start fallback. Original graphics-state call intercepted; coherent zero initial visibility fields supplied. NXDK view uses borrowed fixture arrays; native allocation/residency and live renderer integration excluded.')
(root/'artifacts/level-visibility-verification.json').write_text(json.dumps(dict(report=report,results=results),indent=2)+'\n');print(report)
