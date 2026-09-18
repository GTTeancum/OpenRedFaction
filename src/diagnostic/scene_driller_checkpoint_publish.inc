#ifndef RF_SCENE_DRILLER_CHECKPOINT_PUBLISH_INC
#define RF_SCENE_DRILLER_CHECKPOINT_PUBLISH_INC
/* Include after live runtime and checkpoint adapter. Caller already validated
 * candidate terrain and full host/player placement before committing geometry.
 * This parked-only publication has no callbacks, allocations or registry edits
 * after staging succeeds. It retains class data, resources and fresh handles. */
static int scene_vehicle_checkpoint_parked_owner(scene_stream *s,uint32_t occupied)
{
    scene_driller_runtime *r;
    if(!s)return RF_RANGE;
    r=s->driller_runtime;
    if(!r||!scene_driller_damage_runtime_valid(&r->damage)||r->damage.entry!=&r->entry||
       r->entry.physics!=&r->physics)return RF_NOT_FOUND;
    if(occupied||r->entry.session.active||r->entry.host.driver!=UINT32_MAX||
       r->entry.occupant!=-1||r->player.saved||
       r->entry.player_view->linked_handle==(int32_t)r->entry.host.handle)return RF_RANGE;
    if(r->damage.damage.state.effects.burn)return RF_NOT_FOUND;
    return RF_OK;
}
/* Infallible common commit after owner and candidate admission. */
static void scene_vehicle_checkpoint_publish_staged(scene_stream *s,const scene_driller_checkpoint_candidate *prepared)
{
    scene_driller_checkpoint_candidate candidate=*prepared;scene_driller_runtime *r=s->driller_runtime;
    rf_entity_damage_state damage={0};rf_damage_effect_state *effects;
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
}
static int scene_driller_checkpoint_apply_parked(scene_stream *s,const rf_vehicle_checkpoint *record)
{
    scene_driller_checkpoint_candidate candidate;int status;
    if(!s||!record)return RF_RANGE;
    status=scene_vehicle_checkpoint_parked_owner(s,record->player_occupied);if(status)return status;
    status=scene_driller_checkpoint_stage(s,record,&candidate);if(status)return status;
    scene_vehicle_checkpoint_publish_staged(s,&candidate);return RF_OK;
}
/* Include scene_vehicle_combat_restore.inc before this file. Parent supplied
 * typed, validated stage; it may clear ONLY occupied for intermediate host
 * publication while retaining a gunner role/aim pending semantic possession.
 * This function always leaves the live host parked and Jeep driver-role0.
 * Caller reapplies final saved role/reference/aim after occupancy admission. */
