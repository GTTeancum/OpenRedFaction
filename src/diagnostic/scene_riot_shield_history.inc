#ifndef RF_SCENE_RIOT_SHIELD_HISTORY_INC
#define RF_SCENE_RIOT_SHIELD_HISTORY_INC
/* First-pass in-memory revisit policy, no disk format. Include after NPC shield
 * gameplay adapter. Durable key is level-index+UID from the existing campaign
 * actor catalog; that catalog owns level names until new-campaign reset.
 * Only touched shields occupy this sparse128-record table (2564 bytes).
 * No handles, actor indices, model IDs or raw owner pointers are retained. */
#define SCENE_SHIELD_HISTORY_CAPACITY 128u
typedef struct scene_shield_saved {
    uint32_t level,uid;rf_clutter_damage_state damage;
} scene_shield_saved;
static scene_shield_saved scene_shield_history[SCENE_SHIELD_HISTORY_CAPACITY];
static uint32_t scene_shield_history_count;
static void scene_npc_shield_history_reset(void)
{memset(scene_shield_history,0,sizeof(scene_shield_history));scene_shield_history_count=0;}
static int scene_npc_shield_history_key(uint32_t actor,uint32_t *level,uint32_t *uid)
{
    const campaign_npc_body *npc;const rf_campaign_object_record *key;uint32_t slot;
    if(actor>=campaign_npc_body_count || !campaign_npc_bodies)return RF_RANGE;
    npc=campaign_npc_bodies+actor;if(!npc->persistence_registered)return RF_NOT_FOUND;
    slot=npc->persistence_slot;if(slot>=rf_scene_defeated_actors.count || slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_RANGE;
    key=rf_scene_defeated_actors.items+slot;
    if(key->level>=rf_scene_defeated_actors.level_count)return RF_RANGE;
    *level=key->level;*uid=key->uid;return RF_OK;
}
static uint32_t scene_npc_shield_history_find(uint32_t level,uint32_t uid)
{uint32_t i;for(i=0;i<scene_shield_history_count;i++)if(scene_shield_history[i].level==level && scene_shield_history[i].uid==uid)break;return i;}
static int scene_npc_shield_history_capture(void)
{
    uint32_t pass,i,needed=0;int status;
    if(scene_npc_shields.count>campaign_npc_body_count || (scene_npc_shields.count && !scene_npc_shields.owners))return RF_RANGE;
    /* Preflight prevents a full history from partially saving this level. */
    for(pass=0;pass<2;pass++)for(i=0;i<scene_npc_shields.count;i++){
        const scene_riot_shield_owner *shield=scene_npc_shields.owners+i;uint32_t level,uid,slot;
        /* Retired actors are restored by defeated-actor history, not this
         * equipment owner; unregister may already have cleared their handle. */
        if(!shield->initialized || !campaign_npc_bodies[i].registration.view)continue;
        if(shield->handle!=campaign_npc_bodies[i].registration.handle)return RF_FORMAT;
        if(!isfinite(shield->damage.health))return RF_FORMAT;
        status=scene_npc_shield_history_key(i,&level,&uid);if(status)return status;
        slot=scene_npc_shield_history_find(level,uid);
        if(!pass){if(slot==scene_shield_history_count && ++needed>SCENE_SHIELD_HISTORY_CAPACITY-scene_shield_history_count)return RF_RANGE;continue;}
        if(slot==scene_shield_history_count)++scene_shield_history_count;
        scene_shield_history[slot]=(scene_shield_saved){level,uid,shield->damage};
    }
    return RF_OK;
}
static int scene_npc_shield_history_restore(void)
{
    uint32_t i;int status;int32_t weapon=scene_npc_shields.weapon;
    if(weapon<0 || weapon>=64 || scene_npc_shields.count>campaign_npc_body_count ||
       (scene_npc_shields.count && !scene_npc_shields.owners))return RF_RANGE;
    for(i=0;i<scene_npc_shields.count;i++){
        campaign_npc_body *npc=campaign_npc_bodies+i;scene_riot_shield_owner *shield=scene_npc_shields.owners+i;
        uint32_t level,uid,slot,changed;
        if(!npc->registration.view)continue;
        status=scene_npc_shield_history_key(i,&level,&uid);if(status)return status;
        slot=scene_npc_shield_history_find(level,uid);if(slot==scene_shield_history_count)continue;
        shield->initialized=1;shield->handle=npc->registration.handle;shield->damage=scene_shield_history[slot].damage;
        if(shield->damage.health<=0){
            /* Constructor and normal inventory restoration may grant the shield
             * again; remembered breakage wins over both on every revisit. */
            npc->inventory.owned[weapon]=0;
            status=campaign_enemy_broken_shield_fallback(i,weapon,&changed);if(status)return status;
            if(!changed && npc->view.weapons[0]==weapon){
                status=campaign_pain_reset_retained(NULL,npc->registration.handle,weapon);if(status)return status;
                npc->view.weapons[0]=-1;npc->combat_burst_remaining=npc->combat_due=npc->combat_reload_due=0;
                npc->combat_reload_weapon=-1;npc->combat_navigation_due=0;
            }
        }
    }
    return RF_OK;
}
#endif
