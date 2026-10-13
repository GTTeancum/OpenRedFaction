#ifndef RF_SCENE_NPC_AIRBORNE_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_AIRBORNE_CHECKPOINT_INCLUDED
/* RFNC26: exact ordinary guard1/use0 committed fall3. This owner is distinct
 * from script-physics, Drone, flight and route state. No helper runs physics,
 * animation, a support query, an event or a callback. */
/* Failure-only receipt. Source-line sites are bound to the exact source hash;
 * neither this receipt nor the terminal bank is an admission input. */
typedef struct scene_npc_airborne_failure {
    uint32_t site,part,kind,actual,expected;
} scene_npc_airborne_failure;
uint32_t rf_scene_npc_checkpoint_airborne_observation[16];
static uint32_t scene_npc_airborne_float_bits(const float *value)
{uint32_t bits;memcpy(&bits,value,4);return bits;}
static int scene_npc_airborne_fail(scene_npc_airborne_failure *failure,uint32_t site,
    uint32_t part,uint32_t kind,uint32_t actual,uint32_t expected,int status)
{
    if(failure){failure->site=site;failure->part=part;failure->kind=kind;
        failure->actual=actual;failure->expected=expected;}return status;
}
#define NPC_AIRBORNE_U(test,part,actual,expected) do{if(!(test))return scene_npc_airborne_fail(failure,__LINE__,part,1,(uint32_t)(actual),(uint32_t)(expected),RF_FORMAT);}while(0)
#define NPC_AIRBORNE_F(test,part,actual,expected) do{if(!(test))return scene_npc_airborne_fail(failure,__LINE__,part,2,scene_npc_airborne_float_bits(&(actual)),scene_npc_airborne_float_bits(&(const float){expected}),RF_FORMAT);}while(0)
#define NPC_AIRBORNE_STATUS(call) do{int result=(call);if(result)return scene_npc_airborne_fail(failure,__LINE__,0,0,0,0,result);}while(0)
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
        !o->scripted_physics&&!o->scripted_physics_seen&&
        (!o->flight.qualified||scene_civilian_flight_owner(o))&&!o->flight.entered;
}
static int scene_npc_airborne_identity(uint32_t index,const campaign_npc_body *o,
    scene_npc_airborne_failure *failure)
{
    const campaign_model_owner *m;const rf_entity_pose *p;uint32_t h,cls;
    NPC_AIRBORNE_U(campaign_npc_bodies!=NULL,0,0,1);
    NPC_AIRBORNE_U(index<campaign_npc_body_count,0,index,campaign_npc_body_count);
    NPC_AIRBORNE_U(o==campaign_npc_bodies+index,0,0,1);
    NPC_AIRBORNE_U(scene_npc_airborne_class(index),0,0,1);
    NPC_AIRBORNE_U(campaign_model_owners!=NULL,0,0,1);
    NPC_AIRBORNE_U(index<campaign_model_owner_count,0,index,campaign_model_owner_count);
    NPC_AIRBORNE_U(campaign_poses.items!=NULL,0,0,1);
    NPC_AIRBORNE_U(index<campaign_poses.count,0,index,campaign_poses.count);
    h=o->registration.handle;m=campaign_model_owners+index;p=campaign_poses.items+index;
    cls=campaign_seeds.items[index].class_index;
    NPC_AIRBORNE_U(h!=0,0,h,1);
    NPC_AIRBORNE_U(o->registration.object_kind==0,0,o->registration.object_kind,0);
    NPC_AIRBORNE_U(o->registration.view==&o->view,0,0,1);
    NPC_AIRBORNE_U(o->view.type==0,0,o->view.type,0);
    NPC_AIRBORNE_U((uint32_t)o->view.handle==h,0,o->view.handle,h);
    NPC_AIRBORNE_U(o->damage.effects.handle==h,0,o->damage.effects.handle,h);
    NPC_AIRBORNE_U(rf_object_registry_lookup(&campaign_registry,h)==&o->registration,0,0,1);
    NPC_AIRBORNE_U(rf_entity_lookup(&campaign_entities,(int32_t)h)==&o->view,0,0,1);
    NPC_AIRBORNE_U(!m->owned,0,1,0);
    NPC_AIRBORNE_U(m->pose==p,0,0,1);
    NPC_AIRBORNE_U(p->bone_count!=0,0,p->bone_count,1);
    NPC_AIRBORNE_U(p->matrices!=NULL,0,0,1);
    NPC_AIRBORNE_U(p->generations!=NULL,0,0,1);
    NPC_AIRBORNE_U(p->skeleton<campaign_skeletons.count,0,p->skeleton,campaign_skeletons.count);
    NPC_AIRBORNE_U(campaign_skeletons.items!=NULL,0,0,1);
    NPC_AIRBORNE_U(campaign_skeletons.class_indices!=NULL,0,0,1);
    NPC_AIRBORNE_U(cls<campaign_skeletons.class_count,0,cls,campaign_skeletons.class_count);
    NPC_AIRBORNE_U(campaign_skeletons.class_indices[cls]==p->skeleton,0,campaign_skeletons.class_indices[cls],p->skeleton);
    NPC_AIRBORNE_U(p->bone_count==campaign_skeletons.items[p->skeleton].count,0,p->bone_count,campaign_skeletons.items[p->skeleton].count);
    NPC_AIRBORNE_U(campaign_skeletons.items[p->skeleton].bones!=NULL,0,0,1);
    NPC_AIRBORNE_U(scene_npc_airborne_name(campaign_skeletons.items[p->skeleton].model,"ult2_guard.v3c"),0,0,1);
    NPC_AIRBORNE_U(o->persistence_registered,0,0,1);
    NPC_AIRBORNE_U(rf_scene_defeated_actors.count<=RF_CAMPAIGN_ACTOR_SLOTS,0,rf_scene_defeated_actors.count,RF_CAMPAIGN_ACTOR_SLOTS);
    NPC_AIRBORNE_U(o->persistence_slot<rf_scene_defeated_actors.count,0,o->persistence_slot,rf_scene_defeated_actors.count);
    NPC_AIRBORNE_U(o->persistence_slot<RF_CAMPAIGN_ACTOR_SLOTS,0,o->persistence_slot,RF_CAMPAIGN_ACTOR_SLOTS);
    NPC_AIRBORNE_U(!rf_scene_defeated_actors.items[o->persistence_slot].retired,0,rf_scene_defeated_actors.items[o->persistence_slot].retired,0);
    NPC_AIRBORNE_U(rf_scene_defeated_actors.items[o->persistence_slot].uid==(uint32_t)campaign_seeds.records.items[index].record.uid,0,
        rf_scene_defeated_actors.items[o->persistence_slot].uid,campaign_seeds.records.items[index].record.uid);
    NPC_AIRBORNE_F(isfinite(o->damage.effects.health)&&o->damage.effects.health>0,0,o->damage.effects.health,0);
    NPC_AIRBORNE_U(!(o->view.flags_810&1u),0,o->view.flags_810&1u,0);
    NPC_AIRBORNE_U(!(o->damage.effects.flags_810&1u),0,o->damage.effects.flags_810&1u,0);
    NPC_AIRBORNE_U(o->death.action_824==-1,0,o->death.action_824,-1);
    NPC_AIRBORNE_U(o->view.linked_handle==-1,0,o->view.linked_handle,-1);
    NPC_AIRBORNE_U(o->view.occupant_count==0,0,o->view.occupant_count,0);
    NPC_AIRBORNE_U(o->attachment_75c==-1,0,o->attachment_75c,-1);
    NPC_AIRBORNE_U(!scene_npc_seat_attached(h),0,1,0);
    NPC_AIRBORNE_U(!o->scripted_physics,0,o->scripted_physics,0);
    NPC_AIRBORNE_U(!o->scripted_physics_seen,0,o->scripted_physics_seen,0);
    /* A lazy civilian constructor owns no airborne/body aliases. */
    NPC_AIRBORNE_U(!o->flight.qualified||scene_civilian_flight_owner(o),0,o->flight.qualified,0);
    NPC_AIRBORNE_U(!o->flight.entered,0,o->flight.entered,0);
    NPC_AIRBORNE_U(!o->robot_death.state,0,o->robot_death.state,0);
    return RF_OK;
}
static int scene_npc_airborne_basis_equal(const float *a,const float *b,
    scene_npc_airborne_failure *failure,uint32_t site)
{
    uint32_t i;for(i=0;i<9;i++)if(!isfinite(a[i])||!isfinite(b[i])||fabsf(a[i]-b[i])>1e-6f)
        return scene_npc_airborne_fail(failure,site,i,2,scene_npc_airborne_float_bits(&a[i]),
            scene_npc_airborne_float_bits(&b[i]),RF_FORMAT);
    return RF_OK;
}
/* Verify the existing immutable loaded class caches. No posed triangle query
 * or fresh pose evaluation supplies physical standing centers. */
