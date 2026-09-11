"""Original sample-loader routing and stream cleanup at resource boundaries."""
import itertools,json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
e=runpy.run_path(str(root/'tools/verify_audio_registration.py'));m,u=e['m'],e['u']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
trace=[];returns={};index=0
arities={0x563370:6,0x5635d0:3,0x5636e0:1,0x521db0:1,0x521f90:1}
def boundary(machine,address,size,user):
    sp=machine.reg_read(UC_X86_REG_ESP);words=struct.unpack('<'+'I'*(arities[address]+1),machine.mem_read(sp,4*(arities[address]+1)))
    trace.append((address,list(words[1:])))
    if address==0x563370:machine.mem_write(0x188759c+index*592,u(e['base']+1024))
    machine.reg_write(UC_X86_REG_EAX,returns.get(address,0)&0xffffffff)
    machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,words[0])
for address in arities:m.hook_add(UC_HOOK_CODE,boundary,begin=address,end=address)
def call(index,mode):
    m.mem_write(e['stack'],u(e['stop'],index&0xffffffff,mode));m.reg_write(UC_X86_REG_ESP,e['stack'])
    m.emu_start(0x521d30,e['stop'],count=100000);assert m.reg_read(UC_X86_REG_EIP)==e['stop']
    return m.reg_read(UC_X86_REG_EAX)
cases=0
for enabled,index,mode,fmt,failure in itertools.product((0,1,256,257),(-1,0,2),(0,1,2,256,257),(1,2,3),range(4)):
    bank=bytearray([0xa5])*592*4;expected=bytearray(bank);start=max(index,0)*592;record=0x1887388+start;stream=record+544
    struct.pack_into('<I',bank,start+512,123);expected[:]=bank
    m.mem_write(0x1887388,bytes(bank));m.mem_write(0x1aed354,u(enabled));m.mem_write(e['base']+1024,struct.pack('<H',fmt));trace.clear()
    returns={0x563370:int(failure==1),0x5635d0:int(failure==2),0x521db0:int(failure==3),0x521f90:int(failure==3)}
    want=[];result=0xffffffff
    if enabled&255 and index>=0:
        struct.pack_into('<I',expected,start+532,e['base']+1024)
        want.append((0x563370,[record+256,123,stream,record+532,record+536,record+568]))
        if failure!=1:want.append((0x5635d0,[stream,record+548,record+568]))
        if failure in (1,2):want.append((0x5636e0,[stream]))
        elif mode&255==1:result=0
        else:
            if fmt in (1,2):want.append((0x521db0 if fmt==1 else 0x521f90,[index]));result=0xffffffff if failure==3 else 0
            want.append((0x5636e0,[stream]))
    actual=call(index,mode)
    assert actual==result,(enabled,index,mode,fmt,failure,actual,result)
    assert trace==want,(enabled,index,mode,fmt,failure,trace,want)
    assert bytes(m.mem_read(0x1887388,len(bank)))==expected
    cases+=1
report=dict(result='PASS',cases=cases,original_sha256=e['digest'],scope='Original521d30 and522130 execute; stream open/read/close and format-specific loading supplied. Exact call arguments/order, enabled and negative-index gates, low-byte mode1 stream retention, format1/2 routing, unsupported format and failure cleanup; full cache bytes checked excluding supplied format pointer. No resource parser/device internals or initial-reference-state claim.')
(root/'artifacts/audio-loader-dispatch.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
