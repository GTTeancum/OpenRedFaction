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
extern uint32_t rf_scene_submarine_weapon[8],rf_scene_drill_state[10];
extern uint32_t rf_scene_vehicle_entry_probe[16];
extern uint32_t rf_scene_vehicle_exit_probe[80];
extern uint32_t rf_scene_use_selection[10];
static void rf_ram_log_sample(uint32_t force)
{
    static int initialized,enabled;
    static uint32_t previous=UINT32_MAX;
    static char text[12288];
    size_t used=0;int count;
    if(!initialized){FILE *f=tester_profile?NULL:fopen("D:\\crash-ram-log.flag","rb");
        initialized=1;enabled=f!=NULL;if(f)fclose(f);}
    if(!enabled)return;
    uint32_t frame=rf_player_replay_diagnostic[2];
    if(!force && (frame==previous || frame%60))return;
    previous=frame;g_XboxBootPhase=rf_diagnostic[2];
    used=(size_t)snprintf(text,sizeof(text),"RFLOG {");
#define RAM_ARRAY(name,n) do {     count=snprintf(text+used,sizeof(text)-used,"\"" #name "\":[");     if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count;     for(uint32_t i=0;i<(n);i++){uint32_t word;memcpy(&word,(const void*)((const unsigned char*)(name)+i*4),4);         count=snprintf(text+used,sizeof(text)-used,"%s%u",i?",":"",word);         if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count;}     count=snprintf(text+used,sizeof(text)-used,"],");     if(count<0 || (size_t)count>=sizeof(text)-used)return;used+=(size_t)count; } while(0)
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
    RAM_ARRAY(rf_scene_drone_player_contact,10);
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
    {extern uint32_t rf_scene_world_player_probe[2];RAM_ARRAY(rf_scene_world_player_probe,2);}
    {extern uint32_t rf_scene_remote_hash_trace[7];RAM_ARRAY(rf_scene_remote_hash_trace,7);}
    {extern uint32_t rf_scene_tactical_failure[3];RAM_ARRAY(rf_scene_tactical_failure,3);}
    {extern uint32_t rf_scene_world_load_fallback[3];RAM_ARRAY(rf_scene_world_load_fallback,3);}
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
#undef RAM_ARRAY
    if(used+3>=sizeof(text))return;
    text[used-1]='}';text[used++]='\n';text[used]=0;
    xbox_debug_MirrorWrite(text);
    OutputDebugStringA(text);
}
