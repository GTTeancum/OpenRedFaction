"""Execute full original4e1180 using real face/ray classification on box shells."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
import itertools

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();rows=[]
    for center,extent,invert,selector in itertools.product([(0,0,0),(13,-8,6)],[(1,1,1),(.25,2,3)],[0,1],[0,-1,99]):
        u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
        b=0x30000000;u.mem_map(b,0x100000);w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]);f=lambda *v:struct.pack('<'+'f'*len(v),*v)
        faces=[];wire=[];low=[center[i]-extent[i] for i in range(3)];high=[center[i]+extent[i] for i in range(3)]
        for axis,side in itertools.product(range(3),[-1,1]):
            n=len(faces);face=b+0x1000+n*0x400;faces.append(face);other=[i for i in range(3) if i!=axis]
            points=[]
            for a,c in [(-1,-1),(1,-1),(1,1),(-1,1)]:
                p=list(center);p[axis]+=side*extent[axis];p[other[0]]+=a*extent[other[0]];p[other[1]]+=c*extent[other[1]];points.append(p)
            sign=side*(-1 if invert else 1);normal=[0,0,0];normal[axis]=sign
            plane=normal+[-sign*points[0][axis]]
            wire.extend(plane+[v for p in points for v in p])
            lo=[min(p[i] for p in points) for i in range(3)];hi=[max(p[i] for p in points) for i in range(3)]
            u.mem_write(face,f(*plane,*lo,*hi));u.mem_write(face+0x2c,w(0));u.mem_write(face+0x40,w(face+0x200))
            for j,p in enumerate(points):
                u.mem_write(face+0x100+j*12,f(*p));u.mem_write(face+0x200+j*32,w(face+0x100+j*12,0,0,0,0,face+0x200+(j+1)%4*32,face+0x200+(j-1)%4*32))
        following={face:faces[i+1] if i+1<len(faces) else 0 for i,face in enumerate(faces)}
        u.mem_write(b+0x70,w(faces[0]));stack=b+0xf0000;stop=b+0xff000;u.mem_write(stack,w(stop,selector));queries=[];face_calls=[]
        def ret(value=0,pop=0):
            sp=u.reg_read(UC_X86_REG_ESP);address=struct.unpack('<I',u.mem_read(sp,4))[0]
            u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,address)
        def hook(cpu,address,size,context):
            sp=cpu.reg_read(UC_X86_REG_ESP)
            if address==0x4d08f0:
                group,lo,hi=struct.unpack('<3I',cpu.mem_read(sp+4,12));assert group==selector&0xffffffff
                cpu.mem_write(lo,f(*low));cpu.mem_write(hi,f(*high));ret(0,12)
            elif address==0x45ec30:ret(following[struct.unpack('<I',cpu.mem_read(sp+4,4))[0]],4)
            elif address==0x4e3780:
                ref,origin,direction,distance,flags=struct.unpack('<5I',cpu.mem_read(sp+4,20));assert ref==0
                queries.append(dict(origin=list(struct.unpack('<3f',cpu.mem_read(origin,12))),direction=list(struct.unpack('<3f',cpu.mem_read(direction,12))),distance=struct.unpack('<f',w(distance))[0]))
            elif address==0x4e3800:face_calls.append(struct.unpack('<I',cpu.mem_read(sp+4,4))[0])
        u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,b);u.reg_write(UC_X86_REG_FPCW,0x27f)
        try:u.emu_start(0x4e1180,stop,count=200000)
        except Exception:print('failed at',hex(u.reg_read(UC_X86_REG_EIP)));raise
        assert u.reg_read(UC_X86_REG_EIP)==stop
        result=u.reg_read(UC_X86_REG_EAX)&255
        assert result==int(not invert and selector!=99)
        assert len(queries)==int(selector!=99)
        rows.append(dict(center=center,extent=extent,inverted=invert,selector=selector,result=result,queries=queries,face_calls=len(face_calls),
                         words=list(struct.unpack('<102I',f(*low,*high,*wire)))))
    out=ROOT/'artifacts/geomod-postedit-re/component-classification.json'
    out.write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,scope='Complete4e1180 and real centroid/ray/face/final classifier; bounds and linked iterator supplied. Box shells only.'),indent=2)+'\n')
    (ROOT/'tests/fixtures/geomod_component_classify.inc').write_text('/* Complete original4e1180 on supplied box shells. */\n'+''.join(
        ' {{'+','.join(str(v)+'u' for v in r['words'])+'},'+str(r['selector'])+','+str(r['result'])+'u},\n' for r in rows))
    print('PASS',len(rows),'original component classifications')

if __name__=='__main__':main()
