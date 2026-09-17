"""Execute466440 and4130b0 through the generic object allocator boundary."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
import itertools

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    im=pefile.PE(data=raw).get_memory_mapped_image();rows=[]
    for radius,origin,sound in itertools.product((.5,1.5,3,3.0001,10),((0,0,0),(.1,-.2,.3),(65536,-32768,16384)),(0,42)):
        u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,4096)
        u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im)
        base=0x30000000;u.mem_map(base,65536);u.mem_write(base,bytes([165])*65536)
        piece=base+0x1000;position=base+0x2000;sp=base+0xe000;captured={};lookups=[]
        def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
        def ret(pop=0,value=None):
            esp=u.reg_read(UC_X86_REG_ESP);target=get(esp)
            u.reg_write(UC_X86_REG_ESP,esp+4+pop)
            if value is not None:u.reg_write(UC_X86_REG_EAX,value)
            u.reg_write(UC_X86_REG_EIP,target)
        def hook(cpu,a,size,data):
            esp=cpu.reg_read(UC_X86_REG_ESP)
            if a==0x4130b0:captured['piece_descriptor']=list(struct.unpack('<25I',cpu.mem_read(get(esp+12),100)))
            elif a==0x4ff3b0:ret()
            elif a==0x4ffa80:ret(4)
            elif a==0x4ff480:ret(value=0x59ca54)
            elif a==0x4689a0:lookups.append(get(esp+4));ret(value=sound)
            elif a==0x4ff470:ret()
            elif a==0x486da0:
                captured['arguments']=[get(esp+4+i*4) for i in range(6)]
                pointer=get(esp+16);words=list(struct.unpack('<38I',cpu.mem_read(pointer,152)))
                captured['object_descriptor']=words;cpu.emu_stop()
        for a in (0x4130b0,0x4ff3b0,0x4ffa80,0x4ff480,0x4689a0,0x4ff470,0x486da0):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
        u.mem_write(piece+0x60,struct.pack('<f',radius));u.mem_write(position,struct.pack('<3f',*origin))
        u.mem_write(sp,struct.pack('<3I',base+0xff00,piece,position));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_FPCW,0x27f)
        u.emu_start(0x466440,base+0xff00,count=100000)
        assert u.reg_read(UC_X86_REG_EIP)==0x486da0
        assert len(lookups)==int(radius>3)
        fword=lambda value:struct.unpack('<I',struct.pack('<f',value))[0]
        identity=[fword(1 if i in (0,4,8) else 0) for i in range(9)]
        expected_piece=[0]*25;expected_piece[:3]=[fword(x) for x in origin];expected_piece[3:12]=identity
        expected_piece[18:25]=[0xffffffff,1,0xffffffff,0x40000018,0,0,sound if radius>3 else 0]
        assert captured['piece_descriptor']==expected_piece
        expected_object=[0]*38
        radius32=struct.unpack('<f',struct.pack('<f',radius))[0];factor=struct.unpack('<f',struct.pack('<f',.2))[0]
        expected_object[2:6]=[piece,fword(radius32*factor),1,fword(-1)]
        expected_object[15:18]=[fword(x) for x in origin];expected_object[18:27]=identity
        expected_object[33]=fword(radius);expected_object[37]=0x8000003f
        assert captured['object_descriptor']==expected_object
        assert captured['arguments'][:3]==[3,0xffffffff,0xffffffff] and captured['arguments'][4:]==[0,0]
        captured.update(radius=radius,origin=origin,sound=sound,lookups=len(lookups));rows.append(captured)
    (ROOT/'artifacts/geomod-postedit-re/piece-birth.json').write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,scope='Real piece constructor and generic descriptor preparation; string/audio lookup supplied; stop before generic allocation, physics and registration.'),indent=2)+'\n')
    print('PASS',len(rows),'original piece birth descriptor captures')
if __name__=='__main__':main()
