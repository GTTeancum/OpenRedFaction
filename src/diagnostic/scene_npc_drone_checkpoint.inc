#ifndef RF_SCENE_NPC_DRONE_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_DRONE_CHECKPOINT_INCLUDED
/* RFNC19 owns ordinary Drone continuation, never Turn_Off_Physics history.
 * Original42b7a0/42be4e retain body864 angles;48ad10/48ae50 retain orientation.
 * This bounded profile represents only the canonical fly11 ordinary branch.
 * All helpers either inspect live state or modify a private restore candidate. */
static uint32_t scene_npc_drone_checkpoint_identity(uint32_t index)
{
    uint32_t cls;
    if(!campaign_seeds.items||!campaign_seeds.records.items||!campaign_seeds.classes||
       index>=campaign_npc_body_count||index>=campaign_seeds.records.count)return 0;
    cls=campaign_seeds.items[index].class_index;
    return cls<campaign_seeds.class_count&&
        !strcmp(campaign_seeds.records.items[index].record.class_name,"Drone")&&
        campaign_seeds.classes[cls].physics.movement_index==11&&
        campaign_seeds.classes[cls].physics.use_kind==0&&
        campaign_seeds.classes[cls].physics.flags==0x12u;
}
static uint32_t scene_npc_drone_checkpoint_mode(uint32_t slot)
{
    return slot<16&&(campaign_modes[slot].enabled&255u)&&campaign_modes[slot].index==11&&
        campaign_modes[slot].rotation[0]==2&&campaign_modes[slot].rotation[1]==3&&
        campaign_modes[slot].rotation[2]==0;
}
static uint32_t scene_npc_drone_checkpoint_ordinary(uint32_t index,const campaign_npc_body *o)
{
    return scene_npc_drone_checkpoint_identity(index)&&
        scene_npc_drone_checkpoint_mode(o->movement_slot)&&
        !o->scripted_physics&&!o->scripted_physics_seen&&
        o->view.linked_handle==-1&&!o->view.occupant_count&&o->attachment_75c==-1&&
        o->ai_mode.action_280!=13;
}
/* Outside the retained event/support mask, this exact fly11 body must still
 * have constructor/class/sphere flags. In particular force-air-cap200000 and
 * contact/pending/special branches are not silently omitted or inherited. */
