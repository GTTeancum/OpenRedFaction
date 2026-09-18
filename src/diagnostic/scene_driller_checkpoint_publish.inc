#ifndef RF_SCENE_DRILLER_CHECKPOINT_PUBLISH_INC
#define RF_SCENE_DRILLER_CHECKPOINT_PUBLISH_INC
/* Include after live runtime and checkpoint adapter. Caller already validated
 * candidate terrain and full host/player placement before committing geometry.
 * This parked-only publication has no callbacks, allocations or registry edits
 * after staging succeeds. It retains class data, resources and fresh handles. */
static int scene_driller_checkpoint_apply_parked(scene_stream *s,const rf_vehicle_checkpoint *record)
{
    scene_driller_checkpoint_candidate candidate;scene_driller_runtime *r;
    rf_entity_damage_state damage={0};rf_damage_effect_state *effects;int status;
    if(!s||!record)return RF_RANGE;
    r=s->driller_runtime;
    if(!r||!scene_driller_damage_runtime_valid(&r->damage)||r->damage.entry!=&r->entry||
       r->entry.physics!=&r->physics)return RF_NOT_FOUND;
    if(record->player_occupied||r->entry.session.active||r->entry.host.driver!=UINT32_MAX||
       r->entry.occupant!=-1||r->player.saved||
       r->entry.player_view->linked_handle==(int32_t)r->entry.host.handle)return RF_RANGE;
    /* No active burn owner may be silently orphaned by a parked restore. */
    if(r->damage.damage.state.effects.burn)return RF_NOT_FOUND;
    status=scene_driller_checkpoint_stage(s,record,&candidate);if(status)return status;
    effects=&r->damage.damage.state.effects;
    damage.effects.class_health=effects->class_health;damage.effects.class_armor=effects->class_armor;
    damage.effects.class_flags_728=effects->class_flags_728;damage.effects.handle=effects->handle;
    damage.effects.affiliation=effects->affiliation;damage.effects.voice=UINT32_MAX;
    damage.effects.health=candidate.health;damage.effects.armor=candidate.armor;
    damage.responsible_handle=damage.burn_source=UINT32_MAX;
    /* Inertia and coefficients are current authored metadata; stage reconstructs
     * angular momentum and zeros old forces/torques/skip-forces. */
    r->physics.state=candidate.physics;r->damage.damage.state=damage;
    r->damage.damage.destroyed=!candidate.alive;r->damage.damage.destruction_pending=0;
    r->damage.damage.object_flags&=~0x200000u; /* Unsaved hit-event signal. */
    r->entry.view.flags_7c=r->damage.damage.object_flags;r->entry.view.flags_810=0;
    r->entry.host.alive=candidate.alive;
    r->damage.eject_pending=0;r->damage.last_destroyer=UINT32_MAX;r->damage.last_amount=0;
    r->damage.damage_calls=0;r->damage.exit_count=0;
    r->entry.held=1; /* Require release before another Use edge after restore. */
    s->driller_weapon.held_seconds=0;s->driller_weapon.damage_wait=0;s->driller_weapon.contact_seconds=0;
    s->driller_weapon.held=0;s->driller_weapon.contact_active=0;s->driller_weapon.phase=0;
    s->driller_weapon.spin=candidate.drill_spin;s->driller_weapon.accepted_cuts=candidate.accepted_drill_cuts;
    s->driller_weapon.contacts=0;s->driller_weapon.damage_calls=0;
    r->support.material=-1;r->support.support_handle=UINT32_MAX;r->support.hits=0;
    memset(r->support.velocity,0,sizeof(r->support.velocity));
    memset(&r->contact.world_contact,0,sizeof(r->contact.world_contact));r->contact.has_world=0;
    r->contact.stream=s;r->contact.source=r->entry.host.handle;r->contact.frame=0;
    r->last_frame=UINT32_MAX;r->driver_seconds=0;
    memcpy(s->driller_position,candidate.physics.position,12);memcpy(s->driller_basis,candidate.physics.orientation,36);
    memset(rf_scene_vehicle_state,0,sizeof(rf_scene_vehicle_state));
    memcpy(rf_scene_vehicle_state+6,candidate.physics.position,12);memcpy(rf_scene_vehicle_state+9,candidate.physics.velocity,12);
    rf_scene_vehicle_state[12]=r->entry.registration.handle;
    memset(rf_scene_vehicle_damage,0,sizeof(rf_scene_vehicle_damage));
    memcpy(rf_scene_vehicle_damage,&candidate.health,4);rf_scene_vehicle_damage[3]=!candidate.alive;
    rf_scene_vehicle_damage[6]=UINT32_MAX;rf_scene_vehicle_damage[7]=candidate.alive;
    memset(rf_scene_drill_state,0,sizeof(rf_scene_drill_state));rf_scene_drill_state[3]=candidate.accepted_drill_cuts;
    return RF_OK;
}
#endif
