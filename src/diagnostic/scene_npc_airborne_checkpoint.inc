#ifndef RF_SCENE_NPC_AIRBORNE_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_AIRBORNE_CHECKPOINT_INCLUDED
/* RFNC26: exact ordinary guard1/use0 committed fall3. This owner is distinct
 * from script-physics, Drone, flight and route state. No helper runs physics,
 * animation, a support query, an event or a callback. */
static uint32_t scene_npc_airborne_name(const char *a,const char *b)
{
    uint32_t i;if(!a||!b)return 0;
    for(i=0;i<64;i++){unsigned char x=(unsigned char)a[i],y=(unsigned char)b[i];
        if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;if(!x)return i!=0;}return 0;
}
static uint32_t scene_npc_airborne_class(uint32_t index)
{
    uint32_t cls;const rf_entity_seed_class *c;
    if(!campaign_seeds.items||!campaign_seeds.records.items||!campaign_seeds.classes||
       index>=campaign_seeds.records.count||index>=campaign_npc_body_count)return 0;
    cls=campaign_seeds.items[index].class_index;if(cls>=campaign_seeds.class_count)return 0;c=campaign_seeds.classes+cls;
    return scene_npc_airborne_name(campaign_seeds.records.items[index].record.class_name,"guard1")&&
        scene_npc_airborne_name(c->model,"ult2_guard.vcm")&&c->model_kind==2&&
        c->physics.flags==0x20019u&&c->physics.flags2==1&&c->physics.use_kind==0&&
        c->physics.movement_index==1&&c->physics.mass==100&&
        scene_npc_airborne_name(c->physics.material,"flesh")&&!campaign_seeds.items[index].spawn.creation_flags;
}
static uint32_t scene_npc_airborne_mode(void)
{
    const rf_movement_descriptor *m=campaign_modes+3;
    return (m->enabled&255u)&&m->index==3&&m->translation[0]==2&&m->translation[1]==0&&
        m->translation[2]==2&&m->rotation[0]==1&&m->rotation[1]==3&&m->rotation[2]==0;
}
/* A potential ordinary owner cannot fall through to the legacy quiet lane if
 * its exact geometry/endpoint proof later fails. Existing typed owners retain
 * their separate admission and do not acquire this presence. */
static uint32_t scene_npc_airborne_potential(uint32_t index,const campaign_npc_body *o)
{
    return o&&scene_npc_airborne_class(index)&&o->registration.view&&
        isfinite(o->damage.effects.health)&&o->damage.effects.health>0&&o->movement_slot==3&&
        !o->scripted_physics&&!o->scripted_physics_seen&&!o->flight.qualified&&!o->flight.entered;
}
static int scene_npc_airborne_identity(uint32_t index,const campaign_npc_body *o)
{
    const campaign_model_owner *m;const rf_entity_pose *p;uint32_t h,cls;
    if(!campaign_npc_bodies||index>=campaign_npc_body_count||o!=campaign_npc_bodies+index||
       !scene_npc_airborne_class(index)||!campaign_model_owners||index>=campaign_model_owner_count||
       !campaign_poses.items||index>=campaign_poses.count)return RF_FORMAT;
    h=o->registration.handle;m=campaign_model_owners+index;p=campaign_poses.items+index;
    cls=campaign_seeds.items[index].class_index;
    if(!h||o->registration.object_kind!=0||o->registration.view!=&o->view||o->view.type!=0||
       (uint32_t)o->view.handle!=h||o->damage.effects.handle!=h||
       rf_object_registry_lookup(&campaign_registry,h)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)h)!=&o->view||
       m->owned||m->pose!=p||!p->bone_count||!p->matrices||!p->generations||
       p->skeleton>=campaign_skeletons.count||!campaign_skeletons.items||
       !campaign_skeletons.class_indices||cls>=campaign_skeletons.class_count||
       campaign_skeletons.class_indices[cls]!=p->skeleton||
       p->bone_count!=campaign_skeletons.items[p->skeleton].count||!campaign_skeletons.items[p->skeleton].bones||
       !scene_npc_airborne_name(campaign_skeletons.items[p->skeleton].model,"ult2_guard.v3c")||
       !o->persistence_registered||rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS||
       o->persistence_slot>=rf_scene_defeated_actors.count||o->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS||rf_scene_defeated_actors.items[o->persistence_slot].retired||
       rf_scene_defeated_actors.items[o->persistence_slot].uid!=(uint32_t)campaign_seeds.records.items[index].record.uid||
       !isfinite(o->damage.effects.health)||!(o->damage.effects.health>0)||
       (o->view.flags_810&1u)||(o->damage.effects.flags_810&1u)||o->death.action_824!=-1||
       o->view.linked_handle!=-1||o->view.occupant_count||o->attachment_75c!=-1||
       scene_npc_seat_attached(h)||o->scripted_physics||o->scripted_physics_seen||
       o->flight.qualified||o->flight.entered||o->robot_death.state)return RF_FORMAT;
    return RF_OK;
}
static uint32_t scene_npc_airborne_basis_equal(const float *a,const float *b)
{
    uint32_t i;for(i=0;i<9;i++)if(!isfinite(a[i])||!isfinite(b[i])||fabsf(a[i]-b[i])>1e-6f)return 0;return 1;
}
/* Verify the existing immutable loaded class caches. No posed triangle query
 * or fresh pose evaluation supplies physical standing centers. */
