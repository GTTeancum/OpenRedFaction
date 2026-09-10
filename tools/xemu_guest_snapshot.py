"""Read only guest RAM and CPU state over XEMU QMP; never host input/capture."""
import argparse,datetime,hashlib,json,re,struct
from pathlib import Path

def words(monitor,address,count):
    reply=monitor.command('human-monitor-command',{'command-line':f'x /{count}wx 0x{address:x}'})
    result=[]
    for line in reply.splitlines():
        if ':' in line:result.extend(int(v,16) for v in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
    if len(result)!=count:raise RuntimeError(f'Guest read at {address:x}: expected {count} words, received {len(result)}')
    return result

def snapshot(monitor,map_text):
    result={'captured_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'map_sha256':hashlib.sha256(map_text.encode()).hexdigest(),
        'consistency':'Non-atomic live reads; use stable completed telemetry for comparisons.',
        'memory':monitor.command('query-memory-size-summary'),
        'status':monitor.command('query-status'),'symbols':{}}
    for name,count in [('rf_scene_visibility_summary',6),('rf_scene_visibility_frames',1088)]:
        match=re.search(r'_'+name+r'\s+([0-9a-fA-F]+)',map_text)
        if match:
            address=int(match[1],16)
            result['symbols'][name]={'address':hex(address),'words':words(monitor,address,count)}
    for name,count in [('rf_player_replay_diagnostic',4),('rf_preview_failure',8),('rf_animation_progress',4),('rf_renderer_profile',32),('rf_scene_profile',32),('rf_scene_profile_stage',2),('rf_player_frame_clock',8),('rf_diagnostic',58),('rf_actor_creation_diagnostic',6),('rf_scene_actor_physics_diagnostic',8),('rf_actor_world_diagnostic',8),('rf_actor_fall_diagnostic',8),('rf_scene_actor_fall_state',77),('rf_scene_actor_initial_state',77),('rf_scene_actor_tick_stats',8),('rf_scene_actor_contact',7),('rf_scene_actor_contact_time',4),('rf_scene_actor_pose',59),('rf_scene_actor_render_frames',320),('resident_miner_config',117),('scene_actor_body',81)]:
        match=re.search(r'_'+name+r'\s+([0-9a-fA-F]+)',map_text)
        if not match:continue
        address=int(match[1],16);data=words(monitor,address,count)
        result['symbols'][name]={'address':hex(address),'words':data}
        if name=='scene_actor_body' and data[78] and data[78]<=8 and data[77]:
            result['spheres']=words(monitor,data[77],data[78]*6)
        if name=='resident_miner_config':
            raw=struct.pack('<117I',*data)
            result['authored']={'mass':struct.unpack_from('<f',raw)[0],'material':raw[4:68].split(bytes(1))[0].decode('ascii','replace'),
                'flags':hex(data[17]),'flags2':hex(data[18]),'movement_index':data[19],'use_kind':data[20],
                'material_index':data[22],'sphere_declarations':data[28]}
    for name,count in [('rf_player_input_diagnostic',6),('rf_scene_player_input_frames',448),('rf_scene_actor_turn_enabled',1),('rf_scene_actor_look_enabled',1),('rf_scene_actor_look_frames',2112),('rf_scene_actor_eye_enabled',1),('rf_scene_actor_eye_frames',2944),('rf_scene_actor_initial_eye_tag',1),('rf_scene_actor_initial_eye_offsets',6),('rf_scene_actor_room_frames',576),('rf_scene_actor_room_summary',8),('rf_scene_actor_room_state',5),('rf_scene_actor_ground_stats',8),('rf_scene_actor_ground_records',2112),('rf_scene_actor_landing',8),('rf_scene_actor_movement',16),('rf_scene_actor_movement_values',4),('rf_scene_actor_ground_modes',64),('rf_scene_actor_run_traction',1),('rf_scene_actor_surface_values',60),('rf_scene_actor_surface_frames',128),('rf_scene_actor_locomotion_frames',768),('rf_scene_actor_animation_timing',192),('rf_scene_actor_live_enabled',1),('rf_scene_actor_frame_count',1),('rf_scene_actor_ring_frames',64),('rf_scene_actor_live_summary',8),('rf_scene_actor_follow_frames',896),('rf_scene_actor_follow_summary',5),('rf_scene_actor_route_enabled',1),('rf_scene_actor_routes',128),('rf_scene_actor_stance_support',576),('rf_scene_actor_stance_ground',2112),('rf_scene_actor_initial_animation',12),('rf_scene_actor_selector_frames',512),('rf_scene_actor_clearance_diagnostic',8),('rf_scene_actor_clearance_queries',24),('rf_scene_actor_movement_config',7),('rf_scene_actor_movement_frames',192),('rf_scene_actor_stance_cache',50),('rf_scene_actor_stance_frames',256),('rf_scene_actor_drive_enabled',1),('rf_scene_actor_contact_count',1),('rf_scene_actor_contacts',1600),('rf_scene_actor_input_frames',192)]:
        match=re.search(r'_'+name+r'\s+([0-9a-fA-F]+)',map_text)
        if match:
            address=int(match[1],16)
            result['symbols'][name]={'address':hex(address),'words':words(monitor,address,count)}
    sweep=result['symbols'].get('rf_actor_world_diagnostic',{}).get('words',[])
    match=re.search(r'_rf_scene_actor_sweep_records\s+([0-9a-fA-F]+)',map_text)
    if match and len(sweep)==8 and sweep[:2]==[0x52464157,1] and 0<sweep[2]<=48 and sweep[6]==80:
        records=words(monitor,int(match[1],16),sweep[2]*20)
        result['actor_sweep_words']=records
        result['actor_sweep_hits']=[]
        for i in range(sweep[2]):
            raw=struct.pack('<20I',*records[i*20:(i+1)*20])
            if records[i*20+8]:
                result['actor_sweep_hits'].append(dict(sphere=i//6,axis=i%6//2,direction=-1 if i%2 else 1,
                    start=struct.unpack_from('<3f',raw),radius=struct.unpack_from('<f',raw,24)[0],
                    fraction=struct.unpack_from('<f',raw,36)[0],point=struct.unpack_from('<3f',raw,40),
                    normal=struct.unpack_from('<3f',raw,52),face=records[i*20+16],room=records[i*20+17]))
    for name,command in [('registers','info registers'),('instructions','x /12i $eip'),('stack','x /24wx $esp')]:
        result[name]=monitor.command('human-monitor-command',{'command-line':command})
    return result

def main():
    from xemu_smoke import Monitor
    p=argparse.ArgumentParser();p.add_argument('--port',type=int,required=True);p.add_argument('--map',type=Path,required=True);p.add_argument('--out',type=Path,required=True);args=p.parse_args()
    monitor=Monitor(args.port)
    try:result=snapshot(monitor,args.map.read_text())
    finally:monitor.stream.close();monitor.sock.close()
    args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(result,indent=2));print(args.out)
if __name__=='__main__':main()
