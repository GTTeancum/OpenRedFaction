"""Original complete live-owner burn body with two spread targets and mutations."""
import itertools,json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ev=runpy.run_path(str(root/'tools/verify_burn_attachment_trace.py'))
g=ev['hook'].__globals__;u=g['u'];b=g['b'];owner=g['owner'];stack=g['stack'];w=g['w'];v=g['v'];emitters=g['emitters']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW
f=lambda value:struct.pack('<f',value)
bits=lambda value:struct.unpack('<I',f(value))[0]
targets=(b+0x7000,b+0x9000);cls=b+0xb000;tramp=b+0xc000
u.mem_write(tramp,b'\xd9\x05'+w(tramp+32)+b'\xc3');u.mem_write(tramp+16,b'\xd9\xee\xc3');u.mem_write(tramp+32,f(6.5))
trace=[];mode=0

def hook(m,address,size,context):
    if address==0x42f0f7:
        point=struct.unpack('<3f',m.mem_read(stack+0x48,12));g['trace'].append(('world',point));return
    if address not in (0x429990,0x427020,0x40a110,0x4290d0,0x504e40,0x4892c0,0x5058c0,0x57312d,0x42f2f0):return
    sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<9I',m.mem_read(sp,36));result=0
    if address in (0x429990,0x427020,0x40a110,0x4290d0):
        g['trace'].append((address,a[1]))
        if address in (0x427020,0x40a110):return # actual flag predicates
    elif address==0x504e40:
        g['trace'].append((address,*a[1:3]));m.reg_write(UC_X86_REG_EIP,tramp);return
    elif address==0x4892c0:
        g['trace'].append((address,*a[1:9]))
        if a[1]==0x1111 and mode==1:m.mem_write(targets[1]+0x810,w(1))
        if a[1]==0x1111 and mode==2:m.mem_write(b+0x2c,w(1))
        m.reg_write(UC_X86_REG_EIP,tramp+16);return
    elif address==0x5058c0:
        g['trace'].append((address,a[1],bytes(m.mem_read(a[2],12)),bytes(m.mem_read(a[3],12)),a[4]))
    elif address==0x57312d:g['trace'].append((address,));result=19
    else:g['trace'].append((address,a[1]))
    m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook)
cases=spread_hits=fade_calls=0
for elapsed,deadline,mode,rotation in itertools.product((0.,12.,12.000001),(-1,1000,1001),(0,1,2),(0,1)):
    g['attachments']=[(-1.,0.,0.),(1.,0.,0.),(3.,4.,5.),(0.,1.,2.)]
    basis=(1.,0.,0.,0.,1.,0.,0.,0.,1.) if rotation==0 else (0.,1.,0.,-1.,0.,0.,0.,0.,1.)
    world=(13.,24.,35.) if rotation==0 else (6.,23.,35.)
    u.mem_write(b,bytes(64));u.mem_write(b,w(*emitters,0x9999,0,1,2,3,0x7777,bits(.75),0,bits(elapsed)))
    u.mem_write(owner+0x20,w(0x4321));u.mem_write(owner+0x2c,w(0x9999));u.mem_write(owner+0x3c,v((10.,20.,30.)));u.mem_write(owner+0x48,struct.pack('<9f',*basis));u.mem_write(owner+0x80,w(0x12345678));u.mem_write(owner+0x144,v((1.,2.,3.)));u.mem_write(owner+0x294,w(cls));u.mem_write(cls+0x44,f(100.));u.mem_write(owner+0x810,w(0));u.mem_write(owner+0x824,w(0x5555));u.mem_write(owner+0x28c,w(targets[0]))
    for j,target in enumerate(targets):
        u.mem_write(target+0x2c,w((j+1)*0x1111));u.mem_write(target+0x3c,v((world[0]+j*2,world[1],world[2])));u.mem_write(target+0x7c,w(0));u.mem_write(target+0x810,w(0,0));u.mem_write(target+0x294,w(cls));u.mem_write(target+0x28c,w(targets[1] if j==0 else 0x5cb060))
    u.mem_write(0x5cb2ec,w(owner));u.mem_write(0x87243c,w(0xabcdef01));u.mem_write(0x62f768,w(deadline));u.mem_write(0x5a3ed8,w(1000));u.mem_write(0x5a4014,f(.125))
    for reg,value in ((UC_X86_REG_ESP,stack),(UC_X86_REG_ESI,b),(UC_X86_REG_EBX,owner),(UC_X86_REG_FPCW,0x27f)):u.reg_write(reg,value)
    g['trace']=[];g['positions']={};u.emu_start(0x42ef3e,0x42f2a2,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x42f2a2
    want=[('attachment',2),('position',3),('update',3)]
    active=elapsed<=12;spread=active and deadline==1000
    if active:want.extend([('attachment',0),('attachment',1),('attachment',3),('position',0),('update',0),('position',1),('update',1),('position',2),('update',2)])
    if spread:
        want.append(('world',world))
        for j,target in enumerate(targets):
            want.extend([(0x429990,target),(0x427020,target)])
            if j==1 and mode==1:continue
            want.extend([(0x40a110,target),(0x4290d0,target),(0x504e40,bits(5),bits(8)),(0x4892c0,(j+1)*0x1111,bits((100./6.5)*.25),0x9999,0xabcdef01,4,0,0x4321,0)])
            spread_hits+=1
    want.append((0x5058c0,0x7777,v((10.,20.,30.)),v((1.,2.,3.)),bits(.75)))
    if spread and mode==2:
        want.append((0x42f2f0,b));fade_calls+=1;expected_elapsed=struct.unpack('<f',f(elapsed+.125))[0];action=0x5555
    else:
        want.extend([(0x427020,owner),(0x504e40,bits(5),bits(8)),(0x4892c0,0x9999,bits(.125*(100./6.5)),0xffffffff,0xffffffff,4,0,0xffffffff,0),(0x57312d,)])
        expected_elapsed=struct.unpack('<f',f(elapsed))[0];action=14
    assert g['trace']==want,(elapsed,deadline,mode,rotation,g['trace'],want)
    assert bytes(u.mem_read(b+0x30,4))==f(expected_elapsed)
    assert bytes(u.mem_read(owner+0x824,4))==w(action)
    assert bytes(u.mem_read(0x62f768,4))==w(deadline) # rearm belongs to outer traversal
    for j,target in enumerate(targets):assert bytes(u.mem_read(target+0x814,4))==w(0x2000 if spread and not(j==1 and mode==1) else 0)
    cases+=1
report=dict(result='PASS',cases=cases,spread_damage_calls=spread_hits,fade_calls=fade_calls,original_sha256=ev['digest'],scope='Complete original live-owner42ef3e..42f2a2; actual vector/world-transform/timer/flag predicates, two spread targets. Damage callback can kill next target or start owner fading before tail. Model/room/emission/audio/random/damage/fade implementations supplied. No pool iteration, fade release or campaign integration.')
(root/'artifacts/burn-body-trace.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
