#ifndef RF_SCENE_CORPSE_SOURCE_RETIREMENT_INCLUDED
#define RF_SCENE_CORPSE_SOURCE_RETIREMENT_INCLUDED
/* Include after campaign_remove_object, NPC checkpoint level-name comparison,
 * script-single queue helpers and generated retirement. Forward-declare the
 * entrypoint before campaign_live_corpses_tick. Invoke ONLY after successful
 * live corpse expiry/release, never from full scene teardown or model close. */
uint32_t rf_scene_corpse_source_retirement[12]; /* retired,repeated,UID,error,death action,drop state,weapon,rounds,health bits,registered,body bytes,RFNC retired */
static void scene_corpse_source_retirement_reset(void)
{memset(rf_scene_corpse_source_retirement,0,sizeof(rf_scene_corpse_source_retirement));}

static int scene_corpse_source_retire(uint32_t model,uint32_t uid)
{
    campaign_npc_body *actor;campaign_model_owner *render;rf_campaign_object_record *key;
    rf_campaign_weapon_drop drop;float health;uint32_t slot,persistent;int status;
    if(!model||uid==UINT32_MAX){status=RF_RANGE;goto fail;}slot=model-1;
    if(!campaign_npc_bodies||!campaign_model_owners||slot>=campaign_npc_body_count||
       slot>=campaign_model_owner_count||slot>=campaign_seeds.records.count||
       (uint32_t)campaign_seeds.records.items[slot].record.uid!=uid){status=RF_RANGE;goto fail;}
    actor=campaign_npc_bodies+slot;render=campaign_model_owners+slot;
    /* The visual owner must already be released, not merely fading/hidden. */
    if(render->owned||render->pose||render->registration.loaded||!actor->persistence_registered||
       actor->persistence_slot>=rf_scene_defeated_actors.count||actor->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS||
       !isfinite(actor->damage.effects.health)||actor->damage.effects.health>0||!(actor->view.flags_810&1u)) {
        status=RF_NOT_FOUND;goto fail;
    }
    persistent=actor->persistence_slot;key=rf_scene_defeated_actors.items+persistent;
    if(key->uid!=uid||key->level>=rf_scene_defeated_actors.level_count||key->level>=RF_CAMPAIGN_PICKUP_LEVELS||
       !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[key->level],campaign_current_level)||
       actor->script_once.count>16){status=RF_FORMAT;goto fail;}
    health=actor->damage.effects.health;drop=rf_scene_defeated_actors.drops[persistent];
    if(!actor->registration.view) {
        if(!key->retired||actor->body.allocated_bytes){status=RF_FORMAT;goto fail;}
        ++rf_scene_corpse_source_retirement[1];
    } else {
        if(actor->registration.view!=&actor->view||
           rf_object_registry_lookup(&campaign_registry,actor->registration.handle)!=&actor->registration||
           rf_entity_lookup(&campaign_entities,(int32_t)actor->registration.handle)!=&actor->view) {
            status=RF_NOT_FOUND;goto fail;
        }
        /* Reuse ordinary terminal NPC ownership teardown, including generated
         * children and seat backlinks. It requests no damage/death/loot/audio.
         * Existing resolved When_Dead references treat this known absence as
         * dead; the stable authored/persistence row itself remains retained. */
        status=campaign_remove_object(NULL,actor->registration.handle);if(status)goto fail;
        actor->damage.effects.health=health; /* Preserve the actual fatal value. */
        ++rf_scene_corpse_source_retirement[0];
    }
    /* Also covers an already-retired source; ordinary removal now cancels
     * these same borrowed banks before returning. Repeating is inert. */
    scene_npc_retirement_cancel_orders(actor);
    key->retired=1;rf_scene_defeated_actors.vitals[persistent].valid=0;
    rf_scene_defeated_actors.vitals[persistent].health=health;
    rf_scene_defeated_actors.vitals[persistent].armor=actor->damage.effects.armor;
    rf_scene_defeated_actors.mission[persistent].affiliation=actor->damage.effects.affiliation;
    rf_scene_defeated_actors.mission[persistent].flags=actor->object_flags&0x4004u;
    /* Do not regenerate a weapon drop or discard a collected-drop tombstone. */
    rf_scene_defeated_actors.drops[persistent]=drop;
    rf_scene_corpse_source_retirement[2]=uid;rf_scene_corpse_source_retirement[3]=0;
    rf_scene_corpse_source_retirement[4]=(uint32_t)actor->death.action_824;
    rf_scene_corpse_source_retirement[5]=drop.state;rf_scene_corpse_source_retirement[6]=(uint32_t)drop.weapon;
    rf_scene_corpse_source_retirement[7]=(uint32_t)drop.quantity;
    memcpy(rf_scene_corpse_source_retirement+8,&health,4);
    rf_scene_corpse_source_retirement[9]=actor->registration.view!=NULL;
    rf_scene_corpse_source_retirement[10]=actor->body.allocated_bytes;
    rf_scene_corpse_source_retirement[11]=key->retired;return RF_OK;
fail:rf_scene_corpse_source_retirement[3]=(uint32_t)status;return status;
}
#endif
