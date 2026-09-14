"""Summarize native phase timings without counting renderer time twice."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('snapshot',type=Path);p.add_argument('--out',type=Path);a=p.parse_args()
j=json.loads(a.snapshot.read_text());symbols=j['symbols']
def rows(name,labels):
 raw=symbols[name]['words'];assert len(raw)==32
 result=[]
 for i,label in enumerate(labels):
  n,lo,hi,peak=raw[4*i:4*i+4];total=(hi<<32)|lo
  result.append(dict(phase=i,label=label,samples=n,total_ms=total,mean_ms=round(total/n,3) if n else None,max_ms=peak))
 return result
scene=rows('rf_scene_profile',['unused','pre-camera work','camera/visibility/world rebuild','world diagnostics','actor construction/drawing','remaining scene preparation','presentation and state export','physics/event stepping'])
renderer=rows('rf_renderer_profile',['validation','resource preparation','vertex upload','render setup','vblank/reset/clear','world/particles/HUD draw and GPU waits','swap submission','finalization'])
presentation=rows('rf_scene_presentation_profile',['NPCs','clutter','world weapons','pickups','first-person weapon','platform sink','player state export','unused']) if 'rf_scene_presentation_profile' in symbols else []
world=rows('rf_scene_world_profile',['listener/camera effects','audio scheduling','combat/inspection camera','camera setup/room location','visibility traversal','visibility telemetry','world geometry rebuild','unused']) if 'rf_scene_world_profile' in symbols else []
report=dict(scene=scene,renderer=renderer,presentation=presentation,world=world,scope='Guest millisecond phase timing after16 section frames/submissions; scene presentation includes renderer costs, so do not add renderer and scene totals. Input polling/pacing and level loading excluded. Phase means are not an exact FPS measurement; scene final-frame phase counts may differ. XEMU host scheduling affects timings.')
if "rf_renderer_submission" in symbols:
 values=symbols["rf_renderer_submission"]["words"];assert len(values)==4
 report["submission"]=dict(zip(("last_frame_batches","former_methods","submitted_methods","state_changes"),values))
if "rf_renderer_vblank" in symbols:
 values=symbols["rf_renderer_vblank"]["words"];assert len(values)==3
 report["section_vblank"]=dict(zip(("explicit_wait_frames","already_crossed_frames","last_start_counter"),values))
if "rf_scene_step_profile" in symbols:
 report["step"]=rows("rf_scene_step_profile",["early particle emission","forces/movers/player physics/support","light timers","trigger contacts/events","particle simulation/emission/telemetry","NPC scripts/animation/rooms/attachments","collision/alpha checks","unused"])
if "rf_scene_npc_step_profile" in symbols:
 report["npc_step"]=rows("rf_scene_npc_step_profile",["scripted movement","NPC playback/pose/collision cache","NPC room refresh","glare retirement and fixtures","attachments","attachment fixture","glare room refresh","unused"])
if a.out:a.out.write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
