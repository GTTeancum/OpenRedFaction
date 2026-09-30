"""Check RFWM1 compatibility and RFWM2 durability using compiled NXDK code."""
import json
from pathlib import Path
import re
import struct
import sys
import pefile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP

def main():
    pe=pefile.PE(str(ROOT/'build/xbox/main.exe'));blob=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
    cpu=Uc(UC_ARCH_X86,UC_MODE_32);cpu.mem_map(origin,(len(blob)+4095)//4096*4096);cpu.mem_write(origin,blob)
    base=0x30000000;cpu.mem_map(base,0x20000);stack=base+0x18000;stop=base+0x19000
    state,encoded,decoded,written=base,base+256,base+512,base+768
    mapping=(ROOT/'build/xbox/main.map').read_text()
    def call(name,*args):
        entry=int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
        cpu.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.emu_start(entry,stop,count=1000000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop
        return cpu.reg_read(UC_X86_REG_EAX)
    cases=[]
    for flags,life,version in [(3,0.,1),(4,137.5,2),(7,1.,2)]:
        value=struct.pack('<IIf',flags,0x12345678,life);cpu.mem_write(state,value)
        assert call('rf_weapon_modes_checkpoint_encode',123,state,encoded,32,written)==0
        assert struct.unpack('<I',cpu.mem_read(encoded+4,4))[0]==version
        assert call('rf_weapon_modes_checkpoint_decode',encoded,32,123,decoded)==0
        assert bytes(cpu.mem_read(decoded,12))==value
        cases.append(f'RFWM{version} flags{flags} durability{life} round trip')
    sentinel=b'\xa5'*12
    for flags,life in [(4,0.),(4,-1.),(4,float('nan')),(0,5.)]:
        data=bytearray(b'RFWM'+struct.pack('<6If',2,32,0,123,flags,0,life))
        checksum=2166136261
        for byte in data:checksum=((checksum^byte)*16777619)&0xffffffff
        struct.pack_into('<I',data,12,checksum)
        cpu.mem_write(encoded,bytes(data));cpu.mem_write(decoded,sentinel)
        assert call('rf_weapon_modes_checkpoint_decode',encoded,32,123,decoded)==0xfffffffe
        assert bytes(cpu.mem_read(decoded,12))==sentinel
    cases.append('zero, negative, NaN and unowned durability reject without publication')
    report={'result':'PASS','scope':'Compiled NXDK save codec; no PC runtime','cases':cases}
    (ROOT/'artifacts/xbox-weapon-modes-codec.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
if __name__=='__main__':main()
