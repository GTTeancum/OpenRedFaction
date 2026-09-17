"""Execute original solid occupancy/mass/inertia; intercept only mesh recenter mutation.
Input solids are authored polygon lists; bounds, segment/plane tests, polygon
containment, occupancy fill, center, and tensor inversion run original code.
"""
from probe_debris_motion import ROOT, hashlib, json, struct, pefile, Uc, UC_ARCH_X86, UC_MODE_32
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX, UC_X86_REG_FPCW

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes()
    sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image()
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
    base=0x30000000;u.mem_map(base,0x20000)
    solid=base+0x1000;center=base+0x2000;tensor=center+0x100;result=center+0x200
    stop=base+0x100;sp=base+0x1e000
    captured={}
    def hook(cpu,a,size,data):
        if a==0x4d1670:
            esp=cpu.reg_read(UC_X86_REG_ESP)
            target=struct.unpack('<I',cpu.mem_read(esp,4))[0]
            captured['center']=list(struct.unpack('<3f',cpu.mem_read(center,12)))
            cpu.reg_write(UC_X86_REG_ESP,esp+8);cpu.reg_write(UC_X86_REG_EIP,target)
        elif a==0x4d1cb7:
            captured['inertia']=list(struct.unpack('<9f',cpu.mem_read(tensor,36)))
    u.hook_add(UC_HOOK_CODE,hook,begin=0x4d1670,end=0x4d1670)
    u.hook_add(UC_HOOK_CODE,hook,begin=0x4d1cb7,end=0x4d1cb7)
    configs=[('cube', [((-2,-2,-2),(2,2,2))]),
             ('wide', [((-4,-2,-1),(4,2,1))]),
             ('tall', [((-1,-2,-4),(1,2,4))]),
             ('offset_cut', [((-2,-2,-2),(.8,2,2))]),
             ('split', [((-2,-2,-2),(-.5,2,2)),((.5,-2,-2),(2,2,2))])]
    rows=[]
    for name,boxes in configs:
      for density in [1,2.5,10]:
        captured.clear();u.mem_write(base,bytes(0x20000))
        u.mem_write(stop,b'\xd9\x1d'+struct.pack('<I',result))
        def put(a,fmt,*v):u.mem_write(a,struct.pack('<'+fmt,*v))
        lo=[min(b[0][i] for b in boxes)-.0001 for i in range(3)]
        hi=[max(b[1][i] for b in boxes)+.0001 for i in range(3)]
        put(solid+0x48,'6f',*lo,*hi)
        faces=[];cursor=base+0x3000
        for low,high in boxes:
          for axis in range(3):
            other=[i for i in range(3) if i!=axis]
            for side in range(2):
              face=cursor;cursor+=0x200;faces.append(face)
              normal=[0,0,0];normal[axis]=1 if side else -1
              plane=high[axis] if side else low[axis]
              put(face,'4f',*normal,-normal[axis]*plane)
              flo=list(low);fhi=list(high);flo[axis]=plane-.00001;fhi[axis]=plane+.00001
              put(face+0x10,'6f',*flo,*fhi)
              put(face+0x40,'I',face+0x80)
              for j,(s,t) in enumerate([(0,0),(1,0),(1,1),(0,1)]):
                node=face+0x80+j*0x30;v=node+0x1c
                xyz=[0,0,0];xyz[axis]=plane
                xyz[other[0]]=(low,high)[s][other[0]];xyz[other[1]]=(low,high)[t][other[1]]
                put(node,'I',v);put(v,'3f',*xyz)
                put(node+0x14,'2I',face+0x80+((j+1)%4)*0x30,face+0x80+((j-1)%4)*0x30)
        put(solid+0x70,'I',faces[0])
        for i,face in enumerate(faces):put(face+0x54,'I',faces[i+1] if i+1<len(faces) else 0)
        put(sp,'IIIf',stop,center,tensor,density)
        u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,solid);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x4d1700,stop+6,count=2000000)
        assert u.reg_read(UC_X86_REG_EIP)==stop+6,hex(u.reg_read(UC_X86_REG_EIP))
        row=dict(name=name,density=density,boxes=boxes,grid=list(u.mem_read(solid+0x30c,64)),
                 spacing_origin=list(struct.unpack('<4f',u.mem_read(solid+0x34c,16))),
                 mass=struct.unpack('<f',u.mem_read(result,4))[0],
                 inverse_inertia=list(struct.unpack('<9f',u.mem_read(tensor,36))),**captured)
        assert row['mass']>0 and 'center' in row
        occupancy=sum((v&15).bit_count() for v in row['grid'])/4
        expected_occupancy={'cube':64,'wide':8,'tall':0,'offset_cut':48,'split':64}[name]
        assert occupancy==expected_occupancy,(name,occupancy)
        spacing=row['spacing_origin'][0]
        expected_mass=(occupancy*spacing**3*density if occupancy else
                       .5*density*(hi[0]-lo[0])*(hi[1]-lo[1])*(hi[2]-lo[2]))
        assert abs(row['mass']-expected_mass)<expected_mass*2e-6
        if not occupancy:
            assert row['inertia']==[0.0]*9 and row['inverse_inertia']==[0.0]*9
        else:
            for i in range(3):
                for j in range(3):
                    product=sum(row['inertia'][3*i+k]*row['inverse_inertia'][3*k+j] for k in range(3))
                    assert abs(product-(i==j))<2e-6,(name,i,j,product)
            assert abs(row['center'][0]-(-.5*spacing if name=='offset_cut' else 0))<1e-6
            assert max(abs(v) for v in row['center'][1:])<1e-6
        rows.append(row)
    out=ROOT/'artifacts/geomod-postedit-re/solid-mass.json'
    out.write_text(json.dumps(dict(original_sha256=sha,cases=rows,scope=__doc__),indent=2)+'\n')
    def words(values):
        return ','.join(str(v)+'u' for v in struct.unpack('<'+'I'*len(values),struct.pack('<'+'f'*len(values),*values)))
    fixture=[]
    for r in rows:
        bounds=[v for b in r['boxes'] for side in b for v in side]
        output=r['spacing_origin']+r['center']+[r['mass']]+r['inverse_inertia']
        fixture.append(' {'+str(len(r['boxes']))+'u,{'+words(bounds)+'},'+words([r['density']])+', {'+','.join(str(v) for v in r['grid'])+'},{'+words(output)+'}},')
    (ROOT/'tests/fixtures/geomod_solid_mass.inc').write_text('/* Original4d1700; only mesh recenter mutation intercepted. */\n'+'\n'.join(fixture)+'\n')
    for r in rows:print(r['name'],r['density'],'mass',r['mass'],'center',r['center'])
    print('PASS',len(rows),'original solid-grid mass/inertia executions')
if __name__=='__main__':main()
