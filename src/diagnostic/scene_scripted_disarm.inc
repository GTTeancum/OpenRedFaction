#ifndef RF_SCENE_SCRIPTED_DISARM_INCLUDED
#define RF_SCENE_SCRIPTED_DISARM_INCLUDED
/* Drop_Weapon81, original4b9c20..4b9cc7: reset held firing, release held
 * presentation, clear ownership and both selected weapons on a living actor.
 * Include after scene_burning.inc, where campaign_weapon_drop_emit is visible.
 * Installed L1S1/8323 holds Riot Stick; L13S3/8687 and L14S3/9642 name none.
 * First pass uses the existing grounded finite-ammo drop ledger. Unsupported
 * owned primaries, missing floor, used drop slots and external sound/effect
 * reset requirements reject before disarming. No tumbling or secondary drop.
 */
uint32_t rf_scene_scripted_disarm[12]; /* requests,applied,newdrop,idempotent,rejected,UID,handle,status,reason,oldPrimary,quantity,nowMs */
uint32_t rf_scene_scripted_disarm_apply[32]; /* Immutable first successful change; layout in sample below. */
uint32_t rf_scene_scripted_disarm_live[16];
static uint32_t scene_scripted_disarm_actor=UINT32_MAX;
static void scene_scripted_disarm_reset(void)
{
    memset(rf_scene_scripted_disarm,0,sizeof(rf_scene_scripted_disarm));
    memset(rf_scene_scripted_disarm_apply,0,sizeof(rf_scene_scripted_disarm_apply));
    memset(rf_scene_scripted_disarm_live,0,sizeof(rf_scene_scripted_disarm_live));
    scene_scripted_disarm_actor=UINT32_MAX;
}
static uint32_t scene_scripted_disarm_owned(const campaign_npc_body *o)
{uint32_t n=0;for(uint32_t i=0;i<64;i++)n+=o->inventory.owned[i]!=0;return n;}
static void scene_scripted_disarm_sample(void)
{
    uint32_t *p=rf_scene_scripted_disarm_live,i;campaign_npc_body *o;
    if(scene_scripted_disarm_actor==UINT32_MAX)return;
    memset(p,0,sizeof(rf_scene_scripted_disarm_live));p[2]=scene_scripted_disarm_actor;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==scene_scripted_disarm_actor)break;
    if(i==campaign_npc_body_count){p[15]=RF_NOT_FOUND;return;}o=campaign_npc_bodies+i;
    if(o->registration.view!=&o->view || rf_object_registry_lookup(&campaign_registry,p[2])!=&o->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)p[2])!=&o->view || i>=campaign_seeds.records.count){p[15]=RF_NOT_FOUND;return;}
    p[0]=1;p[1]=(uint32_t)campaign_seeds.records.items[i].record.uid;
    p[3]=(uint32_t)o->view.weapons[0];p[4]=(uint32_t)o->view.weapons[1];p[5]=scene_scripted_disarm_owned(o);
    if(o->persistence_registered && o->persistence_slot<rf_scene_defeated_actors.count){
        const rf_campaign_weapon_drop *d=rf_scene_defeated_actors.drops+o->persistence_slot;
        p[6]=d->state;p[7]=(uint32_t)d->weapon;p[8]=(uint32_t)d->quantity;
    }
    memcpy(p+9,&o->damage.effects.health,4);p[10]=o->script_move.active;p[11]=o->combat_burst_remaining;
    p[12]=o->combat_reload_due;p[13]=o->script_once.count;
    p[14]=rf_weapon_world_model_token(campaign_weapon_models.weapons,o->view.weapons[0]);
}
static int campaign_drop_npc_weapon(void *context,uint32_t handle)
{
    campaign_npc_body *o;rf_entity_pose *pose=NULL;rf_entity_motion_selection selection;
    rf_weapon_reset_state firing;rf_motion_playback_state playback;
    const rf_entity_playback_model *model;rf_campaign_weapon_drop *drop=NULL;
    rf_weapon_reset_context reset={0,campaign_weapon_reset.names.count};
    uint32_t slot,cls,owned,reason=1,before[32]={0};int32_t weapon,quantity=0;int status;(void)context;
    ++rf_scene_scripted_disarm[0];rf_scene_scripted_disarm[5]=UINT32_MAX;
    rf_scene_scripted_disarm[6]=handle;rf_scene_scripted_disarm[7]=rf_scene_scripted_disarm[8]=0;
    rf_scene_scripted_disarm[9]=UINT32_MAX;rf_scene_scripted_disarm[10]=0;
    rf_scene_scripted_disarm[11]=(uint32_t)campaign_blackout_now;
    status=campaign_npc_motion_owner(handle,&o,&cls,&pose);if(status)goto fail;
    slot=(uint32_t)(o-campaign_npc_bodies);
    if(o->registration.view!=&o->view || rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration ||
       !(o->damage.effects.health>0) || (o->view.flags_810&1u)){status=RF_NOT_FOUND;goto fail;}
    rf_scene_scripted_disarm[5]=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    weapon=o->view.weapons[0];owned=scene_scripted_disarm_owned(o);rf_scene_scripted_disarm[9]=(uint32_t)weapon;
    if(weapon< -1 || weapon>=64 || o->view.weapons[1]< -1 || o->view.weapons[1]>=64){status=RF_FORMAT;goto fail;}
    if(weapon==-1 && o->view.weapons[1]==-1 && !owned && !o->combat_burst_remaining &&
       !o->combat_reload_due && !o->script_once.count && !o->script_shoot.active){
        ++rf_scene_scripted_disarm[3];scene_scripted_disarm_sample();return RF_OK;
    }
    /* Stage only local state. The reset primitive's resources are read-only;
     * absent reached audio/effect callbacks reject, never silently leak owners. */
    reason=2;status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&selection);if(status)goto fail;
    if(!campaign_playback_resources.models || pose->skeleton>=campaign_playback_resources.model_count ||
       !reset.weapon_count){status=RF_NOT_FOUND;goto fail;}
    model=campaign_playback_resources.models+pose->skeleton;playback=pose->playback;firing=o->firing;
    firing.flags_7d0=o->view.flags_7d0;firing.flags_810=o->view.flags_810;
    firing.character_present=1;firing.player_present=0;
    for(uint32_t i=0;i<64;i++)if(firing.active[i] && (int32_t)i!=weapon && (int32_t)i!=o->view.weapons[1]){status=RF_NOT_FOUND;goto fail;}
    status=rf_weapon_reset(&firing,weapon,campaign_weapon_reset.definitions,&reset,&playback,model->resources,model->count,NULL,NULL);if(status)goto fail;
    if(o->view.weapons[1]!=weapon){status=rf_weapon_reset(&firing,o->view.weapons[1],campaign_weapon_reset.definitions,&reset,&playback,model->resources,model->count,NULL,NULL);if(status)goto fail;}
    if(weapon>=0 && o->inventory.owned[weapon]){
        reason=3;status=campaign_enemy_drop_supply(&o->inventory,&campaign_weapon_supply,weapon,&quantity);if(status)goto fail;
        reason=4;
        if(!o->persistence_registered || o->persistence_slot>=rf_scene_defeated_actors.count){status=RF_NOT_FOUND;goto fail;}
        drop=rf_scene_defeated_actors.drops+o->persistence_slot;
        /* One persistent drop per actor. Re-arming after a collected drop is
         * outside this slice; do not erase a second gun without its pickup. */
        if(drop->state){status=RF_NOT_FOUND;goto fail;}
        reason=5;status=campaign_weapon_drop_emit(o);if(status)goto fail;
        if(drop->state!=1){status=RF_NOT_FOUND;goto fail;} /* No grounded hit. */
        ++rf_scene_scripted_disarm[2];
    }
    /* All fallible work completed. Inventory/model publication cannot fail. */
    before[0]=1;before[1]=rf_scene_scripted_disarm[5];before[2]=handle;before[3]=(uint32_t)campaign_blackout_now;
    before[4]=(uint32_t)weapon;before[5]=(uint32_t)o->view.weapons[1];before[8]=owned;
    memcpy(before+13,&o->damage.effects.health,4);before[15]=o->script_move.active;
    before[17]=o->combat_burst_remaining;before[19]=o->combat_reload_due;before[21]=o->script_once.count;
    o->selection=selection;o->firing=firing;pose->playback=playback;
    o->view.flags_7d0=firing.flags_7d0;o->view.flags_810=o->damage.effects.flags_810=firing.flags_810;
    memset(&o->inventory,0,sizeof(o->inventory));memset(o->firing.active,0,sizeof(o->firing.active));
    /* Ordinary weapon draw/muzzle owners derive their model from these IDs;
     * shared class resources and reusable hand tags must not be destroyed. */
    o->view.weapons[0]=o->view.weapons[1]=-1;
    o->combat_burst_remaining=o->combat_due=o->combat_reload_due=0;o->combat_reload_weapon=-1;
    memset(&o->script_once,0,sizeof(o->script_once));memset(&o->script_shoot,0,sizeof(o->script_shoot));
    if(o->combat_scripted==3)o->combat_scripted=o->combat_target=0;
    ++rf_scene_scripted_disarm[1];rf_scene_scripted_disarm[10]=(uint32_t)quantity;
    if(!rf_scene_scripted_disarm_apply[0]){
        before[6]=before[7]=UINT32_MAX;memcpy(before+14,&o->damage.effects.health,4);before[16]=o->script_move.active;
        before[23]=rf_weapon_world_model_token(campaign_weapon_models.weapons,o->view.weapons[0]);
        before[24]=o->persistence_registered?o->persistence_slot:UINT32_MAX;
        if(drop){before[10]=drop->state;before[11]=(uint32_t)drop->weapon;before[12]=(uint32_t)drop->quantity;memcpy(before+25,drop->position,12);}
        memcpy(before+28,o->body.state.position,12);before[31]=o->script_shoot.active;
        memcpy(rf_scene_scripted_disarm_apply,before,sizeof(before));scene_scripted_disarm_actor=handle;
    }
    scene_scripted_disarm_sample();return RF_OK;
fail:
    ++rf_scene_scripted_disarm[4];rf_scene_scripted_disarm[7]=(uint32_t)status;rf_scene_scripted_disarm[8]=reason;return status;
}
#endif