static int scene_npc_airborne_geometry(uint32_t index,const campaign_npc_body *o)
{
    uint32_t cls,i,j,flags;float tensor[9];rf_physics_bounds bounds;rf_movement_settings speed;
    const rf_physics_stance_cache *stance;const scene_npc_hit_location_class *hit;
    const rf_entity_material *material;const rf_physics_body_state *b=&o->body.state;int status;
    status=scene_npc_airborne_identity(index,o);if(status)return status;
    cls=campaign_seeds.items[index].class_index;
    if(!campaign_npc_stances||!scene_npc_hit_location_classes||!campaign_npc_movement_configs||
       !campaign_surface_palette||campaign_surface_palette->count<=3)return RF_FORMAT;
    stance=campaign_npc_stances+cls;hit=scene_npc_hit_location_classes+cls;material=campaign_surface_palette->materials+3;
    if(stance->count!=3||!hit->enabled||hit->count!=3||hit->skeleton!=campaign_poses.items[index].skeleton||
       o->body.spheres.count!=3||!o->body.spheres.items||
       o->body.allocated_bytes!=sizeof(o->body)+3u*sizeof(*o->body.spheres.items)||
       o->body.spheres.allocated_bytes!=sizeof(o->body.spheres)+3u*sizeof(*o->body.spheres.items)||
       material->index!=3||!isfinite(material->elasticity)||!isfinite(material->friction)||
       o->collision_material!=3||b->mass!=campaign_seeds.classes[cls].physics.mass||
       !isfinite(b->mass)||!(b->mass>0)||b->coefficients[0]!=material->elasticity||b->coefficients[2]!=material->friction||
       o->movement.mode<0||o->movement.mode>2)return RF_FORMAT;
    for(i=0;i<3;i++){
        const rf_physics_sphere *sphere=o->body.spheres.items+i;
        if(hit->spheres[i].index!=i||!isfinite(hit->spheres[i].radius)||!(hit->spheres[i].radius>0)||
           sphere->radius!=hit->spheres[i].radius||sphere->parameter_10!=-1||sphere->opaque_14)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(sphere->center[j])||!isfinite(stance->centers[0][i][j])||
           sphere->center[j]!=stance->centers[0][i][j])return RF_FORMAT;
    }
    flags=rf_entity_creation_physics_flags(campaign_seeds.items[index].spawn.creation_flags,
        campaign_seeds.classes[cls].physics.flags,campaign_seeds.classes[cls].physics.flags2,
        campaign_seeds.classes[cls].physics.use_kind,0);
    if(flags!=0x80000078u)return RF_FORMAT;
    speed=o->movement;status=scene_npc_resolve_speed(o,cls,0,o->movement.mode,-1,&speed);if(status)return status;
    if(!isfinite(o->movement.response)||!isfinite(o->movement.speed)||o->movement.response!=speed.response||
       o->movement.speed!=speed.speed||b->coefficients[1]!=speed.response)return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(b->local_tensor[i])||b->local_tensor[i]!=0||!isfinite(b->orientation[i]))return RF_FORMAT;
    status=rf_physics_tensor_world(b->local_tensor,b->orientation,tensor);if(status)return status;
    for(i=0;i<9;i++)if(!isfinite(b->world_tensor[i])||b->world_tensor[i]!=tensor[i])return RF_FORMAT;
    status=rf_physics_spheres_bounds(o->body.spheres.items,3,b->position,&bounds);if(status)return status;
    if(b->bounds.radius!=bounds.radius)return RF_FORMAT;
    for(i=0;i<3;i++)if(b->bounds.minimum[i]!=bounds.minimum[i]||b->bounds.maximum[i]!=bounds.maximum[i])return RF_FORMAT;
    return RF_OK;
}
static int scene_npc_airborne_endpoint(uint32_t index,const campaign_npc_body *o)
{
    float basis[9],eye[9];rf_eye_angle_state restored_eye={0};uint32_t i;int status;const rf_physics_body_state *b=&o->body.state;
    if(o->movement_orientation!=campaign_identity[0]||o->look.body_angles[0]!=0||o->look.body_angles[2]!=0||
       !isfinite(o->look.body_angles[1])||(o->view.flags_810&0x400u)||
       b->state_124||b->scalar_144!=1||b->reference_15c!=-1||b->word_164)return RF_FORMAT;
    status=rf_look_orientation(o->look.body_angles,basis);if(status)return status;
    /* Match the existing candidate builder exactly; other eye fields have
     * their own saved lane and cannot silently alter this initial basis. */
    memcpy(restored_eye.angles_87c,o->look.angles.angles_87c,12);
    status=rf_eye_physics_orientation(basis,&restored_eye,eye);if(status)return status;
    if(!scene_npc_airborne_basis_equal(b->orientation,basis)||
       !scene_npc_airborne_basis_equal(b->next_orientation,basis)||
       !scene_npc_airborne_basis_equal(campaign_model_owners[index].basis,basis)||
       !scene_npc_airborne_basis_equal(o->look.orientation,eye)||
       memcmp(b->orientation,b->next_orientation,36))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(b->position[i])||b->next_position[i]!=b->position[i]||
       o->published[i]!=b->position[i]||campaign_model_owners[index].position[i]!=b->position[i]||
       !isfinite(b->vector_138[i])||!isfinite(o->collision_contact.point[i])||
       !isfinite(o->collision_contact.velocity[i])||o->command_714[i]!=0)return RF_FORMAT;
    if(!isfinite(o->collision_contact.inverse_mass))return RF_FORMAT;
    return RF_OK;
}
static int scene_npc_airborne_checkpoint_capture(uint32_t index,const campaign_npc_body *o,
    rf_npc_checkpoint_airborne *p)
{
    const float *vectors[6]={o->body.state.velocity,o->body.state.vector_c8,o->body.state.mass_vector_d4,
        o->body.state.vector_e0,o->body.state.vector_ec,o->support_velocity};uint32_t i,j;int status;
    if(!p)return RF_RANGE;memset(p,0,sizeof(*p));
    if(!scene_npc_airborne_potential(index,o))return RF_OK;
    status=scene_npc_airborne_geometry(index,o);if(status)return status;
    status=scene_npc_airborne_endpoint(index,o);if(status)return status;
    if(!scene_npc_airborne_mode()||o->body.state.flags!=0x80000079u||
       (o->object_flags&~RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK)||o->view.flags_7c!=o->object_flags||
       o->support.handle||o->support.material!=-1||o->piece_support.tag||o->piece_reacquire)return RF_FORMAT;
    for(i=0;i<6;i++)for(j=0;j<3;j++)if(!isfinite(vectors[i][j])||(i==5&&fabsf(vectors[i][j])>10000.f))return RF_RANGE;
    p->present=1;p->body_bits=RF_NPC_CHECKPOINT_AIRBORNE_BODY_MASK;p->object_bits=o->object_flags;
    memcpy(p->velocity,vectors[0],12);memcpy(p->angular,vectors[1],12);memcpy(p->momentum,vectors[2],12);
    memcpy(p->force,vectors[3],12);memcpy(p->torque,vectors[4],12);memcpy(p->inherited_velocity,vectors[5],12);
    return RF_OK;
}
/* Fresh constructor is a separately qualified compensation owner. Static
 * material and scalar piece_reacquire may be retained; active borrowed support
 * may not. The old word168-zero subset remains deliberately strict. */
