/* RAM log ABI/writer adapted from OpenJKDF2ogx xbox_debug.c.
 * OpenJKDF2 contributors; permission and disclaimer retained in
 * tools/vendor/openjkdf2/LICENSE.md. Reader is vendored unchanged.
 * Opt-in diagnostic logging only; no input or gameplay state writes. */
#define XDBG_BUF_SIZE 512
#define XDBG_MIRROR_SIZE 65536
#define XDBG_MIRROR_MAGIC0 0x4A4B4446u /* JKDF */
#define XDBG_MIRROR_MAGIC1 0x52414D4Cu /* RAML */

volatile unsigned int g_XboxDebugMirrorMagic0 = XDBG_MIRROR_MAGIC0;
volatile unsigned int g_XboxBootPhase = 0;
volatile unsigned int g_XboxLogWriteCount = 0;
volatile unsigned int g_XboxHeartbeatCount = 0;
volatile unsigned int g_XboxLastLogTick = 0;
volatile unsigned int g_XboxLogMirrorWriteOffset = 0;
volatile unsigned int g_XboxLogMirrorWrapped = 0;
volatile unsigned int g_XboxDebugMirrorMagic1 = XDBG_MIRROR_MAGIC1;
volatile char g_XboxLogMirror[XDBG_MIRROR_SIZE];




static void xbox_debug_MirrorWrite(const char *msg)
{
    unsigned int offset;
    unsigned int i;

    if (!msg)
        return;

    offset = g_XboxLogMirrorWriteOffset;
    for (i = 0; msg[i]; i++)
    {
        g_XboxLogMirror[offset] = msg[i];
        offset++;
        if (offset >= XDBG_MIRROR_SIZE)
        {
            offset = 0;
            g_XboxLogMirrorWrapped = 1;
        }
    }

    g_XboxLogMirror[offset] = 0;
    g_XboxLogMirrorWriteOffset = offset;
    g_XboxLogWriteCount++;
    g_XboxHeartbeatCount++;
    g_XboxLastLogTick = GetTickCount();
    g_XboxDebugMirrorMagic0 = XDBG_MIRROR_MAGIC0;
    g_XboxDebugMirrorMagic1 = XDBG_MIRROR_MAGIC1;
}

