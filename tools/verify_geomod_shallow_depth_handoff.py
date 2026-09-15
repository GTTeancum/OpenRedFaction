"""Original45cff0 signed-depth selection and4dbfb3..4dc03f limit handoff, no math stubs."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
rows=[]
for depths in [(0,),(-2,),(2,),(0,3),(2,0),(-2,3),(2,-3),(0,-3)]:
 regions=[(0,1,0),(1,0,0)][:len(depths)];u.mem_write(B,bytes(0x10000));u.mem_write(B+0x103c,f(5))
 for i,(n,d) in enumerate(zip(regions,depths)):
  a=B+0x3000+i*0x50;u.mem_write(B+0x2000+i*4,w(a));u.mem_write(a,w(2,25)+bytes([1,0,0,0])+f(d,0,0,0,1,0,0,*n,0,0,1,100,100,100,100))
 u.mem_write(S,w(stop,B,B+0x1000,1));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x45cff0,stop,count=100000);assert u.reg_read(UC_X86_REG_EAX)&255==1
 limits=struct.unpack('<6f',u.mem_read(B+0x104c,24));u.mem_write(S,bytes(0x200));u.mem_write(S+0x104,w(B+0x104c,B+0x1058));u.mem_write(0xc968b8,w(B+0x9000));u.mem_write(B+0x9060,f(1));u.mem_write(0xc9f55c,f(5));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4dbfb3,0x4dc03f,count=10000)
 values=struct.unpack('<8f',u.mem_read(S+0x30,32));enabled=u.mem_read(S+0x13,1)[0]
 assert enabled==int(depths[0]!=0)
 assert abs(values[0])==(abs(depths[0]) if enabled else 0)
 assert abs(values[1])==(abs(depths[1]) if enabled and len(depths)>1 else 0)
 rows.append(dict(depths=depths,selected_limits=limits,enabled=enabled,depths_and_normal2_normal1=values))
(R/'artifacts/crater-shading-re/shallow-depth-handoff.json').write_text(json.dumps(rows,indent=2));print('PASS:',len(rows),'original signed-depth selection and normalization cases')

import subprocess
for row in rows:
 depths=row['depths'];values=[len(depths)]
 for n,d in zip([(0,-1,0),(-1,0,0)],depths):values.extend([*n,d])
 actual=list(map(float,subprocess.check_output([str(R/'build/pc/Release/rf_geomod_basis_probe.exe'),'--shallow-normalize'],input=' '.join(map(str,values)),text=True).split()))
 original=row['depths_and_normal2_normal1'];expected=[original[0],*original[5:8],original[1],*original[2:5]]
 assert actual[1:]==expected,(row,actual,expected)
 assert int(actual[0])==sum(d!=0 for d in depths) if row['enabled'] else int(actual[0])==0
print('PASS:8 shared C/original signed-depth handoff comparisons')