static int scene_npc_airborne_geometry(uint32_t index,const campaign_npc_body *o,
    scene_npc_airborne_failure *failure)
{
    uint32_t cls,i,j,flags,physical=!!(o->view.flags_810&0x400u);float tensor[9];rf_physics_bounds bounds;rf_movement_settings speed;
    const rf_physics_stance_cache *stance;const scene_npc_hit_location_class *hit;
    const rf_entity_material *material;const rf_physics_body_state *b=&o->body.state;int status;
    status=scene_npc_airborne_identity(index,o,failure);if(status)return status;
    cls=campaign_seeds.items[index].class_index;
    if(physical)NPC_AIRBORNE_STATUS(scene_crouch_live_shape(index,o));
    NPC_AIRBORNE_U(campaign_npc_stances!=NULL,0,0,1);
    NPC_AIRBORNE_U(scene_npc_hit_location_classes!=NULL,0,0,1);
    NPC_AIRBORNE_U(campaign_npc_movement_configs!=NULL,0,0,1);
    NPC_AIRBORNE_U(campaign_surface_palette!=NULL,0,0,1);
    NPC_AIRBORNE_U(campaign_surface_palette->count>3,0,campaign_surface_palette->count,4);
    stance=campaign_npc_stances+cls;hit=scene_npc_hit_location_classes+cls;material=campaign_surface_palette->materials+3;
    NPC_AIRBORNE_U(stance->count==3,0,stance->count,3);
    NPC_AIRBORNE_U(hit->enabled,0,0,1);
    NPC_AIRBORNE_U(hit->count==3,0,hit->count,3);
    NPC_AIRBORNE_U(hit->skeleton==campaign_poses.items[index].skeleton,0,hit->skeleton,campaign_poses.items[index].skeleton);
    NPC_AIRBORNE_U(o->body.spheres.count==3,0,o->body.spheres.count,3);
    NPC_AIRBORNE_U(o->body.spheres.items!=NULL,0,0,1);
    NPC_AIRBORNE_U(o->body.allocated_bytes==sizeof(o->body)+3u*sizeof(*o->body.spheres.items),0,
        o->body.allocated_bytes,sizeof(o->body)+3u*sizeof(*o->body.spheres.items));
    NPC_AIRBORNE_U(o->body.spheres.allocated_bytes==sizeof(o->body.spheres)+3u*sizeof(*o->body.spheres.items),0,
        o->body.spheres.allocated_bytes,sizeof(o->body.spheres)+3u*sizeof(*o->body.spheres.items));
    NPC_AIRBORNE_U(material->index==3,0,material->index,3);
    NPC_AIRBORNE_F(isfinite(material->elasticity),0,material->elasticity,0);
    NPC_AIRBORNE_F(isfinite(material->friction),0,material->friction,0);
    NPC_AIRBORNE_U(o->collision_material==3,0,o->collision_material,3);
    NPC_AIRBORNE_F(b->mass==campaign_seeds.classes[cls].physics.mass,0,b->mass,campaign_seeds.classes[cls].physics.mass);
    NPC_AIRBORNE_F(isfinite(b->mass)&&b->mass>0,0,b->mass,0);
    NPC_AIRBORNE_F(b->coefficients[0]==material->elasticity,0,b->coefficients[0],material->elasticity);
    NPC_AIRBORNE_F(b->coefficients[2]==material->friction,2,b->coefficients[2],material->friction);
    NPC_AIRBORNE_U(o->movement.mode>=0&&o->movement.mode<=2,0,o->movement.mode,2);
    for(i=0;i<3;i++){
        const rf_physics_sphere *sphere=o->body.spheres.items+i;
        NPC_AIRBORNE_U(hit->spheres[i].index==i,i,hit->spheres[i].index,i);
        NPC_AIRBORNE_F(isfinite(hit->spheres[i].radius)&&hit->spheres[i].radius>0,i,hit->spheres[i].radius,0);
        NPC_AIRBORNE_F(sphere->radius==hit->spheres[i].radius,i,sphere->radius,hit->spheres[i].radius);
        NPC_AIRBORNE_F(sphere->parameter_10==-1,i,sphere->parameter_10,-1);
        NPC_AIRBORNE_U(!sphere->opaque_14,i,sphere->opaque_14,0);
        for(j=0;j<3;j++)NPC_AIRBORNE_F(isfinite(sphere->center[j])&&isfinite(stance->centers[physical][i][j])&&
            sphere->center[j]==stance->centers[physical][i][j],i*3+j,sphere->center[j],stance->centers[physical][i][j]);
    }
    flags=rf_entity_creation_physics_flags(campaign_seeds.items[index].spawn.creation_flags,
        campaign_seeds.classes[cls].physics.flags,campaign_seeds.classes[cls].physics.flags2,
        campaign_seeds.classes[cls].physics.use_kind,0);
    NPC_AIRBORNE_U(flags==0x80000078u,0,flags,0x80000078u);
    speed=o->movement;NPC_AIRBORNE_STATUS(scene_npc_resolve_speed(o,cls,0,o->movement.mode,-1,&speed));
    NPC_AIRBORNE_F(isfinite(o->movement.response),0,o->movement.response,0);
    NPC_AIRBORNE_F(isfinite(o->movement.speed),0,o->movement.speed,0);
    NPC_AIRBORNE_F(o->movement.response==speed.response,0,o->movement.response,speed.response);
    NPC_AIRBORNE_F(o->movement.speed==speed.speed,0,o->movement.speed,speed.speed);
    NPC_AIRBORNE_F(b->coefficients[1]==speed.response,1,b->coefficients[1],speed.response);
    for(i=0;i<9;i++){
        NPC_AIRBORNE_F(isfinite(b->local_tensor[i])&&b->local_tensor[i]==0,i,b->local_tensor[i],0);
        NPC_AIRBORNE_F(isfinite(b->orientation[i]),i,b->orientation[i],0);
    }
    NPC_AIRBORNE_STATUS(rf_physics_tensor_world(b->local_tensor,b->orientation,tensor));
    for(i=0;i<9;i++)NPC_AIRBORNE_F(isfinite(b->world_tensor[i])&&b->world_tensor[i]==tensor[i],i,b->world_tensor[i],tensor[i]);
    NPC_AIRBORNE_STATUS(rf_physics_spheres_bounds(o->body.spheres.items,3,b->position,&bounds));
    NPC_AIRBORNE_F(b->bounds.radius==bounds.radius,0,b->bounds.radius,bounds.radius);
    for(i=0;i<3;i++){
        NPC_AIRBORNE_F(b->bounds.minimum[i]==bounds.minimum[i],i,b->bounds.minimum[i],bounds.minimum[i]);
        NPC_AIRBORNE_F(b->bounds.maximum[i]==bounds.maximum[i],i,b->bounds.maximum[i],bounds.maximum[i]);
    }
    return RF_OK;
}
static int scene_npc_airborne_endpoint(uint32_t index,const campaign_npc_body *o,
    scene_npc_airborne_failure *failure)
{
    float basis[9],eye[9];uint32_t i;int status;const rf_physics_body_state *b=&o->body.state;
    NPC_AIRBORNE_U(o->movement_orientation==campaign_identity[0],0,0,1);
    NPC_AIRBORNE_F(o->look.body_angles[0]==0,0,o->look.body_angles[0],0);
    NPC_AIRBORNE_F(o->look.body_angles[2]==0,2,o->look.body_angles[2],0);
    NPC_AIRBORNE_F(isfinite(o->look.body_angles[1]),1,o->look.body_angles[1],0);
    if(o->view.flags_810&0x400u)NPC_AIRBORNE_STATUS(scene_crouch_live_shape(index,o));
    NPC_AIRBORNE_U(!b->state_124,0,b->state_124,0);
    NPC_AIRBORNE_F(b->scalar_144==1,0,b->scalar_144,1);
    NPC_AIRBORNE_U(b->reference_15c==-1,0,b->reference_15c,-1);
    NPC_AIRBORNE_U(!b->word_164,0,b->word_164,0);
    NPC_AIRBORNE_STATUS(rf_look_orientation(o->look.body_angles,basis));
    /* Ordinary49d0a0 owns4a0d70(body+eye87c), including saved eye yaw.
     * No eye integration, offsets or extra clamp/wrap occurs during capture. */
    NPC_AIRBORNE_STATUS(rf_ordinary_eye_orientation(o->look.body_angles,o->look.angles.angles_87c,eye));
    status=scene_npc_airborne_basis_equal(b->orientation,basis,failure,__LINE__);if(status)return status;
    status=scene_npc_airborne_basis_equal(b->next_orientation,basis,failure,__LINE__);if(status)return status;
    status=scene_npc_airborne_basis_equal(campaign_model_owners[index].basis,basis,failure,__LINE__);if(status)return status;
    status=scene_npc_airborne_basis_equal(o->look.orientation,eye,failure,__LINE__);if(status)return status;
    /* The previous memcmp was exact, including signed zeros. */
    for(i=0;i<9;i++)NPC_AIRBORNE_U(scene_npc_airborne_float_bits(&b->orientation[i])==scene_npc_airborne_float_bits(&b->next_orientation[i]),i,
        scene_npc_airborne_float_bits(&b->orientation[i]),scene_npc_airborne_float_bits(&b->next_orientation[i]));
    for(i=0;i<3;i++){
        NPC_AIRBORNE_F(isfinite(b->position[i]),i,b->position[i],0);
        NPC_AIRBORNE_F(b->next_position[i]==b->position[i],i,b->next_position[i],b->position[i]);
        NPC_AIRBORNE_F(o->published[i]==b->position[i],i,o->published[i],b->position[i]);
        NPC_AIRBORNE_F(campaign_model_owners[index].position[i]==b->position[i],i,campaign_model_owners[index].position[i],b->position[i]);
        NPC_AIRBORNE_F(isfinite(b->vector_138[i]),i,b->vector_138[i],0);
        NPC_AIRBORNE_F(isfinite(o->collision_contact.point[i]),i,o->collision_contact.point[i],0);
        NPC_AIRBORNE_F(isfinite(o->collision_contact.velocity[i]),i,o->collision_contact.velocity[i],0);
        NPC_AIRBORNE_F(o->command_714[i]==0,i,o->command_714[i],0);
    }
    NPC_AIRBORNE_F(isfinite(o->collision_contact.inverse_mass),0,o->collision_contact.inverse_mass,0);
    return RF_OK;
}
static int scene_npc_airborne_checkpoint_capture(uint32_t index,const campaign_npc_body *o,
    rf_npc_checkpoint_airborne *p,scene_npc_airborne_failure *failure)
{
    const float *vectors[6]={o->body.state.velocity,o->body.state.vector_c8,o->body.state.mass_vector_d4,
        o->body.state.vector_e0,o->body.state.vector_ec,o->support_velocity};uint32_t i,j;int status;
    const rf_movement_descriptor *m=campaign_modes+3;
    if(failure)memset(failure,0,sizeof(*failure));
    if(!p)return scene_npc_airborne_fail(failure,__LINE__,0,0,0,0,RF_RANGE);memset(p,0,sizeof(*p));
    if(!scene_npc_airborne_potential(index,o))return RF_OK;
    status=scene_npc_airborne_geometry(index,o,failure);if(status)return status;
    status=scene_npc_airborne_endpoint(index,o,failure);if(status)return status;
    NPC_AIRBORNE_U(m->enabled&255u,0,m->enabled,1);
    NPC_AIRBORNE_U(m->index==3,0,m->index,3);
    NPC_AIRBORNE_U(m->translation[0]==2,0,m->translation[0],2);
    NPC_AIRBORNE_U(m->translation[1]==0,1,m->translation[1],0);
    NPC_AIRBORNE_U(m->translation[2]==2,2,m->translation[2],2);
    NPC_AIRBORNE_U(m->rotation[0]==1,0,m->rotation[0],1);
    NPC_AIRBORNE_U(m->rotation[1]==3,1,m->rotation[1],3);
    NPC_AIRBORNE_U(m->rotation[2]==0,2,m->rotation[2],0);
    NPC_AIRBORNE_U(o->body.state.flags==0x80000079u,0,o->body.state.flags,0x80000079u);
    NPC_AIRBORNE_U(!(o->object_flags&~RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK),0,o->object_flags,RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK);
    NPC_AIRBORNE_U(o->view.flags_7c==o->object_flags,0,o->view.flags_7c,o->object_flags);
    NPC_AIRBORNE_U(!o->support.handle,0,o->support.handle,0);
    NPC_AIRBORNE_U(o->support.material==-1,0,o->support.material,-1);
    NPC_AIRBORNE_U(!o->piece_support.tag,0,o->piece_support.tag,0);
    NPC_AIRBORNE_U(!o->piece_reacquire,0,o->piece_reacquire,0);
    for(i=0;i<6;i++)for(j=0;j<3;j++)if(!isfinite(vectors[i][j])||(i==5&&fabsf(vectors[i][j])>10000.f))
        return scene_npc_airborne_fail(failure,__LINE__,i*3+j,2,scene_npc_airborne_float_bits(&vectors[i][j]),
            i==5?scene_npc_airborne_float_bits(&(const float){10000.f}):0,RF_RANGE);
    p->present=1;p->body_bits=RF_NPC_CHECKPOINT_AIRBORNE_BODY_MASK;p->object_bits=o->object_flags;
    memcpy(p->velocity,vectors[0],12);memcpy(p->angular,vectors[1],12);memcpy(p->momentum,vectors[2],12);
    memcpy(p->force,vectors[3],12);memcpy(p->torque,vectors[4],12);memcpy(p->inherited_velocity,vectors[5],12);
    return RF_OK;
}
#undef NPC_AIRBORNE_U
#undef NPC_AIRBORNE_F
#undef NPC_AIRBORNE_STATUS
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
    status=scene_npc_airborne_geometry(index,o,NULL);if(status)return status;
    status=scene_npc_airborne_endpoint(index,o,NULL);if(status)return status;
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