static uint32_t scene_npc_drone_checkpoint_body_flags(uint32_t index,const campaign_npc_body *o)
{
    const rf_entity_class_physics *p=&campaign_seeds.classes[campaign_seeds.items[index].class_index].physics;
    uint32_t i,flags=rf_entity_creation_physics_flags(campaign_seeds.items[index].spawn.creation_flags,
        p->flags,p->flags2,p->use_kind,0);
    for(i=0;i<o->body.spheres.count;i++)if(o->body.spheres.items[i].parameter_10>0)flags|=0x2000u;
    /* rf_movement_start changes low flags only for turret10, never fly11. */
    return flags&~RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK;
}
static uint32_t scene_npc_drone_checkpoint_basis_equal(const float *a,const float *b)
{
    uint32_t i;
    /* The constructor retains binary32 authored bases, then extracts yaw.
     * Reconstructing that yaw can differ by a final rounding. This tolerance
     * is componentwise only; it never substitutes for the exact roll/eye gates. */
    for(i=0;i<9;i++)if(!isfinite(a[i])||!isfinite(b[i])||fabsf(a[i]-b[i])>1e-6f)return 0;
    return 1;
}
static int scene_npc_drone_checkpoint_admit(uint32_t index,const campaign_npc_body *o,
    const rf_npc_checkpoint_record *r)
{
    uint32_t i,cls;float angles[3]={r->drone.pitch,r->yaw,0},basis[9];
    if(!r->drone.kind)return RF_OK;
    if(!scene_npc_drone_checkpoint_identity(index)||r->physics.present||r->dead_pose||
       r->ai_mode==13||r->support_uid||r->capek_shield_broken||
       o->view.linked_handle!=-1||o->view.occupant_count||o->attachment_75c!=-1||
       (r->drone.kind==1?(!r->movement_present||!scene_npc_drone_checkpoint_mode(r->movement_slot)):
        !scene_npc_drone_checkpoint_mode(o->movement_slot)))return RF_FORMAT;
    if(r->drone.kind==1&&(r->drone.body_bits&(1u|0x400000u)))return RF_FORMAT;
    cls=campaign_seeds.items[index].class_index;
    /* These limits are constructor-only class data. Verify the resident bank
     * too: publication does not carry an independent eye envelope. */
    for(i=0;i<3;i++)if(r->eye_angles[i]!=0||r->look_command[i]!=0||r->look_delta[i]!=0||
        r->look_offset[i]!=0||r->support_velocity[i]!=0||
        o->look.angles.minimum_1434[i]!=campaign_seeds.classes[cls].eye_limits.minimum[i]||
        o->look.angles.maximum_1440[i]!=campaign_seeds.classes[cls].eye_limits.maximum[i])return RF_FORMAT;
    /* Retired publication intentionally leaves support/movement banks inert.
     * Require the same recapture invariants rather than revive/reset owners. */
    if(r->drone.kind==2){
        if(o->support.handle||o->movement_orientation!=campaign_identity[0])return RF_FORMAT;
        for(i=0;i<3;i++)if(o->support_velocity[i]!=0)return RF_FORMAT;
    }
    if(r->look_vector[2]!=0)return RF_FORMAT;
    return rf_look_orientation(angles,basis); /* Bounds pitch/yaw before any publication. */
}
static int scene_npc_drone_checkpoint_capture(uint32_t index,const campaign_npc_body *o,
    rf_npc_checkpoint_record *r)
{
    rf_npc_checkpoint_drone *p=&r->drone;const rf_physics_body_state *body=&o->body.state;
    uint32_t i,cls;float basis[9];int status;
    if(r->dead_pose||!scene_npc_drone_checkpoint_ordinary(index,o))return RF_OK;
    cls=campaign_seeds.items[index].class_index;
    if(o->look.body_angles[2]!=0||o->support.handle||
       o->movement_orientation!=campaign_identity[0])return RF_RANGE;
    for(i=0;i<3;i++)if(o->support_velocity[i]!=0||o->look.angles.minimum_1434[i]!=campaign_seeds.classes[cls].eye_limits.minimum[i]||
        o->look.angles.maximum_1440[i]!=campaign_seeds.classes[cls].eye_limits.maximum[i])return RF_RANGE;
    status=rf_look_orientation(o->look.body_angles,basis);if(status)return status;
    if(!scene_npc_drone_checkpoint_basis_equal(o->look.orientation,basis))return RF_RANGE;
    p->kind=r->retired?2u:1u;p->pitch=o->look.body_angles[0];
    if(r->retired){
        /* Removal closes the body and leaves retained look state. The model
         * bank is non-executable and can contain an old authored basis after
         * fresh load: never consult or revive it for tombstone recapture. */
        if(o->registration.view||o->body.allocated_bytes||o->body.spheres.count||
           o->body.spheres.items||o->body.spheres.allocated_bytes)return RF_RANGE;
    }else{
        if(!o->registration.view||!o->body.allocated_bytes||
           !campaign_model_owners||index>=campaign_model_owner_count||campaign_model_owners[index].owned||
           body->word_164||body->word_168||body->state_124||body->reference_15c!=-1||
           body->scalar_144!=1||(body->flags&(1u|0x400000u))||
           (body->flags&~RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK)!=scene_npc_drone_checkpoint_body_flags(index,o)||
           body->vector_138[0]!=0||body->vector_138[1]!=1||body->vector_138[2]!=0||
           !scene_npc_drone_checkpoint_basis_equal(body->orientation,basis)||
           !scene_npc_drone_checkpoint_basis_equal(body->next_orientation,basis)||
           !scene_npc_drone_checkpoint_basis_equal(campaign_model_owners[index].basis,basis))return RF_RANGE;
        for(i=0;i<3;i++)if(!isfinite(body->position[i])||body->next_position[i]!=body->position[i]||
           o->published[i]!=body->position[i]||campaign_model_owners[index].position[i]!=body->position[i])return RF_RANGE;
        p->body_bits=body->flags&RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK;
        p->object_bits=o->object_flags&RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK;
        memcpy(p->velocity,body->velocity,12);memcpy(p->angular,body->vector_c8,12);
        memcpy(p->momentum,body->mass_vector_d4,12);memcpy(p->force,body->vector_e0,12);
        memcpy(p->torque,body->vector_ec,12);
    }
    /* RFNC7 already owns the real command/delta/offset/vector. Validate this
     * profile's canonical zeros rather than erase an unsupported eye state.
     * The codec separately validates every derivative, including quiet linear
     * velocity; pitch0 still retains angular/torque continuation here. */
    return scene_npc_drone_checkpoint_admit(index,o,r);
}
static void scene_npc_drone_checkpoint_prepare(uint32_t index,const campaign_npc_body *o,
    const rf_npc_checkpoint_record *r,
    rf_physics_body_state *body,uint32_t *flags)
{
    const rf_npc_checkpoint_drone *p=&r->drone;
    if(p->kind!=1)return;
    body->flags=scene_npc_drone_checkpoint_body_flags(index,o)|p->body_bits;
    *flags=(*flags&~RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK)|p->object_bits;
    memcpy(body->velocity,p->velocity,12);memcpy(body->vector_c8,p->angular,12);
    memcpy(body->mass_vector_d4,p->momentum,12);memcpy(body->vector_e0,p->force,12);
    memcpy(body->vector_ec,p->torque,12);
    /* These omitted scratch fields have exact canonical capture requirements;
     * a later resident script/contact cannot leak them into the candidate. */
    body->state_124=body->word_164=body->word_168=0;body->reference_15c=-1;body->scalar_144=1;
    body->vector_138[0]=body->vector_138[2]=0;body->vector_138[1]=1;
}
static uint32_t scene_npc_drone_checkpoint_no_support(uint32_t index,const rf_npc_checkpoint_record *r)
{
    return r->drone.kind==1&&!r->retired&&!r->dead_pose&&r->health>0&&
        !r->physics.present&&!r->support_uid&&!(r->drone.body_bits&(1u|0x400000u))&&
        r->movement_present&&scene_npc_drone_checkpoint_mode(r->movement_slot)&&
        scene_npc_drone_checkpoint_identity(index)&&!rf_entity_falling(11,0,-1);
}
#endif
