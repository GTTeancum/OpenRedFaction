"""Verify the original lightmap static-relighting entry gate without replacing branches.

Execution stops at the light query or the post-relight label. This establishes
static dirty/inhibit gating only. Dirty bit1 has a later dynamic-light path
that this stop point does not execute; scheduling, callers and pixels are excluded.
"""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP
from extract_geomod_template import SHA

def main():
    exe=ROOT/'Installed_Game/RF.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
    image=pefile.PE(str(exe)).get_memory_mapped_image()
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
    base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,65536)
    pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
    reached=[]
    def hook(cpu,address,size,data):
        if address in (0x4d9c00,0x4f2c79,stop):
            reached.append(address);cpu.emu_stop()
    u.hook_add(UC_HOOK_CODE,hook)
    rows=[]
    for flags in range(256):
        for inhibit in (0,1):
            reached.clear();u.mem_write(base,bytes(128));u.mem_write(base+8,bytes((flags,0,inhibit)))
            u.mem_write(base+0x18,pack(4,4));u.mem_write(base+0x68,pack(0xffffffff))
            u.mem_write(stack,pack(stop,0,0));u.reg_write(UC_X86_REG_ECX,base);u.reg_write(UC_X86_REG_ESP,stack)
            u.emu_start(0x4f26a0,stop+1,count=1000)
            expected=0x4d9c00 if flags&6 and not inhibit else 0x4f2c79
            assert reached==[expected],(flags,inhibit,reached)
            rows.append(dict(dirty=flags,inhibit=inhibit,queries_static_lights=expected==0x4d9c00))
    (ROOT/'artifacts/geomod-relight-gate-original.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=SHA,cases=rows),indent=2)+'\n')
    print('PASS: 512 original dirty/inhibit combinations; initial dirty8 bypasses static relighting')

if __name__=='__main__':main()
