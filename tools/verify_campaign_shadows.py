"""Real Live Mines shadow integration; not an original or native XEMU oracle."""
import csv,io,json,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
command=[str(root/'build/pc/Release/rf_campaign_shadow_probe.exe'),str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl','0','5851']+[str(root/'Installed_Game'/('maps'+name+'.vpp')) for name in ('1','2','3','4','_en')]
start=time.perf_counter()
run=subprocess.run(command,capture_output=True,text=True,check=True)
seconds=time.perf_counter()-start
assert not run.stderr,run.stderr
rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(io.StringIO(run.stdout))]
assert len(rows)==5851 and [r['mapping'] for r in rows]==list(range(5851))
assert all(r['sources']<64 and r['callbacks']<=r['sources'] and r['backfacing']<=r['callbacks'] and r['accepted']<=r['eligible']<=r['visited'] for r in rows)
assert any(r['accepted'] and r['changed_bytes'] for r in rows)
assert any(r['rgb_changed_bytes'] and r['unmasked_packed_hash']!=r['masked_packed_hash'] for r in rows)
# Reopening ownership must reproduce the first64 complete jobs exactly.
short=command.copy();short[4]='64'
repeat=subprocess.run(short,capture_output=True,text=True,check=True)
assert not repeat.stderr and repeat.stdout.splitlines()==run.stdout.splitlines()[:65]
cached_command=command.copy();cached_command.insert(1,'--retained')
start=time.perf_counter();cached=subprocess.run(cached_command,capture_output=True,text=True,check=True);cached_seconds=time.perf_counter()-start
assert not cached.stderr and cached.stdout==run.stdout
report=dict(result='PASS',level='L1S1.rfl',mappings=len(rows),mappings_with_shadows=sum(r['callbacks']>0 for r in rows),
    ordinary_lit_mappings=sum(r['sources']>0 and not r['special'] for r in rows),special_lit_mappings=sum(r['sources']>0 and bool(r['special']) for r in rows),
    mappings_with_rgb_change=sum(r['rgb_changed_bytes']>0 for r in rows),mappings_with_packed_change=sum(r['unmasked_packed_hash']!=r['masked_packed_hash'] for r in rows),
    totals={k:sum(r[k] for r in rows) for k in ('sources','callbacks','passes','backfacing','visited','eligible','accepted','changed_bytes','rgb_changed_bytes')},
    maximum_sources=max(r['sources'] for r in rows),peak_pc_shadow_scratch_bytes=max(r['scratch_bytes'] for r in rows),
    seconds=seconds,cached_seconds=cached_seconds,cached_identical_jobs=len(rows),cached_face_bytes=7418*56,reopened_identical_jobs=64,
    scope='PC real world geometry, authored light selection and shadow modes, loaded texture formats, receiver grouping, bounded masks and actual projected callbacks. All initial world faces in file order, mapping-box selection, dirty2 and renderer mode1; caller routing supplied. Lit mappings also execute ordinary/special accumulation, RGB resolve and linear1555 packing, comparing masked/unmasked contributions with explicitly supplied zero ambient and directional scale1. No original full-scene comparison, campaign ambient binding, movers, removed faces, live room routing, GPU upload/swizzling, native Xbox memory or rendered parity evidence. Scratch budget excludes other owners and allocator overhead.')
(root/'artifacts/campaign-shadow-l1s1.csv').write_text(run.stdout)
(root/'artifacts/campaign-shadow-l1s1.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
