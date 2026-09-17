"""Replay authored destruction, then execute original wet placement on its live inputs."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW


def main():
    folder=ROOT/'artifacts/wet-debris-live';folder.mkdir(parents=True,exist_ok=True)
    exe=ROOT/'build/pc/Release/rf_pc_play.exe'
    replay=ROOT/'artifacts/authored-post-live/two-shot.bin'
    env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0')
    with (folder/'two-shot.log').open('wb') as log:
        run=subprocess.run([str(exe),'--spawn-replay',str(ROOT/'Installed_Game'),str(replay),str(folder/'two-shot.ppm')],
            cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
    assert run.returncode==0
    lines=(folder/'two-shot.log').read_text().splitlines()
    samples=[list(map(int,line.split()[1:])) for line in lines if line.startswith('DEBRIS_WET_SAMPLE ')]
    state=list(map(int,next(line for line in lines if line.startswith('DEBRIS_WET_STATE ')).split()[1:]))
    assert len(samples)==state[2] and len(samples)>0 and state[6]==0
    original=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(original.read_bytes()).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    pe=pefile.PE(str(original));image=pe.get_memory_mapped_image()
    cpu=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase
    cpu.mem_map(base,(len(image)+4095)&~4095);cpu.mem_write(base,image)
    arena=0x30000000;cpu.mem_map(arena,65536)
    start,end,room,roomptr,out,stack,stop=[arena+x for x in (0x2000,0x2100,0x3000,0x2200,0x2300,0xe000,0xff00)]
    pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
    word=lambda at:struct.unpack('<I',cpu.mem_read(at,4))[0]
    calls=[]
    def solid_miss(machine,address,size,context):
        if address!=0x4df1c0:return
        sp=machine.reg_read(UC_X86_REG_ESP);query=word(sp+4);result=word(sp+8)
        assert word(query+0x50)==5 and word(query+0x4c)==0
        calls.append(1);machine.mem_write(result,bytes(40))
        machine.reg_write(UC_X86_REG_EIP,word(sp));machine.reg_write(UC_X86_REG_ESP,sp+16)
    cpu.hook_add(UC_HOOK_CODE,solid_miss);rows=[]
    for sample in samples:
        roomid,*values=sample;assert len(values)==12
        cpu.mem_write(room,bytes(0x200));cpu.mem_write(room+0x184,b'\1')
        cpu.mem_write(room+0x188,pack(values[6]));cpu.mem_write(room+12,pack(values[7]))
        cpu.mem_write(roomptr,pack(room));cpu.mem_write(start,pack(*values[:3]));cpu.mem_write(end,pack(*values[3:6]))
        cpu.mem_write(out,bytes([165])*20);cpu.mem_write(stack,pack(stop,start,end,roomptr,out,0))
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f);calls.clear()
        cpu.emu_start(0x48fc10,stop,count=10000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop and cpu.reg_read(UC_X86_REG_EAX)&255==1 and len(calls)==1
        actual=bytes(cpu.mem_read(out,20));assert actual==pack(0,*values[8:])
        rows.append(dict(room=roomid,input_words=values[:8],fraction_point_words=values[8:]))
    report=dict(result='PASS',original_sha256=sha,binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
        replay_sha256=hashlib.sha256(replay.read_bytes()).hexdigest(),state=state,cases=rows,
        scope='Actual live accepted wet births versus complete original48fc10 and its math helpers. Solid miss is supplied after the live scene already reports miss; no full original geometry execution, ongoing bounce, audible splash or visual parity proof.')
    (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS:',len(rows),'actual wet birth positions/fractions match original instructions bit-for-bit; state',state)


if __name__=='__main__':main()
