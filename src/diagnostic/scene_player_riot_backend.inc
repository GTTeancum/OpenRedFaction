#ifndef RF_SCENE_PLAYER_RIOT_BACKEND_INCLUDED
#define RF_SCENE_PLAYER_RIOT_BACKEND_INCLUDED
/* Scene adapters for accepted primary/alternate contacts. Input/charge/cadence remain
 * in campaign_combat_tick; contact uses the current pose at authored expiry. */
static uint32_t scene_player_riot_live(void *context,uint32_t source,int32_t weapon)
{
    const scene_stream *stream=context;
    return stream && scene_ai_drone_smash_player_live(source) &&
        !(campaign_player_damage.object_flags&(2u|0x4000u)) &&
        campaign_equipped_slot==2 && !campaign_explicit_unarmed &&
        weapon>=0 && weapon<64 && weapon==campaign_riot_id &&
        weapon==scene_player_riot_definition.weapon &&
        campaign_player_inventory.owned[weapon] &&
        !scene_driller_active(stream) && !scene_turret_player_active() &&
        !rf_scene_combat[6] && !rf_scene_enemy_combat[6];
}
static int scene_player_riot_present(void *context,const scene_riot_primary_strike *strike)
{
    (void)context;(void)strike;
    ++rf_scene_combat[0];campaign_last_alt=0;
    combat_sound("Riot Attack",rf_scene_gameplay_eye);
    return RF_OK;
}
static int scene_player_riot_contact_backend(void *,const scene_riot_primary_strike *,
    uint32_t,const float[3],const float[3],uint32_t);
static const scene_riot_primary_backend scene_player_riot_backend={
    scene_player_riot_live,scene_player_riot_present,scene_player_riot_contact_backend};
#include "scene_player_riot_contact.inc"
static int scene_player_riot_contact_backend(void *context,
    const scene_riot_primary_strike *strike,uint32_t frame,
    const float eye[3],const float direction[3],uint32_t due_mask)
{
    return scene_player_riot_primary_contact(context,&scene_player_riot_owner,
        &scene_player_riot_backend,context,strike,frame,eye,direction,due_mask);
}

static uint32_t scene_player_riot_alternate_live(void *context,uint32_t source,int32_t weapon)
{
    /* Accepted pulse survives release and final-cell exhaustion. No held-input
     * or loaded-charge query; explicit lifecycle/reload wrappers retire it. */
    return scene_player_riot_live(context,source,weapon) &&
        scene_player_riot_alternate_definition.ready &&
        weapon==scene_player_riot_alternate_definition.weapon;
}
static int scene_player_riot_alternate_present(void *context,
    const scene_riot_alternate_strike *strike)
{
    (void)context;(void)strike;
    ++rf_scene_combat[0];campaign_last_alt=1;
    combat_sound("Riot Attack Taser",rf_scene_gameplay_eye);
    return RF_OK;
}
typedef struct scene_player_riot_alternate_contact_guard {
    void *context;const scene_riot_alternate_strike *strike;
} scene_player_riot_alternate_contact_guard;
static uint32_t scene_player_riot_alternate_contact_current(const void *context)
{
    const scene_player_riot_alternate_contact_guard *guard=context;
    return guard && scene_riot_alternate_contact_current(
        &scene_player_riot_alternate_owner,guard->strike) &&
        scene_player_riot_alternate_live(guard->context,guard->strike->source,
            guard->strike->weapon);
}
static int scene_player_riot_alternate_contact_backend(void *context,
    const scene_riot_alternate_strike *strike,uint32_t frame,
    const float eye[3],const float direction[3])
{
    scene_player_riot_alternate_contact_guard guard={context,strike};
    scene_player_riot_contact_record contact;
    if(!strike)return RF_RANGE;
    /* Service already reserved this ticket. Commit once before callbacks. */
    scene_continuous_charge_attempt(strike->source,strike->weapon);
    contact=(scene_player_riot_contact_record){strike->source,strike->mode,strike->weapon,
        strike->kind,strike->damage,scene_player_riot_alternate_contact_current,&guard};
    return scene_player_riot_melee_contact(context,&contact,frame,eye,direction);
}
static const scene_riot_alternate_backend scene_player_riot_alternate_backend={
    scene_player_riot_alternate_live,scene_player_riot_alternate_present,
    scene_player_riot_alternate_contact_backend};
#endif