static int scene_npc_airborne_checkpoint_admit(uint32_t index,const campaign_npc_body *o,
    const rf_npc_checkpoint_record *r,uint32_t fresh_boot)
{
    uint32_t i,j;int status;const float *vectors[6]={r->airborne.velocity,r->airborne.angular,
        r->airborne.momentum,r->airborne.force,r->airborne.torque,r->airborne.inherited_velocity};
    if(!r->airborne.present)return RF_OK;
    if(!fresh_boot||r->airborne.present!=1||r->retired||r->dead_pose||!isfinite(r->health)||!(r->health>0)||
       r->flags||!r->movement_present||r->movement_slot!=3||!scene_npc_airborne_mode()||
       r->physics.present||r->drone.kind||r->flight.present||r->support_uid||r->ai_mode==13||
       r->airborne.body_bits!=RF_NPC_CHECKPOINT_AIRBORNE_BODY_MASK||
       (r->airborne.object_bits&~RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK))return RF_FORMAT;
    status=scene_npc_airborne_geometry(index,o);if(status)return status;
    status=scene_npc_airborne_endpoint(index,o);if(status)return status;
    if(r->class_id!=campaign_seeds.items[index].class_index||r->uid!=(uint32_t)campaign_seeds.records.items[index].record.uid||
       o->body.state.flags!=0x80000078u||(o->object_flags&~RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK)||
       o->view.flags_7c!=o->object_flags||o->body.state.word_168||o->support.handle||
       o->piece_support.tag||o->support.material< -1)return RF_FORMAT;
    for(i=0;i<3;i++)if(r->support_velocity[i]!=0||!isfinite(o->support_velocity[i])||
        !isfinite(o->body.state.velocity[i])||!isfinite(o->body.state.vector_c8[i])||
        !isfinite(o->body.state.mass_vector_d4[i])||!isfinite(o->body.state.vector_e0[i])||!isfinite(o->body.state.vector_ec[i]))return RF_FORMAT;
    for(i=0;i<6;i++)for(j=0;j<3;j++)if(!isfinite(vectors[i][j])||(i==5&&fabsf(vectors[i][j])>10000.f))return RF_FORMAT;
    return RF_OK;
}
static void scene_npc_airborne_checkpoint_prepare(const rf_npc_checkpoint_record *r,
    rf_physics_body_state *body,uint32_t *flags)
{
    const rf_npc_checkpoint_airborne *p=&r->airborne;if(!p->present)return;
    body->flags=0x78u|p->body_bits;*flags=(*flags&~RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK)|p->object_bits;
    memcpy(body->velocity,p->velocity,12);memcpy(body->vector_c8,p->angular,12);memcpy(body->mass_vector_d4,p->momentum,12);
    memcpy(body->vector_e0,p->force,12);memcpy(body->vector_ec,p->torque,12);
    body->state_124=body->word_164=body->word_168=0;body->reference_15c=-1;body->scalar_144=1;
    memset(body->vector_138,0,12);
}
static void scene_npc_airborne_checkpoint_assign(campaign_npc_body *o,const rf_npc_checkpoint_record *r)
{
    if(!r->airborne.present)return;
    memset(&o->collision_contact,0,sizeof(o->collision_contact));
    o->collision_contact.material=o->collision_contact.reference=UINT32_MAX;
    o->support.handle=0;o->support.material=-1;memset(&o->piece_support,0,sizeof(o->piece_support));o->piece_reacquire=0;
    memcpy(o->support_velocity,r->airborne.inherited_velocity,12);
}
#endif
