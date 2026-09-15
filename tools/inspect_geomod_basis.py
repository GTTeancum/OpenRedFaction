"""Compare shared crater orientation with original4fccc0 machine code.

Only the CRT integer random source is supplied; vector sampling and basis math
execute unmodified. This does not establish the live game's RNG stream/seed.
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
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_FPCW
from extract_geomod_template import SHA

def run():
    exe=ROOT/'Installed_Game/RF.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    pe=pefile.PE(str(exe));image=pe.get_memory_mapped_image()
    cpu=Uc(UC_ARCH_X86,UC_MODE_32)
    cpu.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(image)+4095)&~4095)
    cpu.mem_write(pe.OPTIONAL_HEADER.ImageBase,image)
    base=0x30000000;cpu.mem_map(base,0x10000);stack=base+0xe000;stop=base+0xf000
    state=0;draws=0
    def hook(u,address,size,_):
        nonlocal state,draws
        if address!=0x57312d:return
        state=(state*214013+2531011)&0xffffffff;draws+=1
        sp=u.reg_read(UC_X86_REG_ESP)
        u.reg_write(UC_X86_REG_EAX,(state>>16)&32767)
        u.reg_write(UC_X86_REG_EIP,struct.unpack('<I',u.mem_read(sp,4))[0])
        u.reg_write(UC_X86_REG_ESP,sp+4)
    cpu.hook_add(UC_HOOK_CODE,hook)
    rows=[];worst=0
    # Solve CRT seeds whose sphere direction is near +/-Y, exercising the
    # special4fcfa0 branch instead of hoping ordinary random cases reach it.
    vertical=[];inverse=pow(214013,-1,2**32)
    for target in (8192,24576):
        for low in range(65536):
            first=(16384<<16)|low
            second=(first*214013+2531011)&0xffffffff
            if ((second>>16)&32767)==target:
                vertical.append(((first-2531011)*inverse)&0xffffffff)
                break
        else:raise AssertionError('No vertical fixture seed')
    for seed in [0,1,0xffffffff]+[i*1234567 for i in range(1,65)]+vertical:
        state=seed;draws=0
        cpu.mem_write(base,bytes(36));cpu.mem_write(stack,struct.pack('<I',stop))
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,base);cpu.reg_write(UC_X86_REG_FPCW,0x37f)
        cpu.emu_start(0x4fccc0,stop,count=100000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop and draws==2
        original=struct.unpack('<9f',cpu.mem_read(base,36))
        output=subprocess.check_output([str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),str(seed)],text=True).split()
        assert int(output[0])==state
        shared=list(map(float,output[1:]));error=max(abs(a-b) for a,b in zip(original,shared))
        assert len(shared)==9 and struct.pack("<9f",*shared)==bytes(cpu.mem_read(base,36)),(seed,error)
        if seed in vertical:assert original[6]==original[8]==0 and abs(original[7])==1
        worst=max(worst,error);rows.append(dict(seed=seed,state=state,original=original,shared=shared,error=error,vertical=seed in vertical))
    report=dict(exe_sha256=SHA,entry='004fccc0',scope=__doc__,bit_exact=True,max_text_roundtrip_error=worst,cases=rows)
    (ROOT/'artifacts/geomod-basis-original.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',len(rows),'original random bases; two draws/state match; maximum component error',worst)

if __name__=='__main__':run()
