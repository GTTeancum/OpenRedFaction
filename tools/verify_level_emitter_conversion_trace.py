"""Original 45fcf0 conversion with decoded field and bitmap services supplied."""
import runpy,struct,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,root,base,stack,stop=(c[k] for k in ('u','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
u.mem_map(0,4096)
trampoline=base+0x800;float_value=base+0x900
u.mem_write(trampoline,b'\xd9\x05'+struct.pack('<I',float_value)+b'\xc2\x08\x00')
def ret(value,cleanup):
    sp=u.reg_read(UC_X86_REG_ESP);target=struct.unpack('<I',u.mem_read(sp,4))[0]
    u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);u.reg_write(UC_X86_REG_EIP,target)
integers=[];floats=[];vectors=[];colors=[];booleans=[];captured=[]
def hook(m,a,size,data):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x52c910:ret(integers.pop(0),8)
    elif a==0x52c9b0:
        m.mem_write(float_value,struct.pack('<f',floats.pop(0)));m.reg_write(UC_X86_REG_EIP,trampoline)
    elif a==0x52c780:ret(booleans.pop(0)!=0,8)
    elif a==0x52cc10:ret(0,12)
    elif a==0x52ca00:
        output=struct.unpack('<I',m.mem_read(sp+4,4))[0];m.mem_write(output,struct.pack('<3f',*vectors.pop(0)));ret(output,12)
    elif a==0x52d170:
        output=struct.unpack('<I',m.mem_read(sp+4,4))[0];m.mem_write(output,bytes(colors.pop(0)));ret(output,12)
    elif a==0x523990:ret(1,4)
    elif a==0x5239c0:ret(180,0)
    elif a==0x45fe08:
        # Bypass string/extension/resource loading with a resolved bitmap/frame count.
        m.mem_write(sp+0x88,struct.pack('<2I',23,1));m.reg_write(UC_X86_REG_EIP,0x45fe97)
    elif a==0x497ca0:
        args=struct.unpack('<5I',m.mem_read(sp+4,20));captured.append((args,bytes(m.mem_read(args[1],132))));ret(0,0)
    elif a==0x4ff470:ret(0,0)
u.hook_add(UC_HOOK_CODE,hook)
inventory=json.loads((root/'artifacts/level-emitters.json').read_text());results=[]
for level in inventory['results']:
    for record in level['records']:
        for fill in (0,0xa5):
            integers[:]=[1,record['uid'],record['header_word'],record['emitter_flags'],record['particle_flags']]
            floats[:]=[record['spawn_radius'],*record['unknown_floats'],*record['delay'],*record['speed'],record['acceleration'],*record['life'],*record['radius'],record['growth'],record['gravity_scale'],record['cone_angle'],*record['cycle'],record['finish_age']]
            vectors[:]=[record['position'],record['orientation_disk'][:3],record['orientation_disk'][3:6],record['orientation_disk'][6:]]
            colors[:]=[record['color'],record['color_destination']];booleans[:]=[record['header_byte'],record['enabled']];captured.clear()
            u.mem_write(stack-0x1000,bytes([fill])*0x1000);u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack)
            u.emu_start(0x45fcf0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
            assert not any((integers,floats,vectors,colors,booleans)) and len(captured)==1
            args,template=captured[0]
            assert args[0]==0 and args[2:4]==(0,0) and args[4]&255==bool(record['enabled'])
            assert template[16:28]==struct.pack('<3f',*record['orientation_disk'][6:])
            assert template[124:128]==bytes([fill])*4
            assert template[128:132]==struct.pack('<f',record['finish_age'])
            assert template[120:124]==bytes(4)
            results.append(dict(level=level['file'],uid=record['uid'],fill=fill,template=template.hex(),enabled=args[4]&255))
report=dict(result='PASS',records=len(results)//2,replays=len(results),scope='Original 45fcf0 arithmetic, actual 52cac0 orientation ordering and vector copies execute; decoded field reads and bitmap services supplied, allocation captured. Reveals third serialized orientation vector, preserved template +7c, final float at +80. Not original byte-stream parsing, texture loading or campaign creation.')
(root/'artifacts/level-emitter-conversion-trace.json').write_text(json.dumps(dict(report=report,results=results),indent=2)+'\n');print(report)
