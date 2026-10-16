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

extern uint32_t rf_scene_combat_population[4],rf_scene_crash_activity[4];
extern uint32_t rf_scene_crash_scenario_trace[8],rf_scene_enemy_failure[6],rf_scene_crash_terrain[8];
extern uint32_t rf_scene_crash_terrain_owner[8];
extern uint32_t rf_scene_crash_terrain_admission[10];
extern uint32_t rf_scene_drone_player_contact[10];
extern uint32_t rf_scene_drone_separation[20];
extern uint32_t rf_scene_drone_smash[12],rf_scene_drone_secondary[12];
extern uint32_t rf_scene_grenade_contact_trace[16];
extern uint32_t rf_scene_crash_flash[6],rf_scene_noise_live_trace[20],rf_scene_noise_nav_delay[4];
extern uint32_t rf_scene_contact_admission[12];
extern uint32_t rf_scene_drone_owner_admission[16];
extern uint32_t rf_scene_campaign_load_stage,rf_scene_campaign_load_detail;
extern uint32_t rf_scene_npc_body_admission[4],rf_scene_npc_body_failure[2],rf_scene_patrol_failure[3];
extern uint32_t rf_scene_clutter_load_trace[8],rf_scene_clutter_materials[8],rf_material_failure[3];
extern char rf_material_failure_name[61];
extern uint32_t rf_scene_profile_stage[2],rf_preview_failure[8];
extern float rf_scene_gameplay_eye[3];
static void rf_ram_log_sample(uint32_t force)
{
    static int initialized,enabled;
    static uint32_t previous=UINT32_MAX;
    static char text[8192];
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
    RAM_ARRAY(rf_scene_enemy_combat,8);
    RAM_ARRAY(rf_scene_combat_population,4);
    RAM_ARRAY(rf_scene_crash_activity,4);
    RAM_ARRAY(rf_scene_rockets,8);
    RAM_ARRAY(rf_scene_grenades,8);
    RAM_ARRAY(rf_scene_grenade_contact_trace,16);
    RAM_ARRAY(rf_scene_rocket_blast,8);
    RAM_ARRAY(rf_scene_geomod,8);
    RAM_ARRAY(rf_scene_debris,8);
    RAM_ARRAY(rf_scene_crash_scenario_trace,8);
    RAM_ARRAY(rf_scene_enemy_failure,6);
    RAM_ARRAY(rf_scene_profile_stage,2);
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
#undef RAM_ARRAY
    if(used+3>=sizeof(text))return;
    text[used-1]='}';text[used++]='\n';text[used]=0;
    xbox_debug_MirrorWrite(text);
    OutputDebugStringA(text);
}