extern uint32_t rf_scene_combat_population[4],rf_scene_crash_activity[4],rf_scene_player_ammo[8];
extern uint32_t rf_scene_crash_scenario_trace[8],rf_scene_enemy_failure[6],rf_scene_crash_terrain[8];
extern uint32_t rf_scene_crash_terrain_owner[8];
extern uint32_t rf_scene_crash_terrain_admission[10];
extern uint32_t rf_scene_drone_player_contact[10];
extern uint32_t rf_scene_drone_separation[20];
extern uint32_t rf_scene_drone_smash[12],rf_scene_drone_secondary[12];
extern uint32_t rf_scene_grenade_contact_trace[20];
extern uint32_t rf_scene_ai_fusion[6],rf_scene_fusion_projectiles[5];
extern uint32_t rf_scene_ai_flame[8],rf_scene_ai_grenades[5];
extern uint32_t rf_scene_combat_event_count,rf_scene_combat_events[32][5];
extern uint32_t rf_scene_enemy_damage_kinds[10],rf_scene_crash_aim_trace[4];
extern uint32_t rf_scene_crash_npc_motion[12],rf_scene_crash_npc_progress[12];
extern uint32_t rf_scene_npc_playback[7],rf_scene_npc_eyes[4];
extern uint32_t rf_scene_crash_visibility[4],rf_scene_player_failure[4],rf_scene_pain_bank_pressure[5];
extern uint32_t rf_scene_combat_pain[8],rf_scene_npc_pain_audio[9],rf_scene_live_audio[8];
extern uint32_t rf_scene_pain_voice_pressure[4];
extern uint32_t rf_scene_turret_combat[10],rf_scene_turret_owners[8];
extern uint32_t rf_scene_crash_flash[6],rf_scene_noise_live_trace[20],rf_scene_noise_nav_delay[4];
extern uint32_t rf_scene_contact_admission[12];
extern uint32_t rf_scene_drone_owner_admission[16];
extern uint32_t rf_scene_campaign_load_stage,rf_scene_campaign_load_detail;
extern uint32_t rf_scene_npc_body_admission[4],rf_scene_npc_body_failure[2],rf_scene_patrol_failure[3];
extern uint32_t rf_scene_clutter_load_trace[8],rf_scene_clutter_materials[8],rf_material_failure[3];
extern char rf_material_failure_name[61];
extern uint32_t rf_scene_profile_stage[2],rf_preview_failure[8];
extern uint32_t rf_scene_presentation_failure[8],rf_xbox_renderer_stage[4],rf_xbox_draw_audit[4],rf_xbox_texture_budget[12];
extern uint32_t rf_scene_late_load[8];
extern float rf_scene_gameplay_eye[3];
extern uint32_t rf_scene_vehicle_enabled,rf_scene_vehicle_state[16],rf_scene_vehicle_damage[8];
extern uint32_t rf_scene_jeep_seats[8],rf_scene_apc_primary[8],rf_scene_fighter_weapon[8];
extern uint32_t rf_scene_submarine_weapon[8],rf_scene_drill_state[10],rf_scene_drill_reject[8],rf_scene_drill_trace[32],rf_scene_drill_tunnel[4],rf_geomod_prepare_failure[3];
extern uint32_t rf_scene_vehicle_entry_probe[16];
extern uint32_t rf_scene_vehicle_exit_probe[80];
extern uint32_t rf_scene_use_selection[10];
/* Sampled each log line: available, committed VM, pool and stack pages. */
uint32_t rf_xbox_live_memory[4];
static void rf_ram_log_sample(uint32_t force)
{
    static int initialized,enabled;
    static uint32_t previous=UINT32_MAX;
    static char text[16384];static uint32_t rf_xbox_run_nonce[1];
    size_t used=0;int count;
    if(!initialized){FILE *f=tester_profile?NULL:fopen("D:\\crash-ram-log.flag","rb");
        initialized=1;enabled=f!=NULL;if(f)fclose(f);
        /* Harness run identity: lines left in memory by an earlier run carry
         * a different value and are ignored by the reader. */
        if(enabled && (f=fopen("D:\\crash-run-nonce.bin","rb"))!=NULL){
            if(fread(rf_xbox_run_nonce,4,1,f)!=1)rf_xbox_run_nonce[0]=0;fclose(f);}}
    if(!enabled)return;
    /* Replay position paces the log; a live-controller session has none, so it
     * follows game frames instead (else the host sees no writes and calls it stalled). */
    uint32_t frame=player_replay?rf_player_replay_diagnostic[2]:campaign_total_frames;
    if(!force && (frame==previous || frame%60))return;
    previous=frame;g_XboxBootPhase=rf_diagnostic[2];
    {MM_STATISTICS m={0};m.Length=sizeof(m);if(NT_SUCCESS(MmQueryStatistics(&m))){rf_xbox_live_memory[0]=m.AvailablePages;
        rf_xbox_live_memory[1]=m.VirtualMemoryBytesCommitted>>12;rf_xbox_live_memory[2]=m.PoolPagesCommitted;rf_xbox_live_memory[3]=m.StackPagesCommitted;}}
    used=(size_t)snprintf(text,sizeof(text),"RFLOG {");
#define RAM_ARRAY(name,n) do {     count=snprintf(text+used,sizeof(text)-used,"\"" #name "\":[");     if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count;     for(uint32_t i=0;i<(n);i++){uint32_t word;memcpy(&word,(const void*)((const unsigned char*)(name)+i*4),4);         count=snprintf(text+used,sizeof(text)-used,"%s%u",i?",":"",word);         if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count;}     count=snprintf(text+used,sizeof(text)-used,"],");     if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count; } while(0)
    RAM_ARRAY(rf_xbox_run_nonce,1);
    RAM_ARRAY(rf_diagnostic,58);
    RAM_ARRAY(&rf_xbox_load_stage,1);
    RAM_ARRAY(rf_xbox_startup_trace,4);
    RAM_ARRAY(rf_vpp_io_stats,16);
    RAM_ARRAY(rf_vpp_io_stages,48);
    {extern uint32_t rf_scene_load_stage_ms[96];RAM_ARRAY(rf_scene_load_stage_ms,96);}
    RAM_ARRAY(&rf_scene_campaign_load_stage,1);
    RAM_ARRAY(&rf_scene_campaign_load_detail,1);
    RAM_ARRAY(rf_scene_npc_body_admission,4);
    RAM_ARRAY(rf_scene_npc_body_failure,2);
    RAM_ARRAY(rf_scene_patrol_failure,3);
    RAM_ARRAY(rf_scene_clutter_load_trace,8);
    RAM_ARRAY(rf_scene_clutter_materials,8);
    RAM_ARRAY(rf_material_failure,3);
    RAM_ARRAY(rf_material_failure_name,15);
    RAM_ARRAY(rf_player_replay_diagnostic,4);
    RAM_ARRAY(rf_scene_player_life,8);
    RAM_ARRAY(rf_scene_combat,8);
    RAM_ARRAY(rf_scene_weapon_selection,8);
    RAM_ARRAY(rf_scene_player_ammo,8);
    RAM_ARRAY(rf_scene_enemy_combat,8);
    RAM_ARRAY(rf_scene_turret_combat,10);
    RAM_ARRAY(rf_scene_turret_owners,8);
    RAM_ARRAY(&rf_scene_combat_event_count,1);
    RAM_ARRAY(rf_scene_combat_events,160);
    RAM_ARRAY(rf_scene_enemy_damage_kinds,10);
    RAM_ARRAY(rf_scene_crash_aim_trace,4);
    RAM_ARRAY(rf_scene_crash_npc_motion,12);
    RAM_ARRAY(rf_scene_crash_npc_progress,12);
    RAM_ARRAY(rf_scene_npc_playback,7);
    RAM_ARRAY(rf_scene_npc_eyes,4);
    RAM_ARRAY(rf_scene_crash_visibility,4);
    RAM_ARRAY(rf_scene_combat_pain,8);
    RAM_ARRAY(rf_scene_npc_pain_audio,9);
    RAM_ARRAY(rf_scene_live_audio,8);
    RAM_ARRAY(rf_scene_pain_voice_pressure,4);
    RAM_ARRAY(rf_scene_combat_population,4);
    RAM_ARRAY(rf_scene_crash_activity,4);
    RAM_ARRAY(rf_scene_rockets,8);
    RAM_ARRAY(rf_scene_grenades,8);
    RAM_ARRAY(rf_scene_ai_fusion,6);
    RAM_ARRAY(rf_scene_ai_flame,8);
    RAM_ARRAY(rf_scene_ai_grenades,5);
    RAM_ARRAY(rf_scene_fusion_projectiles,5);
    RAM_ARRAY(rf_scene_grenade_contact_trace,20);
    RAM_ARRAY(rf_scene_rocket_blast,8);
    RAM_ARRAY(rf_scene_geomod,8);
    RAM_ARRAY(rf_scene_debris,8);
    RAM_ARRAY(rf_scene_crash_scenario_trace,8);
    RAM_ARRAY(rf_scene_enemy_failure,6);
    RAM_ARRAY(rf_scene_player_failure,4);
    RAM_ARRAY(rf_scene_pain_bank_pressure,5);
    RAM_ARRAY(rf_scene_profile_stage,2);
    RAM_ARRAY(rf_scene_presentation_failure,8);
    RAM_ARRAY(rf_scene_late_load,8);
    RAM_ARRAY(rf_xbox_renderer_stage,4);
    RAM_ARRAY(rf_xbox_draw_audit,4);
    RAM_ARRAY(rf_xbox_texture_budget,12);
    RAM_ARRAY(rf_preview_failure,8);
    RAM_ARRAY(rf_scene_crash_terrain,8);
    RAM_ARRAY(rf_scene_crash_terrain_owner,8);
    RAM_ARRAY(rf_scene_crash_terrain_admission,10);
    {extern uint32_t rf_scene_geomod_shrunk[3];RAM_ARRAY(rf_scene_geomod_shrunk,3);}
    RAM_ARRAY(rf_scene_drone_player_contact,10);
    {extern uint32_t rf_scene_drone_player_response[4];RAM_ARRAY(rf_scene_drone_player_response,4);}
    RAM_ARRAY(rf_scene_drone_separation,20);
    RAM_ARRAY(rf_scene_drone_smash,12);
    RAM_ARRAY(rf_scene_drone_secondary,12);
    RAM_ARRAY(rf_scene_crash_flash,6);
    RAM_ARRAY(rf_scene_noise_live_trace,20);
    RAM_ARRAY(rf_scene_noise_nav_delay,4);
    RAM_ARRAY(rf_scene_contact_admission,12);
    RAM_ARRAY(rf_scene_drone_owner_admission,16);
    RAM_ARRAY(rf_scene_gameplay_eye,3);
    RAM_ARRAY(&rf_scene_vehicle_enabled,1);
    RAM_ARRAY(rf_scene_vehicle_state,16);
    RAM_ARRAY(rf_scene_vehicle_damage,8);
    RAM_ARRAY(rf_scene_jeep_seats,8);
    RAM_ARRAY(rf_scene_apc_primary,8);
    RAM_ARRAY(rf_scene_fighter_weapon,8);
    RAM_ARRAY(rf_scene_submarine_weapon,8);
    RAM_ARRAY(rf_scene_drill_state,10);
    RAM_ARRAY(rf_scene_drill_reject,8);
    RAM_ARRAY(rf_scene_drill_trace,32);
    RAM_ARRAY(rf_scene_drill_tunnel,4);
    {extern uint32_t rf_scene_rocket_trail[8];RAM_ARRAY(rf_scene_rocket_trail,8);}
    {extern uint32_t rf_scene_alarm_sight_fail[3];RAM_ARRAY(rf_scene_alarm_sight_fail,3);}
    {extern uint32_t rf_scene_drill_refusal[8];RAM_ARRAY(rf_scene_drill_refusal,8);}
    {extern uint32_t rf_scene_terrain_draw[5],rf_scene_authored_source_cuts[8];RAM_ARRAY(rf_scene_terrain_draw,5);RAM_ARRAY(rf_scene_authored_source_cuts,8);}
    RAM_ARRAY(rf_geomod_prepare_failure,3);
    RAM_ARRAY(rf_scene_vehicle_entry_probe,16);
    RAM_ARRAY(rf_scene_vehicle_exit_probe,80);
    RAM_ARRAY(rf_scene_use_selection,10);
    /* Guest ms, last frame-start VBlank, simulation ticks, presented frames,
     * frame starts that found their VBlank already crossed; rates come from
     * deltas between samples. */
    {extern uint32_t rf_renderer_vblank[3];
     uint32_t rf_xbox_frame_rate[5]={GetTickCount(),rf_renderer_vblank[2],rf_player_frame_clock.steps,
         rf_player_frame_clock.presentations,rf_renderer_vblank[1]};
     RAM_ARRAY(rf_xbox_frame_rate,5);}
    {extern uint32_t rf_xbox_scanout_hazard[3];RAM_ARRAY(rf_xbox_scanout_hazard,3);}
    {extern uint32_t rf_scene_rocket_lock_sight_failure[8];RAM_ARRAY(rf_scene_rocket_lock_sight_failure,8);}
    {extern uint32_t rf_scene_npc_walk_in_place[8];RAM_ARRAY(rf_scene_npc_walk_in_place,8);}
    {extern uint32_t rf_scene_world_build_skips;RAM_ARRAY(&rf_scene_world_build_skips,1);}
    {extern uint32_t rf_scene_draw_skips;RAM_ARRAY(&rf_scene_draw_skips,1);}
    {extern uint32_t rf_scene_world_load_reject[3];RAM_ARRAY(rf_scene_world_load_reject,3);}
    {extern uint32_t rf_scene_world_player_probe[2];RAM_ARRAY(rf_scene_world_player_probe,3);}
    {extern uint32_t rf_scene_remote_hash_trace[7];RAM_ARRAY(rf_scene_remote_hash_trace,7);}
    {extern uint32_t rf_scene_tactical_failure[3];RAM_ARRAY(rf_scene_tactical_failure,3);}
    {extern uint32_t rf_scene_world_load_fallback[3];RAM_ARRAY(rf_scene_world_load_fallback,3);}
    RAM_ARRAY(rf_xbox_live_memory,4);
    {extern uint32_t rf_renderer_profile[8][4],rf_renderer_draw_profile[8][4];RAM_ARRAY(rf_renderer_profile,32);RAM_ARRAY(rf_renderer_draw_profile,32);}
    {extern uint32_t rf_xbox_particle_batch[4],rf_xbox_particle_texture_switches;RAM_ARRAY(rf_xbox_particle_batch,4);RAM_ARRAY(&rf_xbox_particle_texture_switches,1);}
    {extern uint32_t rf_renderer_fx_split[3];RAM_ARRAY(rf_renderer_fx_split,3);}
    {extern uint64_t rf_scene_corona_cycles[10];RAM_ARRAY(rf_scene_corona_cycles,20);}
    {extern uint32_t rf_scene_npc_pose_lod[2];RAM_ARRAY(rf_scene_npc_pose_lod,2);}
    {extern uint32_t rf_xbox_liquid_faces[4];RAM_ARRAY(rf_xbox_liquid_faces,4);}
    {extern uint32_t rf_xbox_liquid_waves[4];RAM_ARRAY(rf_xbox_liquid_waves,4);}
    {extern uint32_t rf_xbox_pacing[8];RAM_ARRAY(rf_xbox_pacing,8);}
    {extern uint32_t rf_xbox_pacing_sleep[4];RAM_ARRAY(rf_xbox_pacing_sleep,4);}
    {extern uint32_t rf_xbox_audio_time[4];RAM_ARRAY(rf_xbox_audio_time,4);}
    {extern uint32_t rf_scene_clutter_draw[7];RAM_ARRAY(rf_scene_clutter_draw,7);}
    {extern uint32_t rf_material_dds_stats[4],rf_xbox_texture_pack[2];RAM_ARRAY(rf_material_dds_stats,4);RAM_ARRAY(rf_xbox_texture_pack,2);}
    {extern uint32_t rf_scene_world_texture_budget[4],rf_scene_npc_texture_cap,rf_xbox_video[4];
     RAM_ARRAY(rf_scene_world_texture_budget,4);RAM_ARRAY(&rf_scene_npc_texture_cap,1);RAM_ARRAY(rf_xbox_video,4);}
    {extern uint32_t rf_xbox_screen[4];RAM_ARRAY(rf_xbox_screen,4);}
    {extern uint32_t rf_scene_weapon_ir[8];RAM_ARRAY(rf_scene_weapon_ir,8);}
    {extern int32_t rf_scene_weapon_ir_bounds[8];RAM_ARRAY(rf_scene_weapon_ir_bounds,8);}
    {extern uint32_t rf_scene_weapon_digits[4];RAM_ARRAY(rf_scene_weapon_digits,4);}
    {extern uint32_t rf_xbox_stream_peak[3];RAM_ARRAY(rf_xbox_stream_peak,3);}
    {extern uint32_t rf_scene_player_rounds[4];RAM_ARRAY(rf_scene_player_rounds,4);}
    {extern uint32_t rf_scene_precision_rounds[4];RAM_ARRAY(rf_scene_precision_rounds,4);}
    {extern uint32_t rf_scene_tracers[8];RAM_ARRAY(rf_scene_tracers,8);}
    {extern uint32_t rf_scene_player_tracer_stats[4];RAM_ARRAY(rf_scene_player_tracer_stats,4);}
    {extern uint32_t rf_scene_rail_trail[4];RAM_ARRAY(rf_scene_rail_trail,4);}
    {extern uint32_t rf_scene_beam_draws[4];RAM_ARRAY(rf_scene_beam_draws,4);}
    {extern uint32_t rf_scene_remote_hand[4];RAM_ARRAY(rf_scene_remote_hand,4);}
    {extern uint32_t rf_scene_player_muzzle[4];RAM_ARRAY(rf_scene_player_muzzle,4);}
    {extern float rf_scene_player_muzzle_point[8];RAM_ARRAY(rf_scene_player_muzzle_point,8);}
    {extern uint32_t rf_scene_npc_round_kinds[4];RAM_ARRAY(rf_scene_npc_round_kinds,4);}
    {extern uint32_t rf_scene_rail_scope[8];RAM_ARRAY(rf_scene_rail_scope,8);}
    {extern uint32_t rf_scene_crash_enemy_state[12];RAM_ARRAY(rf_scene_crash_enemy_state,12);}
    {extern uint32_t rf_scene_underwater_fire[4];RAM_ARRAY(rf_scene_underwater_fire,4);}
    {extern uint32_t rf_scene_frame_late_failure[3];RAM_ARRAY(rf_scene_frame_late_failure,3);}
    {extern uint32_t rf_scene_undercover_open_failure[2];RAM_ARRAY(rf_scene_undercover_open_failure,2);}
    {extern uint32_t rf_scene_close_movers_failure[2];RAM_ARRAY(rf_scene_close_movers_failure,2);}
    {extern uint32_t rf_scene_vehicle_switch_boot_failure[3];RAM_ARRAY(rf_scene_vehicle_switch_boot_failure,3);}
    {extern uint32_t rf_scene_medic_syringe[12],rf_scene_medic_syringe_close_refused[3];RAM_ARRAY(rf_scene_medic_syringe,12);RAM_ARRAY(rf_scene_medic_syringe_close_refused,3);}
    {extern uint32_t rf_scene_clutter_bodies[10],rf_scene_glare_instances[10];RAM_ARRAY(rf_scene_clutter_bodies,10);RAM_ARRAY(rf_scene_glare_instances,10);}
    {extern uint32_t rf_scene_swim_drive[6];RAM_ARRAY(rf_scene_swim_drive,6);}
    {extern uint32_t rf_scene_player_swim[12],rf_scene_actor_landing[8];RAM_ARRAY(rf_scene_player_swim,12);RAM_ARRAY(rf_scene_actor_landing,8);}
    {extern uint32_t rf_scene_liquid_damage[8],rf_scene_player_impact[8];RAM_ARRAY(rf_scene_liquid_damage,8);RAM_ARRAY(rf_scene_player_impact,8);}
    {extern uint32_t rf_xbox_flip_waits[4];RAM_ARRAY(rf_xbox_flip_waits,4);}
    {extern uint64_t rf_renderer_model_split[4];RAM_ARRAY(rf_renderer_model_split,8);}
    {extern uint32_t rf_xbox_world_indexed[8];RAM_ARRAY(rf_xbox_world_indexed,8);}
    {extern uint32_t rf_xbox_native_files[35];RAM_ARRAY(rf_xbox_native_files,35);}
    {extern uint32_t rf_scene_quickload_choice[8];RAM_ARRAY(rf_scene_quickload_choice,8);}
    {extern uint32_t rf_xbox_reload_memory[16][8],rf_xbox_contiguous[4];RAM_ARRAY(rf_xbox_reload_memory,128);RAM_ARRAY(rf_xbox_contiguous,4);}
    {extern uint32_t rf_scene_blind_dropped;RAM_ARRAY(&rf_scene_blind_dropped,1);}
    {extern uint32_t rf_scene_crouch_reject[5];RAM_ARRAY(rf_scene_crouch_reject,5);}
    {extern uint32_t rf_scene_teardown_failure[2];RAM_ARRAY(rf_scene_teardown_failure,2);}
    {extern uint32_t rf_scene_world_validate_line;RAM_ARRAY(&rf_scene_world_validate_line,1);}
    {extern uint32_t rf_xbox_frame_failure[3],rf_scene_begin_frame_failure[3];RAM_ARRAY(rf_xbox_frame_failure,3);RAM_ARRAY(rf_scene_begin_frame_failure,3);}
    {extern uint32_t rf_xbox_level_transitions[4],rf_xbox_transition_failure[3];RAM_ARRAY(rf_xbox_level_transitions,4);RAM_ARRAY(rf_xbox_transition_failure,3);}
    {extern uint32_t rf_scene_npc_save_reject[5];RAM_ARRAY(rf_scene_npc_save_reject,5);}
    {extern uint32_t rf_scene_npc_save_relaxed[4];RAM_ARRAY(rf_scene_npc_save_relaxed,4);}
    {extern uint32_t rf_scene_npc_restore_reject[6];RAM_ARRAY(rf_scene_npc_restore_reject,6);}
    {extern uint32_t rf_scene_npc_place_fallback[6];RAM_ARRAY(rf_scene_npc_place_fallback,6);}
    {extern uint32_t rf_scene_checkpoint_world_reject[9];RAM_ARRAY(rf_scene_checkpoint_world_reject,9);}
    {extern uint32_t rf_scene_player_save_block;RAM_ARRAY(&rf_scene_player_save_block,1);}
    {extern uint32_t rf_scene_world_restore_reject[8];RAM_ARRAY(rf_scene_world_restore_reject,8);}
    {extern uint32_t rf_scene_world_mission_reject[2];RAM_ARRAY(rf_scene_world_mission_reject,2);}
    {extern uint32_t rf_scene_world_snapshot_reject[3];RAM_ARRAY(rf_scene_world_snapshot_reject,3);}
    {extern uint32_t rf_xbox_checkpoint_storage_state[8];RAM_ARRAY(rf_xbox_checkpoint_storage_state,8);}
    {extern uint32_t rf_scene_physics_profile[8][4];RAM_ARRAY(rf_scene_physics_profile,32);}
    /* Cumulative per-phase rows (count, total ms lo/hi, max ms); active only
     * on the paced frame clock. Rates come from deltas between samples. */
    {extern uint32_t rf_scene_profile[8][4],rf_scene_step_profile[8][4],rf_scene_npc_step_profile[8][4],
         rf_scene_world_profile[8][4],rf_scene_presentation_profile[8][4];
     RAM_ARRAY(rf_scene_profile,32);RAM_ARRAY(rf_scene_step_profile,32);RAM_ARRAY(rf_scene_npc_step_profile,32);
     RAM_ARRAY(rf_scene_world_profile,32);RAM_ARRAY(rf_scene_presentation_profile,32);}
    {extern uint32_t rf_scene_world_build_profile[4][4];RAM_ARRAY(rf_scene_world_build_profile,16);}
    {extern uint32_t rf_scene_prestep_profile[8][4],rf_scene_npc_playback_profile[8][4];RAM_ARRAY(rf_scene_prestep_profile,32);RAM_ARRAY(rf_scene_npc_playback_profile,32);}
    {extern uint32_t rf_xbox_overlap[8];RAM_ARRAY(rf_xbox_overlap,8);}
    {extern uint32_t rf_scene_vehicle_rest[8];RAM_ARRAY(rf_scene_vehicle_rest,8);}
    {extern uint32_t rf_xbox_gpu_wait[11],rf_xbox_experiment;RAM_ARRAY(rf_xbox_gpu_wait,11);RAM_ARRAY(&rf_xbox_experiment,1);}
    {extern uint32_t rf_scene_rocket_visual[8];RAM_ARRAY(rf_scene_rocket_visual,8);}
    {extern uint32_t rf_preview_world_stats[4],rf_xbox_retained_world[8];RAM_ARRAY(rf_preview_world_stats,4);RAM_ARRAY(rf_xbox_retained_world,8);}
#ifdef RF_HEAP_AUDIT
    /* Memory audit: totals, then the top call sites as "file:line" (c = contiguous), live bytes, blocks. */
    {uint32_t i;rf_heap_audit_snapshot();RAM_ARRAY(rf_heap_audit_totals,8);
     count=snprintf(text+used,sizeof(text)-used,"\"rf_heap_audit_sites\":[");if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count;
     for(i=0;i<RF_HEAP_AUDIT_TOP && rf_heap_audit_top[i][2];i++){
        const char *file=(const char *)(uintptr_t)rf_heap_audit_top[i][0],*slash=strrchr(file,'/'),*back=strrchr(file,'\\');
        if(back>slash)slash=back;
        count=snprintf(text+used,sizeof(text)-used,"%s[\"%s:%u%s\",%u,%u]",i?",":"",slash?slash+1:file,
            (unsigned)(rf_heap_audit_top[i][1]&0x7fffffffu),(rf_heap_audit_top[i][1]>>31)?"c":"",
            (unsigned)rf_heap_audit_top[i][2],(unsigned)rf_heap_audit_top[i][3]);
        if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count;}
     count=snprintf(text+used,sizeof(text)-used,"],");if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count;}
#endif
#undef RAM_ARRAY
    if(used+3>=sizeof(text))return;
    text[used-1]='}';text[used++]='\n';text[used]=0;
    xbox_debug_MirrorWrite(text);
    OutputDebugStringA(text);
}
