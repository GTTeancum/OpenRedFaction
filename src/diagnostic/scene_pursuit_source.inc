#ifndef RF_SCENE_PURSUIT_SOURCE_INCLUDED
#define RF_SCENE_PURSUIT_SOURCE_INCLUDED
/* UNCALLED bounded broader-return source qualification. Existing tactical
 * qualification alone does not exclude an authored Alarm UID.408ac0 invokes
 *4087a0 before weapon/reset/recovery work; the existing Waiting-only alarm
 * consumer is not a general replacement for that branch. Until those owners
 * are composed, retain legacy behavior for every actor with an authored UID,
 * even if its event is missing or currently inactive. Never infer no Alarm
 * from scene_alarm_runner_authored returning RF_NOT_FOUND for another reason.
 * No actor fields, allocation, runtime event lookup or save changes here.
 */
static int scene_medic_flight_raw_string(const uint8_t *,uint32_t,uint32_t *);
static int scene_pursuit_source(uint32_t slot)
{
    const rf_level_owned_entity *record;rf_level_entity_spawn checked;
    const uint8_t *raw;uint32_t at=4,size;int status;
    status=scene_tactical_source(slot);if(status)return status;
    record=campaign_seeds.records.items+slot;
    /* Validate the entire retained v180 record before projecting its UID. */
    status=rf_level_entity_spawn_read(record,&checked);if(status)return status;
    raw=record->raw;size=record->record.bytes;
    status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    if(at>size||size-at<48)return RF_FORMAT;at+=48;
    status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    if(at>size||size-at<13)return RF_FORMAT;at+=13;
    for(uint32_t i=0;i<2;i++){
        status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    }
    if(at>size||size-at<29)return RF_FORMAT;at+=29;
    for(uint32_t i=0;i<7;i++){
        status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    }
    if(at>size||size-at<18)return RF_FORMAT;
    /*464401/46440c: third authored UID becomes actor76c (AI4cc).
     * All-ones is absent, independent of the live global alarm state. */
    for(uint32_t i=14;i<18;i++)if(raw[at+i]!=255)return RF_NOT_FOUND;
    return RF_OK;
}
/* Fresh ordinary return entry domain, before prelude effects. Include this
 * after tactical qualification, target readers and return weapon gates.
 * Original action17 returns immediately; action11 clears one actor flag and
 * selects behavior9 before the normal prelude. Neither branch may be treated
 * as ordinary action3 admission. Leave both to their existing owner until a
 * complete adapter for those branches is installed. Missing/nonactor targets
 * similarly require outer cancellation/default handling, not fabricated entry.
 * This qualification performs no route, collision, demand or actor mutation. */
static inline int scene_pursuit_return_owner(uint32_t slot,uint32_t *target,
    uint32_t *melee_only)
{
    const campaign_npc_body *o;uint32_t resolved,melee;int status;
    if(!target||!melee_only)return RF_RANGE;
    status=scene_pursuit_source(slot);if(status)return status;
    status=scene_tactical_owner(slot);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(!scene_tactical_service_allowed(o)||
       (o->ai_mode.flags_530&0x40000000u)||
       o->ai_mode.action_280==17||o->ai_mode.action_280==11)return RF_NOT_FOUND;
    /*408ef0 reads AI530 bit100; recovery reads its actor7d0 mirror.
     * A split bit cannot silently turn source recovery into a no-op. */
    if(((o->ai_mode.flags_530^o->view.flags_7d0)|
        (o->ai_mode.flags_530^o->firing.flags_7d0))&0x100u)return RF_FORMAT;
    resolved=scene_blind_target_handle(o);
    if(resolved==o->registration.handle||!scene_blind_target_live(resolved))
        return RF_NOT_FOUND;
    status=scene_pursuit_return_weapon_gate(o->view.weapons[0],o->view.weapons[1],
        campaign_weapon_reset.definitions,campaign_weapon_reset.names.count,&melee);
    if(status)return status;
    *target=resolved;*melee_only=melee;return RF_OK;
}
#endif
