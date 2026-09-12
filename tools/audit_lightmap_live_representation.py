"""Historical raw-RGB baseline versus recovered original packed upload."""
import json,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text())
rows=[]
for name in ['L1S1.rfl','L1S2.rfl','L1S3.rfl']:
 level=next(l for l in levels if l['file']==name);archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==name)
 section=next(s for s in level['sections'] if s['type']=='0x1200')
 with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);raw=f.read(section['size'])
 images=struct.unpack_from('<I',raw)[0];at=4;channels=below_floor=above_floor_quantized=0;max_delta=0;absolute=0
 for image in range(images):
  width,height=struct.unpack_from('<2I',raw,at);at+=8
  rgb=raw[at:at+width*height*3];at+=len(rgb);assert len(rgb)==width*height*3
  for channel in rgb:
   # CPU original1555 face sampling expands by8, not normalized GPU conversion.
   decoded=max(channel>>3,4)*8;delta=abs(decoded-channel)
   channels+=1;below_floor+=channel<32;above_floor_quantized+=channel>=32 and channel%8!=0
   max_delta=max(max_delta,delta);absolute+=delta
 assert at==len(raw)
 rows.append(dict(level=name,images=images,channels=channels,channels_below_original_floor=below_floor,other_quantized_channels=above_floor_quantized,max_cpu_sample_channel_difference=max_delta,mean_cpu_sample_channel_difference=absolute/channels))
report=dict(result='AUDIT',rows=rows,scope='Authored archive RGB compared with original no-brightening1555 pack plus CPU face-sample expansion. Historical rf_lightmaps_open copied raw RGB; the campaign loader now uses original1555 packing. Both current world renderers use two textures and multiply lighting by2, as inspected in preview_fragment.ps.cg and tools/pc_raster.c. These counts quantify representation differences, not GPU pixel errors: normalized5-bit GPU sampling is distinct from CPU channel*8 expansion. Do not replace GPU expansion with CPU expansion or independently double RGB on the current doubled world shader. This preserves the pre-migration baseline; live shared CPU sampling remains open.')
(root/'artifacts/lightmap-live-representation-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
