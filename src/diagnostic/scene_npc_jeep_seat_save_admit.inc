#ifndef RF_SCENE_NPC_JEEP_SEAT_SAVE_ADMIT_INCLUDED
#define RF_SCENE_NPC_JEEP_SEAT_SAVE_ADMIT_INCLUDED
/* Define after scene_npc_seat_checkpoint.inc; forward declare before world vehicle restore.
 * A detached settled driver corpse may still intersect its former Jeep seat.
 * This proves only that pair contact from pending saved state. It never binds
 * the actor, changes the RFNS active bit, or restarts the retained RFVR route. */
static uint32_t scene_npc_jeep_seat_save_dead_contact(
    const scene_npc_seat_checkpoint_stage *seats,
    const scene_npc_checkpoint_restore_stage *npcs,
    const scene_vehicle_checkpoint_record *vehicle,uint32_t host_uid,uint32_t npc_index)
{
    const scene_npc_checkpoint_restore_entry *npc;
    const scene_npc_seat_saved *seat;
    scene_npc_seat_binding *binding=NULL;
    scene_npc_seat_host host;
    float pose[12];uint32_t i;
    if(!seats||!npcs||npc_index>=npcs->count||!vehicle||vehicle->profile!=3||
       !host_uid||host_uid!=(uint32_t)campaign_authored_vehicle_uid)return 0;
    npc=npcs->entries+npc_index;
    seat=scene_npc_seat_checkpoint_find(seats,npc->saved.uid);
    /* Retired/hidden actors already skip pair collision. Live detached actors
     * must pass ordinary clearance; no blanket former-seat exception. */
    if(!seat||seat->active||seat->owns_host_flag||seat->binding.kind!=SCENE_NPC_SEAT_JEEP||
       seat->binding.host_uid!=host_uid||seat->binding.host!=campaign_authored_vehicle_handle||
       seat->binding.slot!=npc->slot||seat->binding.actor!=npc->handle||
       npc->saved.retired||npc->saved.dead_pose!=1||!isfinite(npc->saved.health)||npc->saved.health>0||
       (npc->saved.flags&(2u|0x4000u))||npc->saved.ai_mode==13||
       npc->saved.move.active||npc->saved.combat.active||npc->saved.shot_count||
       npc->saved.script_animation.active||npc->saved.support_uid||npc->support.handle||
       npc->support.material!=-1)return 0;
    /* Resolve immutable seat tags through the canonical generation-checked
     * fresh owner. Its live health/occupancy never substitutes for saved death. */
    for(i=0;i<scene_npc_seat_count;i++) {
        scene_npc_seat_binding *b=scene_npc_seats+i;
        if(b->actor==seat->binding.actor&&b->host==seat->binding.host&&
           b->host_uid==host_uid&&b->kind==SCENE_NPC_SEAT_JEEP&&b->tag==seat->binding.tag) {
            if(binding)return 0;binding=b;
        }
    }
    if(!binding||!scene_npc_seat_actor(binding)||scene_npc_seat_host_read(binding,&host)||
       rf_static_model_tag_place(host.tags,binding->tag,vehicle->value.jeep.vehicle.orientation,
           vehicle->value.jeep.vehicle.position,pose))return 0;
    /* Only the retained seat pose qualifies. A body displaced by a blast or
     * left behind by a moving host must pass normal vehicle/body clearance. */
    for(i=0;i<3;i++)if(!isfinite(pose[9+i])||!isfinite(npc->saved.position[i])||
        !isfinite(npc->published[i])||fabsf(npc->saved.position[i]-pose[9+i])>.001f||
        fabsf(npc->published[i]-pose[9+i])>.001f)return 0;
    for(i=0;i<9;i++)if(!isfinite(pose[i])||!isfinite(npc->basis[i])||
        fabsf(npc->basis[i]-pose[i])>.0001f)return 0;
    return 1;
}
#endif
