"""Execute original stage-2 piece dispatch; extraction/placement are supplied."""
from probe_debris_motion import ROOT, hashlib, json, struct, pefile, Uc, UC_ARCH_X86, UC_MODE_32
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EBP, UC_X86_REG_FPCW

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes()
    sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image(); rows=[]
    for enabled in (0,1,2):
        for capacity in (0,1,3):
            u=Uc(UC_ARCH_X86,UC_MODE_32)
            u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
            base=0x30000000;u.mem_map(base,65536)
            def put(a,v):u.mem_write(a,struct.pack('<I',v))
            def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
            free=0x6485d0;active=0x647c30
            nodes=[base+0x1000+i*0x100 for i in range(capacity)]
            for i,p in enumerate([free]+nodes):
                chain=[free]+nodes;put(p,chain[(i+1)%len(chain)]);put(p+4,chain[i-1])
            put(active,active);put(active+4,active)
            put(0x649604,0);put(0x59c9f4,2);u.mem_write(0x647c28,bytes([enabled]))
            calls=[];published=[];destroyed=[];finish=[]
            def ret(pop=0,value=None):
                sp=u.reg_read(UC_X86_REG_ESP);target=get(sp)
                u.reg_write(UC_X86_REG_ESP,sp+4+pop)
                if value is not None:u.reg_write(UC_X86_REG_EAX,value)
                u.reg_write(UC_X86_REG_EIP,target)
            def hook(uc,address,size,data):
                sp=uc.reg_read(UC_X86_REG_ESP)
                if address==0x4d0990:ret(value=3)
                elif address==0x409f90:ret()
                elif address==0x4d0590:
                    label=get(sp+4);calls.append(label);piece=base+0x2000+label*0x400
                    uc.mem_write(piece+0x48,struct.pack('<6f',-1,-2,-3,1,2,3));ret(4,piece)
                elif address==0x4d1330:
                    uc.mem_write(get(sp+4),struct.pack('<3f',0,0,0));ret(4)
                elif address==0x466550:published.append(get(sp+4));ret()
                elif address==0x4136e0:destroyed.append(uc.reg_read(UC_X86_REG_ECX));ret(4)
                elif address in (0x4666a0,0x4f0b90):finish.append(address);ret()
            addresses=(0x4d0990,0x409f90,0x4d0590,0x4d1330,0x466550,0x4136e0,0x4666a0,0x4f0b90)
            for a in addresses:u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
            u.reg_write(UC_X86_REG_ESP,base+0xe000);u.reg_write(UC_X86_REG_EBP,0);u.reg_write(UC_X86_REG_FPCW,0x27f)
            u.emu_start(0x466dcd,0x466f4c,count=100000)
            assert u.reg_read(UC_X86_REG_EIP)==0x466f4c
            accepted=capacity if enabled==1 else 0
            assert calls==[0,1,2] and published==nodes[:accepted]
            assert destroyed==[base+0x2000+i*0x400 for i in range(accepted,3)]
            assert get(0x649604)==3 and get(0x59c9f4)==3
            assert finish==([0x4666a0] if enabled else [])+[0x4f0b90]
            for i,p in enumerate(published):
                assert get(p+8)==base+0x2000+i*0x400
                assert get(p+4)==(active if i==0 else published[i-1])
                assert get(p)==(active if i+1==len(published) else published[i+1])
            assert get(active)==(published[0] if published else active)
            assert get(active+4)==(published[-1] if published else active)
            rows.append(dict(enabled=enabled,pool_capacity=capacity,extraction_labels=calls,published=len(published),destroyed=len(destroyed),changed_boxes=get(0x649604)))
    out=ROOT/'artifacts/geomod-postedit-re/piece-dispatch.json'
    out.write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,scope='Real stage-2 control/list ownership and bounds arithmetic; selection, extraction, placement, initialization, destruction and finish services supplied.'),indent=2)+'\n')
    print('PASS 9 original piece dispatch cases: indexed extraction, bounded owner pool, destruction fallback, changed boxes')

if __name__=='__main__':main()