typedef struct scene_vehicle_combat_publish_data {int32_t seat;float aim_basis[9];} scene_vehicle_combat_publish_data;
/* Read-only admission shared by precommit validation and actual publication. */
static int scene_vehicle_combat_parked_prepare(scene_stream *s,const scene_vehicle_combat_candidate *candidate,
    scene_vehicle_combat_publish_data *prepared)
{
    scene_driller_runtime *r;uint32_t i;int32_t seat=-1;float local[9],aim_basis[9];int status;
    if(!s||!candidate)return RF_RANGE;
    status=scene_vehicle_checkpoint_parked_owner(s,candidate->vehicle.player_occupied);if(status)return status;
    r=s->driller_runtime;
    if(!s->driller||r->entry.resource!=s->driller||r->entry.player_view!=&campaign_player_view||
       r->entry.player.handle!=campaign_player_object.handle||
       rf_entity_lookup(r->entry.entities,(int32_t)r->entry.player.handle)!=r->entry.player_view||
       r->entry.player_view->linked_handle!=-1||r->entry.player.host!=UINT32_MAX||
       r->entry.player.control!=r->entry.player.handle)return RF_NOT_FOUND;
    if((candidate->profile!=2&&candidate->profile!=3)||rf_scene_vehicle_enabled!=candidate->profile||
       strcmp(s->driller->model,candidate->profile==2?"APC.v3m":"Jeep01.v3m"))return RF_FORMAT;
    if(!isfinite(candidate->vehicle.health)||candidate->vehicle.health< -1000000||
       candidate->vehicle.health>(candidate->profile==2?5000.f:400.f)||candidate->vehicle.armor!=0||
       candidate->vehicle.alive>1||candidate->vehicle.alive!=(uint32_t)(candidate->vehicle.health>0)||
       candidate->vehicle.drill_spin!=0||candidate->vehicle.accepted_drill_cuts||
       candidate->primary_reserve>999||candidate->secondary_reserve>(candidate->profile==2?15u:0u)||
       candidate->role>(candidate->profile==3?1u:0u)||
       memcmp(candidate->vehicle.physics.inverse_inertia,r->physics.state.inverse_inertia,36))return RF_FORMAT;
    if(!isfinite(candidate->aim_pitch)||!isfinite(candidate->aim_yaw))return RF_FORMAT;
    if(candidate->profile==2){
        if(candidate->aim_pitch< -20*.01745329238474369f||candidate->aim_pitch>50*.01745329238474369f||candidate->aim_yaw!=0)return RF_FORMAT;
    }else if(fabsf(candidate->aim_pitch)>1.0471975512f||fabsf(candidate->aim_yaw)>3.1415926536f||
        (!candidate->role&&(candidate->aim_pitch!=0||candidate->aim_yaw!=0)))return RF_FORMAT;
    for(i=0;i<SCENE_APC_PRIMARY_CAPACITY;i++)if(s->apc_primary.rounds[i].flight.active)return RF_NOT_FOUND;
    for(i=0;i<SCENE_APC_SECONDARY_CAPACITY;i++)if(s->apc_secondary.rounds[i].flight.active)return RF_NOT_FOUND;
    if(s->apc_primary.scheduler.held||s->apc_primary.scheduler.cooldown>0||s->apc_primary.scheduler.warmup>0||
       s->apc_secondary.cooldown>0||r->damage.damage.destruction_pending)return RF_NOT_FOUND;
    for(i=0;i<s->driller->tags.count;i++)if(!strcmp(s->driller->tags.items[i].name,"interface_1")){seat=(int32_t)i;break;}
    if(seat<0)return RF_NOT_FOUND;
    {float angles[3]={candidate->aim_pitch,candidate->aim_yaw,0};uint32_t row,column,k;
     status=rf_look_orientation(angles,local);if(status)return status;
     for(row=0;row<3;row++)for(column=0;column<3;column++){
         double value=0;for(k=0;k<3;k++)value+=(double)local[row*3+k]*candidate->aim_reference[k*3+column];
         aim_basis[row*3+column]=(float)value;if(!isfinite(aim_basis[row*3+column]))return RF_FORMAT;
     }}
    if(prepared){prepared->seat=seat;memcpy(prepared->aim_basis,aim_basis,36);}
    return RF_OK;
}
static int scene_vehicle_combat_parked_admit(scene_stream *s,const scene_vehicle_combat_candidate *candidate)
{return scene_vehicle_combat_parked_prepare(s,candidate,NULL);}
static int scene_vehicle_combat_apply_parked(scene_stream *s,const scene_vehicle_combat_candidate *candidate)
{
    scene_vehicle_combat_publish_data prepared;scene_driller_runtime *r;int status;
    status=scene_vehicle_combat_parked_admit(s,candidate);if(status)return status;
    /* Repeat the small pure resolution to retain exact seat/matrix results;
     * no callbacks or mutable providers occur between these checks. */
    status=scene_vehicle_combat_parked_prepare(s,candidate,&prepared);if(status)return status;
    r=s->driller_runtime;
    /* No fallible operations below publication. Definitions, muzzle tags,
     * renderer resources and authored aim limits remain the current class. */
    scene_vehicle_checkpoint_publish_staged(s,&candidate->vehicle);
    memset(&s->apc_primary.scheduler,0,sizeof(s->apc_primary.scheduler));
    memset(s->apc_primary.rounds,0,sizeof(s->apc_primary.rounds));
    s->apc_primary.reserve=(int32_t)candidate->primary_reserve;s->apc_primary.random.value=candidate->primary_random;
    memset(s->apc_secondary.rounds,0,sizeof(s->apc_secondary.rounds));s->apc_secondary.cooldown=0;
    s->apc_secondary.reserve=(int32_t)candidate->secondary_reserve;
    s->apc_aim.angles[0]=candidate->aim_pitch;s->apc_aim.angles[1]=candidate->aim_yaw;s->apc_aim.angles[2]=0;
    memcpy(s->apc_aim_basis,prepared.aim_basis,36);memset(s->apc_aim_eye,0,sizeof(s->apc_aim_eye));s->apc_aim_active=0;
    s->driller->seat=prepared.seat;r->jeep_seat.role=0;r->jeep_seat.held=1;
    return RF_OK;
}
#endif
