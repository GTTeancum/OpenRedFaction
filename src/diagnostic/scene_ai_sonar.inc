#ifndef RF_SCENE_AI_SONAR_INCLUDED
#define RF_SCENE_AI_SONAR_INCLUDED
/* Include after scene_ai_laser.inc. Sonar owns an independent finite pool;
 * only preparation, liquid stepping and nearest direct contacts are shared.
 * Its immutable torpedo flag expires at an admitted authored liquid contact.
 * No impact delay, homing, blast, crater, impact VFX or new audio ownership. */
#define SCENE_AI_SONAR_CAPACITY 64u
static scene_ai_laser_projectile scene_ai_sonars[SCENE_AI_SONAR_CAPACITY];
static uint64_t scene_ai_sonar_epoch;
static uint32_t scene_ai_sonar_updating;
/* Same meanings as Laser: launches,contacts,expiry,live,pool refusals,
 * actor contacts,shield contacts,Nano contacts,last source,target,frame,status. */
uint32_t rf_scene_ai_sonar[12];
static void scene_ai_sonar_reset(void)
{
    scene_sonar_visual_reset();
    memset(scene_ai_sonars,0,sizeof(scene_ai_sonars));++scene_ai_sonar_epoch;
    rf_scene_ai_sonar[3]=0;
    /* A reset inside a contact callback does not reopen the active update. */
}
static uint32_t scene_ai_sonar_pending(void)
{
    uint32_t i;if(scene_ai_sonar_updating)return 1;
    for(i=0;i<SCENE_AI_SONAR_CAPACITY;i++)if(scene_ai_sonars[i].flight.active)return 1;
    return 0;
}
/* Ordinary NPC caller owns range/aim/LOS, cadence and one onset presentation.
 * Source-qualified immediate acceptance/debit is callback-free. Underwater
 * bit0x200 permits submerged firing; it does not require a wet eye. Original
 * 426587's separate wet-muzzle gate is for class flag0x400 (sub), which actual
 * Sea_Creature lacks. Its Movemode "sub" is not that explicit class flag.
 * Do not substitute player swim mode or a guessed wet-only restriction. */
static int scene_ai_sonar_launch(campaign_npc_body *owner,const float target[3])
{
    scene_ai_laser_projectile next={0};rf_random_state random=campaign_enemy_spread_random;
    campaign_enemy_weapon_selection selected={0};const rf_weapon_acquire_definition *ammo;
    uint32_t i,source_slot;int32_t weapon;int status;
    if(!owner || !target)return RF_RANGE;
    status=campaign_enemy_sonar_select(owner,&campaign_weapon_supply,&selected);if(status)return status;
    weapon=owner->view.weapons[0];ammo=selected.ammo;
    for(source_slot=0;source_slot<campaign_npc_body_count;source_slot++)
        if(campaign_npc_bodies+source_slot==owner)break;
    if(source_slot==campaign_npc_body_count ||
       !scene_ai_melee_npc_live(source_slot,owner->registration.handle))return RF_NOT_FOUND;
    if(owner->inventory.loaded[weapon] || owner->inventory.reserve[ammo->ammo_type]<=0)return RF_NOT_FOUND;
    for(i=0;i<SCENE_AI_SONAR_CAPACITY && scene_ai_sonars[i].flight.active;i++){}
    if(i==SCENE_AI_SONAR_CAPACITY){++rf_scene_ai_sonar[4];return RF_NOT_FOUND;}
    status=scene_ai_laser_prepare(owner,target,&scene_ai_sonar_definition,&random,&next);if(status)return status;
    /* Finite reserve readiness is the surrounding port gameplay policy.
     * Original4257c0 proves the clipless debit, not SP NPC exhaustion refusal.
     * No failure/callback can intervene between this debit and publication. */
    status=rf_weapon_consume_shot(&owner->inventory,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,weapon);if(status)return status;
    campaign_enemy_spread_random=random;scene_ai_sonars[i]=next;
    scene_sonar_visual_accept(i,&next,combat_frame);
    ++rf_scene_ai_sonar[0];++rf_scene_ai_sonar[3];rf_scene_ai_sonar[8]=next.source;
    rf_scene_ai_sonar[10]=combat_frame;return RF_OK;
}
static int scene_ai_sonar_tick(scene_stream *stream,uint32_t frame,const float player_eye[3])
{
    return scene_ai_energy_tick(stream,frame,player_eye,scene_ai_sonars,SCENE_AI_SONAR_CAPACITY,
        rf_scene_ai_sonar,&scene_ai_sonar_epoch,&scene_ai_sonar_updating);
}
#endif
