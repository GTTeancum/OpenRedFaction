#ifndef RF_SCENE_BLIND_PURSUIT_COMPOSED_INCLUDED
#define RF_SCENE_BLIND_PURSUIT_COMPOSED_INCLUDED
/* RFNS resolves incoming attachment rather than the outgoing link. Missing
 * ownership is NOT_FOUND; malformed generation/body/death claims remain errors.
 * Hidden living targets retain age but cannot refresh runtime sight. */
static int scene_blind_prepared_live(const scene_npc_checkpoint_restore_entry *e,
    const scene_npc_seat_checkpoint_stage *seats)
{
    const campaign_npc_body *o;float basis[9];uint32_t seated;int status;
    if(!e||!e->registered||e->slot>=campaign_npc_body_count||e->saved.retired||e->saved.dead_pose||
       !isfinite(e->saved.health)||e->saved.health<=0)return RF_FORMAT;
    o=campaign_npc_bodies+e->slot;
    if(scene_npc_reload_identity(e->slot,o)||(o->view.flags_810&1u)||!o->body.allocated_bytes||
       !isfinite(e->body.mass)||e->body.mass<=0)return RF_FORMAT;
    if((o->view.linked_handle!=-1&&!scene_npc_seat_checkpoint_actor(e->handle)))return RF_FORMAT;
    if((o->object_flags&8u)||o->view.occupant_count||o->attachment_75c!=-1||
       e->handle==campaign_player_object.handle)return RF_NOT_FOUND;
    status=scene_npc_seat_checkpoint_basis(seats,&e->saved,basis,&seated);if(status)return status;
    return seated?RF_NOT_FOUND:RF_OK;
}
/* Resolve eligibility independently of the staged receipt. In particular a
 * previously excluded absent row must not acquire grace on a later seal. */
static int scene_blind_composed_target(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_checkpoint_restore_entry *e,const scene_npc_seat_checkpoint_stage *seats,
    const scene_world_player_stage *player,uint32_t player_outside,uint32_t *target)
{
    const rf_npc_checkpoint_record *r=&e->saved;uint32_t expected,j;float seconds;int status;
    *target=0;
    if(r->retired||r->dead_pose||r->health<=0||!r->combat_alert||
       (r->combat.active!=0&&r->combat.active!=2&&r->combat.active!=4))return r->blind_present?RF_FORMAT:RF_OK;
    status=scene_blind_source(e->slot,r->primary,!!r->blind_present,&seconds);
    if(status==RF_NOT_FOUND)return r->blind_present?RF_FORMAT:RF_OK;if(status)return status;
    status=scene_blind_prepared_live(e,seats);
    if(status==RF_NOT_FOUND)return r->blind_present?RF_FORMAT:RF_OK;if(status)return status;
    expected=r->combat.active?e->attack_target_handle:campaign_player_object.handle;
    if(!expected||expected==UINT32_MAX||expected==e->handle)return RF_FORMAT;
    if(expected==campaign_player_object.handle){
        if(r->combat.active==4||r->combat.event)return RF_FORMAT;
        if(!isfinite(player->player.player.health)||player->player.player.health<=0||
           (campaign_player_view.flags_810&1u)||campaign_player_object.view!=&campaign_player_view||
           rf_object_registry_lookup(&campaign_registry,expected)!=&campaign_player_object||
           rf_entity_lookup(&campaign_entities,(int32_t)expected)!=&campaign_player_view)return RF_FORMAT;
        if(player_outside)return r->blind_present?RF_FORMAT:RF_OK;
    }else{
        for(j=0;j<stage->count;j++)if(stage->entries[j].handle==expected)break;
        if(j==stage->count)return r->blind_present?RF_FORMAT:RF_OK; /* Existing vehicle target lane. */
        if(stage->entries[j].saved.uid!=r->combat.event)return RF_FORMAT;
        status=scene_blind_prepared_live(stage->entries+j,seats);
        if(status==RF_NOT_FOUND)return r->blind_present?RF_FORMAT:RF_OK;if(status)return status;
    }
    *target=expected;return RF_OK;
}
static int scene_blind_composed_prepare(scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_world_player_stage *player,uint32_t player_outside)
{
    uint32_t i,target;int status;
    if(!stage||!player||player->npcs!=stage||player_outside>1||stage->blind_joined)return RF_RANGE;
    for(i=0;i<stage->count;i++){
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        status=scene_blind_composed_target(stage,e,seats,player,player_outside,&target);if(status)return status;
        if(!target){e->blind_target=e->blind_last_seen=0;continue;}
        if(e->saved.blind_profile&&!e->saved.blind_present)return RF_FORMAT;
        if(e->blind_target!=target)return RF_FORMAT;
        status=scene_blind_prepared(stage,e);if(status)return status;
    }
    stage->blind_joined=1;return RF_OK;
}
static int scene_blind_composed(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_world_player_stage *player,uint32_t player_outside)
{
    uint32_t i,target;int status;
    if(!stage||!player||player->npcs!=stage||player_outside>1||stage->blind_joined!=1)return RF_RANGE;
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        status=scene_blind_composed_target(stage,e,seats,player,player_outside,&target);if(status)return status;
        if(e->blind_target!=target)return RF_FORMAT;
        status=scene_blind_prepared(stage,e);if(status)return status;
    }
    return RF_OK;
}
#endif
