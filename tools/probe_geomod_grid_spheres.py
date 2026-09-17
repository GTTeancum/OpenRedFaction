"""Execute49ee3d..49efa4 after mass-grid generation; supply only list append."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_FPCW

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    im=pefile.PE(data=raw).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im)
    base=0x30000000;u.mem_map(base,65536);descriptor=base+0x1000;solid=base+0x2000;sp=base+0xe000;spheres=[]
    def hook(cpu,a,size,data):
        esp=cpu.reg_read(UC_X86_REG_ESP)
        spheres.append(list(struct.unpack('<5I',cpu.mem_read(esp+4,20))))
        target=struct.unpack('<I',cpu.mem_read(esp,4))[0];cpu.reg_write(UC_X86_REG_ESP,esp+28);cpu.reg_write(UC_X86_REG_EIP,target)
    u.hook_add(UC_HOOK_CODE,hook,begin=0x417f30,end=0x417f30)
    configs=[([n]*64,(1,0,0,0)) for n in range(16)]
    configs += [([0xf0+n]*64,(.1,-.2,.3,-.4)) for n in range(16)]
    for i in range(64):
        grid=[0]*64;grid[i]=3 if i&1 else 15;configs.append((grid,(2.5,-3.75,-3.75,-3.75)))
    rows=[]
    for grid,values in configs:
        spheres.clear();u.mem_write(base,bytes([165])*65536)
        u.mem_write(descriptor+8,struct.pack('<I',solid));u.mem_write(solid+0x30c,bytes(grid));u.mem_write(solid+0x34c,struct.pack('<4f',*values))
        u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ESI,descriptor);u.reg_write(UC_X86_REG_EDI,0);u.reg_write(UC_X86_REG_EBP,descriptor+0x88);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x49ee3d,0x49efa4,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==0x49efa4
        assert len(spheres)==sum((v&15).bit_count()>1 for v in grid)
        assert all(s[4]==0xbf800000 for s in spheres)
        radius=struct.unpack('<I',u.mem_read(descriptor+0x84,4))[0]
        rows.append(dict(grid=grid,inputs=list(struct.unpack('<4I',struct.pack('<4f',*values))),count=len(spheres),radius=radius,spheres=spheres.copy()))
    (ROOT/'artifacts/geomod-postedit-re/grid-spheres.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,scope='Real grid enumeration, sphere constructor/copy and bound arithmetic; grid generation supplied and vector-list append intercepted.'),indent=2)+'\n')
    def words(v):return ','.join(str(x)+'u' for x in v)
    (ROOT/'tests/fixtures/geomod_grid_spheres.inc').write_text('/* Original49ee3d..49efa4; opaque sphere word omitted. */\n'+''.join(' {{'+words(r['grid'])+'},{'+words(r['inputs'])+'},'+str(r['count'])+'u,'+str(r['radius'])+'u,{'+(','.join('{'+words(s)+'}' for s in r['spheres']) or '{0}')+'}},\n' for r in rows))
    print('PASS',len(rows),'original grid-to-sphere cases and all64 cell positions')
if __name__=='__main__':main()
