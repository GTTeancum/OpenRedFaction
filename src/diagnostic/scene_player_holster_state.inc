#ifndef RF_SCENE_PLAYER_HOLSTER_STATE_INCLUDED
#define RF_SCENE_PLAYER_HOLSTER_STATE_INCLUDED
/* Original428f00/428e90 own entity810 bit0x800, independently of primary
 * selection and inventory. Do not reuse explicit_unarmed or NPC7d0 bit0x200.
 * Include after the player import/export globals. No heap or resource owner. */
static uint32_t scene_player_holster_held,scene_player_holster_raw,scene_player_holster_release;
static uint32_t scene_player_holster_export,scene_player_holster_import;
static uint32_t scene_player_holster_busy(const scene_stream *);
static uint32_t scene_player_weapon_holstered(void)
{return !!(campaign_player_view.flags_810&0x800u);}
static void scene_player_holster_set(uint32_t holstered)
{
    uint32_t bit=holstered?0x800u:0;
    campaign_player_view.flags_810=(campaign_player_view.flags_810&~0x800u)|bit;
    campaign_player_damage.state.effects.flags_810=(campaign_player_damage.state.effects.flags_810&~0x800u)|bit;
}
static uint32_t scene_player_holster_controls(void)
{return (player_input.fire?1u:0u)|(player_input.alt_fire?2u:0u)|(player_input.reload?4u:0u);}
static void scene_player_holster_sample(uint32_t held)
{scene_player_holster_raw=!!held;}
/* Assignment-only restore. Consume the physical edge, not a saved action. */
static void scene_player_holster_restore(uint32_t holstered)
{
    scene_player_holster_set(holstered);
    scene_player_holster_export=scene_player_holster_import=0;
    scene_player_holster_held=scene_player_holster_raw;
    scene_player_holster_release=scene_player_holster_controls();
}
static void scene_player_holster_carry_reset(void)
{scene_player_holster_export=scene_player_holster_import=0;}
/* The ordinary export already owns the exact validated player snapshot.
 * Compare it before teardown instead of duplicating an inventory sidecar.
 * Disk loads must reset this lane; matching bytes are not a disk authority. */
static void scene_player_holster_carry_stage(void)
{
    const rf_campaign_player_state *a=&campaign_player_export,*b=&campaign_player_import;
    scene_player_holster_import=0;
    if(campaign_export_valid && scene_player_holster_export && a->weapon<64 &&
       a->weapon==b->weapon && a->catalog_hash==b->catalog_hash &&
       a->health==b->health && a->armor==b->armor &&
       !memcmp(&a->inventory,&b->inventory,sizeof(a->inventory)))scene_player_holster_import=1;
}
#endif
