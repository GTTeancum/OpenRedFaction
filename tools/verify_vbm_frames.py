"""Validate every installed animated VBM frame against packed source pixels."""
import json,struct,subprocess,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];j=json.loads((root/'artifacts/inventory.json').read_text());probe=root/'build/pc/Release/rf_image_probe.exe'
out=root/'artifacts/vbm-frame-tests';out.mkdir(exist_ok=True);raw=out/'frame.rgba';frames_checked=0;files=[]
for a in j['files']:
 if 'vpp' not in a:continue
 path=root/'Installed_Game'/a['path']
 with path.open('rb') as f:
  for e in a['vpp']['entries']:
   if not e['name'].lower().endswith('.vbm'):continue
   f.seek(e['offset']);data=f.read(e['size'])
   if data[:4]!=b'.vbm':continue
   version,w,h,fmt,rate,count,mips=struct.unpack('<7I',data[4:32])
   if count<=1:continue
   assert mips==0 and len(data)==32+w*h*2*count
   for frame in range(count):
    run=subprocess.run([str(probe),str(path),e['name'],str(raw),str(w*h*4),str(frame)],capture_output=True,text=True)
    assert run.returncode==0,(e['name'],frame,run.stdout)
    rgba=raw.read_bytes();expected=bytearray()
    for (v,) in struct.iter_unpack('<H',data[32+frame*w*h*2:32+(frame+1)*w*h*2]):
     if fmt==0:v^=32768
     if fmt==1:expected.extend(((v>>8&15)*17,(v>>4&15)*17,(v&15)*17,(v>>12)*17))
     else:expected.extend(((v>>(11 if fmt==2 else 10)&31)*255//31,(v>>5&(63 if fmt==2 else 31))*255//(63 if fmt==2 else 31),(v&31)*255//31,255 if fmt==2 or v&32768 else 0))
    assert rgba==expected,(e['name'],frame);frames_checked+=1
   for frame,budget in ((count,w*h*4),(0,w*h*4-1)):
    run=subprocess.run([str(probe),str(path),e['name'],str(raw),str(budget),str(frame)],capture_output=True,text=True);assert run.returncode==1 and run.stdout.strip()=='-4'
   files.append(dict(archive=a['path'],name=e['name'],frames=count,width=w,height=h,rate=rate,sha256=hashlib.sha256(data).hexdigest()))
report=dict(result='PASS',files=len(files),frames=frames_checked,bounds_cases=2*len(files),scope='Every installed animated frame decoded on PC and compared to independent packed-pixel expansion. Original frame layout inspected in 511200; not playback/rendering or native XEMU validation.',assets=files)
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='assets'})
