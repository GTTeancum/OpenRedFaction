"""Execute original new-face lightmap fill; supply only CRT random draws.

4e5bb0..4e5c25 runs its original strided RGB stores and loop arithmetic.
Mapping dimensions/allocation and global RNG scheduling are not recovered here.
"""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI
from extract_geomod_template import SHA

def main():
    exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    pe=pefile.PE(str(exe));image=pe.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
    base=0x30000000;header=base+0x100;pixels=base+0x1000;stack=base+0xe000
    u.mem_map(base,65536);pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
    state=draws=0
    def hook(cpu,address,size,context):
        nonlocal state,draws
        if address!=0x57312d:return
        state=(state*214013+2531011)&0xffffffff;draws+=1
        sp=cpu.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',cpu.mem_read(sp,4))[0]
        cpu.reg_write(UC_X86_REG_EAX,(state>>16)&32767)
        cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
    u.hook_add(UC_HOOK_CODE,hook);rows=[]
    for seed in (0,1,7654321,0xffffffff):
      for width in (1,4,16,64):
       for height in (1,3,32,64):
        pitch=(width+4)*3;size=pitch*(height+2);state=seed;draws=0
        u.mem_write(base,bytes(128));u.mem_write(base+12,pack(header,2,1,width,height))
        u.mem_write(header,pack(0,width+4,height+2,pixels));u.mem_write(pixels,bytes([165])*size)
        u.mem_write(stack,bytes(128));u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack)
        u.emu_start(0x4e5bb0,0x4e5c25,count=1000000)
        assert u.reg_read(UC_X86_REG_EIP)==0x4e5c25 and draws==width*height
        raw=bytes(u.mem_read(pixels,size));expected=b''.join(raw[(y+1)*pitch+6:(y+1)*pitch+6+width*3] for y in range(height))
        values=subprocess.check_output([str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),'--light-noise',str(seed),str(width),str(height)],text=True).split()
        assert int(values[0])==state and bytes(map(int,values[1:]))==expected
        guards=bytearray(raw)
        for y in range(height):guards[(y+1)*pitch+6:(y+1)*pitch+6+width*3]=bytes([165])*(width*3)
        assert guards==bytes([165])*size
        assert all(expected[i]==expected[i+1]==expected[i+2] and 32<=expected[i]<=95 for i in range(0,len(expected),3))
        rows.append(dict(seed=seed,width=width,height=height,state=state,draws=draws,sha256=hashlib.sha256(expected).hexdigest()))
    (ROOT/'artifacts/geomod-light-noise-original.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=SHA,cases=rows),indent=2)+'\n')
    print('PASS:',len(rows),'original/shared new-face lightmap fills, including strided original guards')

if __name__=='__main__':main()
