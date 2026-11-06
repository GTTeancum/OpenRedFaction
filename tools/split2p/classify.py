"""Classify scene globals into per-player state for the split-screen swap table."""
import os
HERE = os.path.dirname(os.path.abspath(__file__))
import re, sys
sys.path.insert(0, HERE)
from globals import out
INCLUDE = [r'^rf_scene_actor_', r'^actor_', r'^campaign_player_', r'^rf_scene_player_', r'^scene_player_', r'^player_(poll|context|frame_limit|input)$',
    r'^life_', r'^campaign_(climb|crouched|jump_held|jump_strength|camera_effect|teleport_\w+|inventory_ready|pickup_notice_frame|startup_inventory\w*|undercover_\w+_sidecar)$',
    r'^pistol_reload_ticks$', r'^weapon_cycle_held$', r'^scene_scope\b', r'^scene_scope_look$', r'^scene_scope$', r'^rf_scene_scope_projection$',
    r'^scene_use_(basis|eye|pose_frame)$', r'^rf_scene_use_', r'^scene_walk_', r'^scene_flame_(input|active|cadence|stream_frame|stream_type)$',
    r'^scene_fusion_input$', r'^scene_remote_(charges|cooldown|delay|followup_target|followup_ticks|held|pending|host_tags|capture_state)$',
    r'^scene_rail_scope$', r'^rf_scene_rail_scope$', r'^scene_scanner_(enabled|held|result)$', r'^rf_scene_scanner$', r'^rf_scene_gameplay_eye$',
    r'^rf_scene_hud_anchor$', r'^scene_inspection_eye$', r'^scene_mp_player_(class_flags|dead|streak)$', r'^rf_scene_mp_player_(stats|name|class|skin|model)$',
    r'^rf_scene_weapon_(selection|aim|hands|draw|muzzle)$', r'^scene_ground_piece_support$',
    # found by tools/split2p/used_by.py over the player's routines (names outside the rules above)
    r'^(scene_actor_body|campaign_support_handle|campaign_support_velocity|campaign_equipped_slot|campaign_explicit_unarmed|campaign_last_alt|campaign_machine_mode|combat_trigger|combat_hit_frame|pistol_fire_ticks|dev_refill_held|campaign_blackout_deadline|scene_fire_basis|scene_continuous_charge|scene_continuous_charge_runtime|scene_player_riot_owner|scene_player_riot_alternate_owner|scene_grenade_throw|scene_grenade_empty_selection|scene_flame_canister_cooldown|scene_flame_canister_held|scene_flame_canister_pending|scene_weapon_camera_profiles|rf_scene_combat|rf_scene_rocket_lock|rf_scene_continuous_charge|rf_scene_remote_hand|rf_scene_machine_mode)$',
    r'^campaign_actor_controller$', r'^rf_scene_item_pickup_(gate|state)$', r'^rf_scene_pickup_(notice|vitals|room_cache)$']
EXCLUDE = [r'\.', r'^rf_scene_actor_(initial_\w+|route_enabled|live_enabled|eye_enabled|look_enabled|turn_enabled|drive_enabled|pair_test\w*|model_test|frame_count|surface_values|movement_config)$',
    r'^campaign_player_(feedback_sink|feedback_context|model)$', r'^rf_scene_player_\w*(test|sample|values|resources|checkpoint_enabled|save_fixture|save_audit)$',
    r'^actor_trace_contacts$', r'^actor_follow_world$', r'^player_(poll|context|frame_limit)$', r'^scene_player_(body_geometry|body_geometry_ready|riot_definition|riot_alternate_definition|kill_test_frame)$']
inc = [o for o in out if any(re.search(p, o[0]) for p in INCLUDE) and not any(re.search(p, o[0]) for p in EXCLUDE)]
locals_ = sorted({o[0] for o in out if '.' in o[0] and re.search(r'player|scope|use_edge|weapon_loop|scanner|crash_aim_input', o[0])})
if __name__ == '__main__':
    print(len(inc), 'per-player values,', sum(o[1] for o in inc), 'bytes')
    print('function-local statics left out:', ', '.join(locals_))
    big = sorted(inc, key=lambda o: -o[1])[:12]
    print('largest:', ', '.join('%s %d' % (n, s) for n, s, t in big))
