"""Summarize NXDK compiler frames; this is not a whole-program stack bound."""
import hashlib,json
from pathlib import Path
import pefile

root=Path(__file__).resolve().parents[1]
binary=root/'build/xbox/main.exe'
rows=[]
for report in sorted((root/'src').rglob('*.su')):
    source=report.with_suffix('.c')
    if not source.exists():continue
    if report.stat().st_mtime<source.stat().st_mtime:
        raise RuntimeError(f'Rebuild Xbox: stale stack report {report}')
    for line in report.read_text().splitlines():
        location,size,kind=line.split('\t')
        file,line_number,function=location.rsplit(':',2)
        rows.append(dict(file=file,line=int(line_number),function=function,bytes=int(size),kind=kind))
if not rows:raise RuntimeError('Build Xbox with -fstack-usage first')
sequence=['main','model_preview','rf_animation_stream_placed','animation_run',
          'model_frame','rf_xbox_model_stream_frame','preview']
chain=[]
for name in sequence:
    matching=[r for r in rows if r['function']==name]
    if len(matching)!=1:raise RuntimeError(f'Ambiguous/missing frame {name}')
    chain.append(matching[0])
pe=pefile.PE(str(binary));reserve=pe.OPTIONAL_HEADER.SizeOfStackReserve
scene_sequence=[('main',None),('scene_preview',None),('rf_scene_stream_miner_states',None),
    ('scene_miner',None),('rf_animation_stream_states',None),('animation_run',None),
    ('scene_frame','src/diagnostic/scene.c'),('scene_frame','src/platform/xbox/main.c'),
    ('rf_xbox_scene_stream_frame',None),('preview',None)]
scene_chain=[]
for name,source in scene_sequence:
    matching=[r for r in rows if r['function']==name and (source is None or r['file'].replace('\\','/').endswith(source))]
    if len(matching)!=1:raise RuntimeError(f'Ambiguous/missing scene frame {name} {source}')
    scene_chain.append(matching[0])
result=dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
    stack_reserve=reserve,functions=len(rows),stream_chain=chain,
    stream_frame_subtotal=sum(r['bytes'] for r in chain),
    scene_stream_chain=scene_chain,scene_stream_frame_subtotal=sum(r['bytes'] for r in scene_chain),
    largest=sorted(rows,key=lambda r:r['bytes'],reverse=True)[:20],
    scope='Compiler local/callee-saved frame reports for project C sources; listed chain subtotal excludes call arguments, return addresses, CRT, NXDK, kernel/interrupt usage and other nested branches. Not a runtime high-water measurement or proof of no overflow.')
(root/'artifacts/xbox-stack.json').write_text(json.dumps(result,indent=2))
print({k:v for k,v in result.items() if k not in ('largest','stream_chain','scene_stream_chain')})
for row in chain:print(row['function'],row['bytes'],row['kind'])
