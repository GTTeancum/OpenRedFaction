#ifndef RF_SCENE_NPC_RELOAD_PLAYER_INCLUDED
#define RF_SCENE_NPC_RELOAD_PLAYER_INCLUDED
/* Current and candidate possession are independent proofs. This does not
 * modify boarding, evict occupants, or cancel an accepted reload on boarding.
 * An unrelated authored NPC driver keeps the existing RFNS parked admission. */
static int scene_npc_reload_player_admit(const scene_stream *s,scene_npc_reload_player_receipt *out)
{
    scene_npc_reload_player_receipt value={0};const scene_driller_runtime *r;int status;
    uint32_t handle=campaign_player_object.handle;
    if(!s||!out||scene_turret_player_active()||scene_player_jeep_gunner_active(s)||
       campaign_player_object.object_kind!=0||campaign_player_object.view!=&campaign_player_view||
       campaign_player_view.type!=0||(uint32_t)campaign_player_view.handle!=handle||
       campaign_player_damage.state.effects.handle!=handle||campaign_player_view.linked_handle!=-1||
       rf_object_registry_lookup(&campaign_registry,handle)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&campaign_player_view||
       !scene_actor_body.allocated_bytes||!scene_actor_body.spheres.items||!scene_actor_body.spheres.count)return RF_NOT_FOUND;
    value.epoch=scene_robot_death_epoch;value.stream=s;value.player=handle;value.runtime=s->driller_runtime;
    value.spheres=scene_actor_body.spheres.items;value.sphere_count=scene_actor_body.spheres.count;
    value.body_bytes=scene_actor_body.allocated_bytes;value.sphere_bytes=scene_actor_body.spheres.allocated_bytes;
    r=s->driller_runtime;
    if(r){
        const scene_driller_entry_exit *e=&r->entry;
        /* Authenticate pointers before the old parked helper dereferences
         * player_view or runtime/host aliases. Keep all its existing checks. */
        if(e->player_view!=&campaign_player_view||e->player_body!=&scene_actor_body||
           e->objects!=&campaign_registry||e->entities!=&campaign_entities||
           e->resource!=s->driller||!e->resource||e->physics!=&r->physics||
           r->damage.entry!=e||e->registration.view!=&e->view||e->registration.object_kind!=0||
           e->view.type!=0||e->registration.handle!=e->host.handle||
           (uint32_t)e->view.handle!=e->host.handle||e->host.handle==handle||
           r->damage.damage.state.effects.handle!=e->host.handle||
           rf_object_registry_lookup(&campaign_registry,e->host.handle)!=&e->registration||
           rf_entity_lookup(&campaign_entities,(int32_t)e->host.handle)!=&e->view||
           e->session.active||r->player.saved||e->player.handle!=handle||
           e->player.host!=UINT32_MAX||e->player.control!=handle||e->host.driver==handle||
           e->occupant==(int32_t)handle||e->gunner_occupant!=-1||e->view.occupants!=e->occupants||
           e->view.occupant_count<1||e->view.occupant_count>2)return RF_NOT_FOUND;
        status=scene_vehicle_checkpoint_parked_owner((scene_stream*)s,0);if(status)return status;
        value.resource=e->resource;value.entry=e;value.physics=e->physics;
        value.registration=&e->registration;value.host_view=&e->view;value.host=e->host.handle;
    }
    *out=value;return RF_OK;
}
static int scene_npc_reload_player_seal(const scene_stream *s,const scene_npc_reload_player_receipt *before)
{
    scene_npc_reload_player_receipt current;int status;
    if(!before||before->epoch!=scene_robot_death_epoch||before->stream!=s||
       !s||before->runtime!=s->driller_runtime)return RF_FORMAT;
    status=scene_npc_reload_player_admit(s,&current);if(status)return status;
    return before->player!=current.player||before->host!=current.host||
        before->resource!=current.resource||before->entry!=current.entry||before->physics!=current.physics||
        before->registration!=current.registration||before->host_view!=current.host_view||
        before->spheres!=current.spheres||before->sphere_count!=current.sphere_count||
        before->body_bytes!=current.body_bytes||before->sphere_bytes!=current.sphere_bytes?RF_FORMAT:RF_OK;
}
static int scene_npc_reload_incoming_vehicle(const scene_vehicle_checkpoint_record *vehicle,
    const scene_npc_seat_checkpoint_stage *seats,uint32_t gunner,uint32_t turret)
{
    uint32_t i;
    if(gunner||turret||(vehicle&&vehicle->value.vehicle.player_occupied))return RF_NOT_FOUND;
    if(seats){
        if(seats->count>SCENE_NPC_SEAT_LIMIT)return RF_FORMAT;
        for(i=0;i<seats->count;i++)if(seats->rows[i].player_gunner)return RF_NOT_FOUND;
    }
    return RF_OK;
}
#endif
