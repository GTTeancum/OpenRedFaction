"""Compare full original4f1f30 with shared mapped-face dirty marking.

Only mapping-array lookup is supplied by the harness. Vector expansion,
inclusive bounds comparisons, existing-dirty gates and writes execute original
instructions. Caller traversal and live rendering are outside this check.
"""
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'local/python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from extract_geomod_template import SHA


def main():
    exe = ROOT/'Installed_Game/RF.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == SHA
    image = pefile.PE(str(exe)).get_memory_mapped_image()
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x400000, (len(image)+4095)&~4095)
    cpu.mem_write(0x400000, image)
    base = 0x30000000
    face, mapping, slot, solid, center, stack, stop = [base+n for n in (0, 256, 512, 1024, 2048, 0xe000, 0xf000)]
    cpu.mem_map(base, 65536)
    lookup = []
    def hook(machine, address, size, data):
        if address == stop:
            machine.emu_stop()
        elif address == 0x40a480:
            esp = machine.reg_read(UC_X86_REG_ESP)
            target, index = struct.unpack('<II', machine.mem_read(esp, 8))
            assert machine.reg_read(UC_X86_REG_ECX) == solid+0xc0
            lookup.append(index)
            machine.reg_write(UC_X86_REG_EAX, slot)
            machine.reg_write(UC_X86_REG_ESP, esp+8)
            machine.reg_write(UC_X86_REG_EIP, target)
    cpu.hook_add(UC_HOOK_CODE, hook)
    inputs, expected, rows = bytearray(), [], []
    points = [(0,0,0),(-3,0,0),(3,0,0),(3.000001,0,0),(0,-4,0),(0,4.000001,0),(0,0,-5),(0,0,5.000001)]
    for index in (-1, 0, 32767):
        for dirty in range(256):
            for point in points:
                lookup.clear()
                cpu.mem_write(base, bytes(4096))
                cpu.mem_write(face+0x10, struct.pack('<6f',-1,-2,-3,1,2,3))
                cpu.mem_write(face+0x36, struct.pack('<h',index))
                cpu.mem_write(mapping+8, bytes((dirty,)))
                cpu.mem_write(slot, struct.pack('<I',mapping))
                cpu.mem_write(center, struct.pack('<3f',*point))
                cpu.mem_write(stack, struct.pack('<IIfII',stop,center,2,solid,face))
                cpu.reg_write(UC_X86_REG_ESP,stack)
                cpu.reg_write(UC_X86_REG_FPCW,0x37f)
                cpu.emu_start(0x4f1f30,stop+1,count=2000)
                assert cpu.reg_read(UC_X86_REG_EIP)==stop
                assert lookup == ([] if index<0 else [index])
                value=cpu.mem_read(mapping+8,1)[0]
                inputs.extend(struct.pack('<iI10f',index,dirty,-1,-2,-3,1,2,3,*point,2))
                expected.append((0,value))
                rows.append(dict(index=index,dirty=dirty,point=point,result=value))
    shared=subprocess.run([str(ROOT/'build/pc/Release/rf_lightmap_probe.exe'),'--dynamic-mark'],
                          input=inputs,capture_output=True,check=True,cwd=ROOT)
    assert list(struct.iter_unpack('<ii',shared.stdout))==expected
    guards=struct.pack('<iI10f',0,0,-1,-2,-3,1,2,3,0,0,0,-1)+struct.pack('<iI10f',0,0,-1,-2,-3,1,2,3,float('nan'),0,0,2)
    result=subprocess.run([str(ROOT/'build/pc/Release/rf_lightmap_probe.exe'),'--dynamic-mark'],input=guards,capture_output=True,check=True,cwd=ROOT)
    assert list(struct.iter_unpack('<ii',result.stdout))==[(-2,0),(-2,0)]
    (ROOT/'artifacts/lightmap-dynamic-mark-original.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=SHA,cases=rows),indent=2)+'\n')
    print('PASS:',len(rows),'original/shared mapped-face cases; negative-radius and NaN guards preserve dirty')


if __name__ == '__main__':
    main()
