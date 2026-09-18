#ifndef RF_SCENE_DRILLER_CHECKPOINT_OCCUPANCY_INC
#define RF_SCENE_DRILLER_CHECKPOINT_OCCUPANCY_INC
/* Include after live runtime and checkpoint adapter. This is ONLY semantic
 * occupancy publication, not save admission or a terrain/placement validator.
 * Parent first restores matching host/player data and validates candidate
 * geometry, full host volume, and seated player volume. Before calling, restore
 * player inventory/control flags and ordinary exit-look metadata: normal player
 * publication captures actor_look for eventual exit before applying the seat
 * camera. Do not supply disk handles or restore the old possession structure.
 */
static int scene_driller_checkpoint_seat_admit(void *context,uint32_t player,uint32_t host,uint32_t *allowed)
{
    scene_driller_entry_exit *entry=context;
    if(!entry||!allowed)return RF_RANGE;
    *allowed=0;
    if(entry->registration.handle!=host||entry->player.handle!=player||
       rf_entity_lookup(entry->entities,(int32_t)host)!=&entry->view||
       rf_entity_lookup(entry->entities,(int32_t)player)!=entry->player_view)return RF_NOT_FOUND;
    /* Geometry and matching-record admission belong to the parent restore
     * transaction. No Use edge or distance from a former position is checked. */
    *allowed=entry->host.alive&&campaign_player_damage.state.effects.health>0;
    return RF_OK;
}
static int scene_driller_checkpoint_restore_occupancy_validated(scene_stream *stream,const rf_vehicle_checkpoint *record)
{
    scene_driller_runtime *runtime;scene_driller_entry_exit *entry;
    scene_vehicle_possession next_session={0},old_session;
    scene_vehicle_player_control next_player,old_player;
    scene_vehicle_host_control next_host,old_host;
    scene_driller_player_adapter old_adapter;
    scene_vehicle_possession_ops ops;scene_vehicle_pose seat;
    int32_t old_linked,old_occupant;uint32_t entered=0,old_held,i;float distance=0;int status;
    if(!stream||!record)return RF_RANGE;
    if(!record->player_occupied||!record->alive)return RF_RANGE;
    runtime=stream->driller_runtime;
    if(!runtime||!scene_driller_damage_runtime_valid(&runtime->damage)||
       runtime->damage.entry!=&runtime->entry||runtime->entry.physics!=&runtime->physics)return RF_NOT_FOUND;
    entry=&runtime->entry;
    if(!entry->host.alive||runtime->damage.damage.destroyed||runtime->damage.damage.state.effects.health<=0||
       campaign_player_damage.state.effects.health<=0||entry->session.active||runtime->player.saved||
       entry->host.driver!=UINT32_MAX||entry->occupant!=-1||entry->player.host!=UINT32_MAX||
       entry->player_view->linked_handle!=-1)return RF_RANGE;
    if(entry->host.handle!=entry->registration.handle||entry->player.handle!=campaign_player_object.handle||
       entry->player_view!=&campaign_player_view||
       rf_entity_lookup(entry->entities,(int32_t)entry->player.handle)!=entry->player_view)return RF_NOT_FOUND;
    /* Host pose must be the staged record already committed by the parent. */
    if(memcmp(runtime->physics.state.position,record->position,12)||
       memcmp(runtime->physics.state.orientation,record->orientation,36)||
       runtime->damage.damage.state.effects.health!=record->health||
       runtime->damage.damage.state.effects.armor!=record->armor)return RF_FORMAT;
    status=scene_driller_entry_seat(entry,entry->host.handle,&seat);if(status)return status;
    for(i=0;i<3;i++){
        float d=scene_actor_body.state.position[i]-seat.position[i];
        if(!isfinite(d))return RF_FORMAT;
        distance+=d*d;
    }
    /* Matching seated checkpoint body, with1cm serialization tolerance. This
     * is not a teleport from an arbitrary old standing player position. */
    if(!isfinite(distance)||distance>.0001f)return RF_FORMAT;
    next_player=entry->player;next_host=entry->host;
    next_player.handle=campaign_player_object.handle;next_player.host=UINT32_MAX;
    next_player.control=campaign_player_object.handle;
    next_player.weapon=entry->player_view->weapons[0];next_player.control_flags=entry->player_view->flags_810;
    memcpy(next_player.pose.position,scene_actor_body.state.position,12);
    /* session.saved is the ordinary exit control state, not the potentially
     * banked seat basis that possess() is about to install. */
    memcpy(next_player.pose.basis,actor_look.body_orientation,36);
    if(!scene_vehicle_pose_valid(&next_player.pose))return RF_FORMAT;
    ops=(scene_vehicle_possession_ops){scene_driller_checkpoint_seat_admit,scene_driller_entry_seat,scene_driller_exit_pose,entry};
    status=scene_vehicle_possess(&next_session,&next_player,&next_host,1,&ops,&entered);if(status)return status;
    if(!entered)return RF_NOT_FOUND;
    old_session=entry->session;old_player=entry->player;old_host=entry->host;old_adapter=runtime->player;
    old_linked=entry->player_view->linked_handle;old_occupant=entry->occupant;old_held=entry->held;
    entry->session=next_session;entry->player=next_player;entry->host=next_host;
    entry->player_view->linked_handle=(int32_t)entry->host.handle;entry->occupant=(int32_t)entry->player.handle;
    entry->held=1; /* A held Use button must not instantly undo the restored seat. */
    status=scene_driller_player_publish(&runtime->player,entry,1);
    if(status){
        /* Normal publish validates body/group work before global publication;
         * its earlier seat-pose mutation is included in this rollback. */
        entry->session=old_session;entry->player=old_player;entry->host=old_host;runtime->player=old_adapter;
        entry->player_view->linked_handle=old_linked;entry->occupant=old_occupant;entry->held=old_held;
        return status;
    }
    runtime->damage.eject_pending=0;runtime->last_frame=UINT32_MAX;
    rf_scene_vehicle_state[3]=1;
    return RF_OK;
}
static int scene_driller_checkpoint_restore_occupancy(scene_stream *s,const rf_vehicle_checkpoint *r)
{
    int status=rf_vehicle_checkpoint_validate(r);if(status)return status;
    return scene_driller_checkpoint_restore_occupancy_validated(s,r);
}
#endif
