#ifndef RF_SCENE_NPC_HIT_LOCATION_INCLUDED
#define RF_SCENE_NPC_HIT_LOCATION_INCLUDED
/* Player-owned48c26d ->49afe0 thin posed contact, then SP42ce00. Only the
 * three admitted humanoid classes have mandatory metadata. Scheduling stays
 * the existing instantaneous ray/continuation policy, not projectile flight. */
enum {SCENE_BODY_CONTACT_FALLBACK=0,SCENE_BODY_CONTACT_REJECT=1,
      SCENE_BODY_CONTACT_MISS=2,SCENE_BODY_CONTACT_HIT=3};
typedef struct scene_player_body_shot {
    uint64_t epoch;
    uint32_t active,source,slot,alternate,pellets,flags;
    int32_t weapon,kind;
    float base,spread,start[3],basis[9],speed,radius,planes[4][4];
} scene_player_body_shot;
typedef struct scene_npc_body_contact {
    uint32_t outcome,matched,slot,handle,cls,skeleton,generation,index,reaction_frame;
    uint32_t object_flags,body_flags,flags_810,flags_814,affiliation,source_affiliation,class_flags;
    int32_t linked_handle,pain_action;
    const campaign_npc_body *owner;const rf_entity_pose *pose;
    float fraction,point[3],factor,position[3],basis[9],published[3],armor;
    float published_basis[9],source_published[3];
    rf_physics_bounds bounds;
} scene_npc_body_contact;

static uint32_t scene_player_body_source_live(const scene_player_body_shot *shot)
{
    if(!shot->active)return 1; /* Existing noncampaign caller policy. */
    return shot->epoch==scene_continuous_charge_runtime.epoch &&
        shot->source==campaign_player_object.handle &&
        campaign_player_object.view==&campaign_player_view && (campaign_player_view.flags_7c&8u) &&
        rf_object_registry_lookup(&campaign_registry,shot->source)==&campaign_player_object &&
        rf_entity_lookup(&campaign_entities,(int32_t)shot->source)==&campaign_player_view;
}
/* Capture after accepted ammo debit, before presentation/damage callbacks.
 * A later equipment selection cannot reinterpret this already accepted shot. */
static int scene_player_body_shot_capture(uint32_t slot,uint32_t alternate,uint32_t shells,
    float spread,const float start[3],const float basis[9],scene_player_body_shot *result)
{
    scene_player_body_shot value={0};uint32_t profile=slot;
    int32_t weapon=campaign_selected_weapon();
    if(!result || !start || !basis || slot>=SCENE_WEAPON_SLOTS || alternate>1 ||
       !isfinite(spread) || spread<0 || spread>90)return RF_RANGE;
    value.slot=slot;value.alternate=alternate;value.weapon=weapon;
    value.kind=campaign_pistol.damage_kind;value.base=alternate?campaign_pistol.alt_damage:campaign_pistol.damage;
    value.pellets=slot==3?campaign_pistol.projectiles*shells:1;
    value.spread=spread;value.source=campaign_player_object.handle;
    value.epoch=scene_continuous_charge_runtime.epoch;
    if(!value.pellets || value.pellets>64 || !isfinite(value.base) || value.base<0)return RF_RANGE;
    memcpy(value.start,start,12);memcpy(value.basis,basis,36);
    for(uint32_t i=0;i<3;i++)if(!isfinite(start[i]))return RF_RANGE;
    for(uint32_t i=0;i<9;i++)if(!isfinite(basis[i]))return RF_RANGE;
    if(campaign_player_object.view) {
        if(campaign_player_object.view!=&campaign_player_view ||
           rf_object_registry_lookup(&campaign_registry,value.source)!=&campaign_player_object ||
           rf_entity_lookup(&campaign_entities,(int32_t)value.source)!=&campaign_player_view ||
           !(campaign_player_view.flags_7c&8u) || !value.epoch)return RF_FORMAT;
        value.active=1;
    }
    if(value.active) {
        int32_t reset_definition;
        if(slot==13 && weapon==campaign_machine_special_id)profile=17;
        if(weapon<0 || weapon>=64 || (uint32_t)weapon>=campaign_weapon_supply.names.count ||
           !scene_player_body_geometry_ready[profile] ||
           rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[profile])!=weapon)return RF_FORMAT;
        value.speed=scene_player_body_geometry[profile].speed;
        value.radius=scene_player_body_geometry[profile].collision_radius;
        /* Flags2 belongs to the retained descriptor/reset catalog, not the
         * ammo-acquisition subset. Resolve this same admitted profile by name. */
        if(!campaign_weapon_reset.names.count || campaign_weapon_reset.names.count>64)return RF_FORMAT;
        reset_definition=rf_weapon_name_find(&campaign_weapon_reset.names,campaign_weapon_names[profile]);
        if(reset_definition<0 || (uint32_t)reset_definition>=campaign_weapon_reset.names.count)return RF_FORMAT;
        value.flags=campaign_weapon_reset.definitions[reset_definition].flags_268;
        /* Selected original ordinary profiles all take54e530's thin branch. */
        if(!isfinite(value.speed) || value.speed<=0 || !isfinite(value.radius) ||
           value.radius<0 || value.radius>.025f)return RF_FORMAT;
    }
    *result=value;return RF_OK;
}
static int scene_player_body_shot_ray(const scene_player_body_shot *source,const float delta[3],
    scene_player_body_shot *result)
{
    scene_player_body_shot value;rf_collision_projectile_plane_source planes;int status;
    if(!source || !delta || !result)return RF_RANGE;
    value=*source;
    /*4265ef..647: zero spread copies the actual incoming basis; positive
     * spread uses4fcea0 after the already accepted single cone sample. */
    if(value.spread>0) {
        status=rf_entity_navigation_basis(delta,(float (*)[3])value.basis);if(status)return status;
    }
    if(value.active && (value.flags&0x20u)) {
        memcpy(planes.position,value.start,12);memcpy(planes.basis,value.basis,36);planes.speed=value.speed;
        rf_collision_projectile_planes(&planes,value.planes);
        for(uint32_t i=0;i<16;i++)if(!isfinite(value.planes[i/4][i%4]))return RF_FORMAT;
    }
    *result=value;return RF_OK;
}
static uint32_t scene_npc_body_contact_live(const scene_player_body_shot *shot,
    const scene_npc_body_contact *contact)
{
    const campaign_npc_body *owner;
    if(!scene_player_body_source_live(shot) || !campaign_npc_bodies ||
       contact->slot>=campaign_npc_body_count)return 0;
    owner=campaign_npc_bodies+contact->slot;
    if(owner!=contact->owner || owner->registration.handle!=contact->handle ||
       owner->registration.view!=&owner->view ||
       rf_object_registry_lookup(&campaign_registry,contact->handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)contact->handle)!=&owner->view ||
       !owner->body.allocated_bytes || owner->damage.effects.health<=0 ||
       (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u) ||
       owner->object_flags!=contact->object_flags || owner->body.state.flags!=contact->body_flags ||
       owner->view.flags_810!=contact->flags_810 || owner->damage.effects.flags_814!=contact->flags_814 ||
       owner->damage.effects.affiliation!=contact->affiliation ||
       owner->view.linked_handle!=contact->linked_handle ||
       campaign_player_damage.state.effects.affiliation!=contact->source_affiliation ||
       memcmp(&owner->damage.effects.armor,&contact->armor,4) ||
       memcmp(owner->published,contact->published,12) ||
       memcmp(&owner->body.state.bounds,&contact->bounds,sizeof(contact->bounds)) ||
       memcmp(owner->body.state.position,contact->position,12) ||
       memcmp(owner->body.state.orientation,contact->basis,36))return 0;
    if(!campaign_seeds.items || contact->slot>=campaign_seeds.records.count ||
       campaign_seeds.items[contact->slot].class_index!=contact->cls ||
       contact->cls>=campaign_seeds.class_count ||
       campaign_seeds.classes[contact->cls].physics.flags!=contact->class_flags)return 0;
    if(contact->outcome==SCENE_BODY_CONTACT_HIT) {
        const rf_entity_view *linked=rf_entity_lookup(&campaign_entities,owner->view.linked_handle);
        if(linked && linked->class_type==1)return 0;
        if(contact->reaction_frame &&
           (!campaign_model_owners || contact->slot>=campaign_model_owner_count ||
            memcmp(campaign_model_owners[contact->slot].basis,contact->published_basis,36) ||
            (shot->slot==3 && memcmp(rf_scene_actor_pose.public_position,contact->source_published,12))))return 0;
        rf_entity_pose *pose=NULL;
        if(!scene_npc_hit_location_classes || contact->cls>=campaign_seeds.class_count ||
           campaign_seeds.items[contact->slot].class_index!=contact->cls ||
           scene_npc_hit_location_classes[contact->cls].skeleton!=contact->skeleton ||
           campaign_model_pose(contact->slot,&pose) || pose!=contact->pose ||
           !pose || pose->skeleton!=contact->skeleton ||
           pose->playback.generation!=contact->generation)return 0;
    }
    return 1;
}
static void scene_npc_body_contact_snapshot(uint32_t slot,scene_npc_body_contact *contact)
{
    const campaign_npc_body *owner=campaign_npc_bodies+slot;
    contact->slot=slot;contact->handle=owner->registration.handle;contact->owner=owner;
    contact->object_flags=owner->object_flags;contact->body_flags=owner->body.state.flags;
    contact->flags_810=owner->view.flags_810;contact->flags_814=owner->damage.effects.flags_814;
    contact->affiliation=owner->damage.effects.affiliation;
    contact->linked_handle=owner->view.linked_handle;
    contact->source_affiliation=campaign_player_damage.state.effects.affiliation;
    contact->armor=owner->damage.effects.armor;contact->bounds=owner->body.state.bounds;
    memcpy(contact->published,owner->published,12);
    memcpy(contact->position,owner->body.state.position,12);
    memcpy(contact->basis,owner->body.state.orientation,36);
}
static int scene_npc_hit_world(const float local[3],const scene_npc_body_contact *contact,float result[3])
{
    for(uint32_t i=0;i<3;i++) {
        /*503290/49b3ac: rotate local geometry, then separately add origin. */
        float rotated=(float)(((double)local[2]*contact->basis[6+i]+
            (double)local[1]*contact->basis[3+i])+(double)local[0]*contact->basis[i]);
        result[i]=(float)((double)rotated+contact->position[i]);
        if(!isfinite(result[i]))return RF_RANGE;
    }
    return RF_OK;
}
/* Pure42ce00 SP scalar selection shared by independently admitted contact
 * paths. No source identity, collision query, reaction or damage publication.
 * Arithmetic and float-store order are unchanged from the PLAYER loop. */
static int scene_npc_body_contact_scalar(const scene_npc_hit_location_class *definition,
    const rf_entity_pose *pose,scene_npc_body_contact *value)
{
    float minimum=FLT_MAX;int status;
    const rf_entity_render_model *model=campaign_render_models.items+pose->skeleton;
    for(uint32_t i=0;i<definition->count;i++) {
        float center[4],world[3],difference[3],distance,score;double length,quotient;
        uint32_t index=definition->spheres[i].index;
        if(index>=model->collision_sphere_count)return RF_FORMAT;
        status=rf_model_collision_sphere_pose(model->collision_spheres+index,
            pose->matrices,pose->bone_count,center);if(status)return status;
        status=scene_npc_hit_world(center,value,world);if(status)return status;
        for(uint32_t j=0;j<3;j++)difference[j]=(float)((double)world[j]-value->point[j]);
        length=sqrt(((double)difference[0]*difference[0]+(double)difference[1]*difference[1])+
            (double)difference[2]*difference[2]);
        if(!isfinite(length) || length>FLT_MAX)return RF_RANGE;
        distance=(float)length;quotient=(double)distance/definition->spheres[i].radius;score=(float)quotient;
        if(!isfinite(quotient) || !isfinite(score))return RF_RANGE;
        if(quotient<minimum){minimum=score;value->factor=definition->spheres[i].scalar;value->index=i;}
    }
    return RF_OK;
}
/*4c5e93..4c5fbd: the reaction side uses published3c/48, distinct from
 * physics e4/fc used for the posed contact and scalar. Index2 and the close
 * SP Shotgun exception leave828 unchanged. No death hint is supplied here. */
static int scene_npc_body_contact_reaction_prepare(const scene_player_body_shot *shot,
    scene_npc_body_contact *contact)
{
    float difference[3],side;uint32_t axis;
    contact->pain_action=-1;contact->reaction_frame=0;
    if(contact->outcome!=SCENE_BODY_CONTACT_HIT || !(contact->class_flags&0x20000u) ||
       (contact->flags_810&1u) || contact->index>1)return RF_OK;
    if(!campaign_model_owners || contact->slot>=campaign_model_owner_count)return RF_FORMAT;
    memcpy(contact->published_basis,campaign_model_owners[contact->slot].basis,36);
    for(uint32_t i=0;i<9;i++)if(!isfinite(contact->published_basis[i]))return RF_FORMAT;
    contact->reaction_frame=1;
    if(shot->slot==3) {
        double squared;
        /*4c5e70..8f compares actor positions, not muzzle/contact distance. */
        memcpy(contact->source_published,rf_scene_actor_pose.public_position,12);
        for(uint32_t i=0;i<3;i++) {
            difference[i]=(float)((double)contact->source_published[i]-contact->published[i]);
            if(!isfinite(difference[i]))return RF_FORMAT;
        }
        squared=((double)difference[0]*difference[0]+(double)difference[1]*difference[1])+
            (double)difference[2]*difference[2];
        if(!isfinite(squared))return RF_RANGE;
        if(squared<25.0)return RF_OK; /*589448; equality follows location choice. */
    }
    for(uint32_t i=0;i<3;i++) {
        difference[i]=(float)((double)contact->point[i]-contact->published[i]);
        if(!isfinite(difference[i]))return RF_RANGE;
    }
    axis=contact->index==1?6u:0u;
    side=(float)(((double)difference[2]*contact->published_basis[axis+2]+
        (double)difference[1]*contact->published_basis[axis+1])+
        (double)difference[0]*contact->published_basis[axis]);
    if(!isfinite(side))return RF_RANGE;
    contact->pain_action=contact->index==1?(side>0?24:25):(side>0?27:26);
    return RF_OK;
}
/* Publish only after shield/Nano and all read-only damage preparation. The
 * caller enters generic damage immediately afterward without a callback.
 * A suppressed or lethal damage result does not roll back original828. */
static uint32_t scene_npc_body_contact_reaction_publish(const scene_player_body_shot *shot,
    const scene_npc_body_contact *contact)
{
    if(!scene_npc_body_contact_live(shot,contact))return 0;
    if(contact->reaction_frame && contact->pain_action>=24 && contact->pain_action<=27)
        campaign_npc_bodies[contact->slot].pain.selected_action=contact->pain_action;
    return 1;
}
static int scene_npc_body_contact_query(uint32_t slot,const scene_player_body_shot *shot,
    const float start[3],const float delta[3],float limit,scene_npc_body_contact *result)
{
    scene_npc_body_contact value={0};campaign_npc_body *owner;
    const scene_npc_hit_location_class *definition=NULL;rf_entity_pose *pose=NULL;
    rf_collision_model_part_query query={0};
    rf_collision_model_response_hit hit={0};rf_collision_projectile_eligibility eligibility={0};
    uint32_t accepted,cls;int status;
    if(!shot || !start || !delta || !result || !isfinite(limit) || limit<0 || limit>1 ||
       !campaign_npc_bodies || slot>=campaign_npc_body_count)return RF_RANGE;
    owner=campaign_npc_bodies+slot;value.factor=1;value.fraction=limit;value.index=UINT32_MAX;value.pain_action=-1;
    scene_npc_body_contact_snapshot(slot,&value);
    if(!scene_player_body_source_live(shot) || !owner->registration.view ||
       rf_object_registry_lookup(&campaign_registry,value.handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)value.handle)!=&owner->view ||
       !owner->body.allocated_bytes || owner->damage.effects.health<=0 ||
       (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u)) {
        value.outcome=SCENE_BODY_CONTACT_REJECT;*result=value;return RF_OK;
    }
    if(slot>=campaign_seeds.records.count)return RF_RANGE;
    cls=campaign_seeds.items[slot].class_index;
    if(cls>=campaign_seeds.class_count)return RF_RANGE;
    value.cls=cls;value.class_flags=campaign_seeds.classes[cls].physics.flags;
    if(scene_npc_hit_location_classes)definition=scene_npc_hit_location_classes+cls;
    if(scene_hit_location_class_named(campaign_seeds.records.items[slot].record.class_name) &&
       (!definition || !definition->enabled))return RF_FORMAT;
    /* Unrelated classes retain every old proxy admission and scalar1. */
    if(!shot->active || !definition || !definition->enabled)goto fallback;
    if(value.handle==shot->source) {
        value.outcome=SCENE_BODY_CONTACT_REJECT;*result=value;return RF_OK;
    }
    {const rf_entity_view *linked=rf_entity_lookup(&campaign_entities,owner->view.linked_handle);
     if(linked && linked->class_type==1) {
        value.outcome=SCENE_BODY_CONTACT_REJECT;*result=value;return RF_OK;
     }}
    /* No invented player AI560; Nano geometry and body400 are separate source
     * branches. Their existing unscaled proxy policy remains explicit. */
    if((!campaign_player_damage.state.effects.affiliation && !value.affiliation) ||
       (owner->body.state.flags&0x400u) ||
       rf_entity_armor_immunity(owner->damage.effects.armor,value.class_flags,
           owner->damage.effects.flags_814))goto fallback;
    value.outcome=SCENE_BODY_CONTACT_MISS;value.cls=cls;value.skeleton=definition->skeleton;
    memcpy(eligibility.projectile_position,shot->start,12);memcpy(eligibility.projectile_forward,shot->basis+6,12);
    memcpy(eligibility.target_position,owner->published,12);memcpy(eligibility.target_next_position,value.position,12);
    eligibility.projectile_extent=shot->radius;eligibility.target_extent=owner->body.state.bounds.radius;
    eligibility.definition_flags=shot->flags;eligibility.target_kind=0;
    eligibility.target_field_1f8=value.affiliation;eligibility.target_handle=value.handle;
    eligibility.owner_present=1;eligibility.owner_field_1f8=campaign_player_damage.state.effects.affiliation;
    eligibility.owner_target_560=UINT32_MAX;eligibility.mode=1;
    memcpy(eligibility.planes,shot->planes,sizeof(eligibility.planes));
    if(!rf_collision_projectile_eligible(&eligibility)) {
        value.outcome=SCENE_BODY_CONTACT_REJECT;*result=value;return RF_OK;
    }
    /*49b02b ->46c340 admits overlapping swept AABBs, not a segment
     * entering the target cube. The latter would add an unsupported veto for
     * posed geometry outside its retained constructor bounds. */
    for(uint32_t i=0;i<3;i++) {
        float end,low,high;
        if(!isfinite(start[i]) || !isfinite(delta[i]))return RF_RANGE;
        end=(float)((double)start[i]+delta[i]);
        low=(float)((double)(start[i]<end?start[i]:end)-shot->radius);
        high=(float)((double)(start[i]>end?start[i]:end)+shot->radius);
        if(!isfinite(end) || !isfinite(low) || !isfinite(high) ||
           !isfinite(value.bounds.minimum[i]) || !isfinite(value.bounds.maximum[i]))return RF_RANGE;
        if(!(low<value.bounds.maximum[i]) || !(high>value.bounds.minimum[i])) {
            *result=value;return RF_OK;
        }
    }
    status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || pose->skeleton!=definition->skeleton ||
       pose->skeleton>=campaign_render_models.count || !campaign_render_models.items)return RF_FORMAT;
    memcpy(query.input.start,start,12);memcpy(query.input.displacement,delta,12);
    memcpy(query.input.origin,value.position,12);memcpy(query.input.matrix,value.basis,36);
    query.input.radius=shot->radius;hit.time=limit; /* flags0: nearest, world input. */
    status=rf_scene_model_collision_query(slot,&query,&hit,0,&accepted);if(status)return status;
    if(!accepted){*result=value;return RF_OK;}
    if(!isfinite(hit.time) || hit.time<0 || !(hit.time<limit))return RF_FORMAT;
    value.fraction=hit.time;status=scene_npc_hit_world(hit.point,&value,value.point);if(status)return status;
    status=scene_npc_body_contact_scalar(definition,pose,&value);if(status)return status;
    value.pose=pose;value.generation=pose->playback.generation;
    value.outcome=SCENE_BODY_CONTACT_HIT;value.matched=1;
    status=scene_npc_body_contact_reaction_prepare(shot,&value);if(status)return status;
    *result=value;return RF_OK;
 fallback:
    value.outcome=SCENE_BODY_CONTACT_FALLBACK;
    value.matched=combat_body(start,delta,&owner->body,limit,&value.fraction);
    if(value.matched)for(uint32_t i=0;i<3;i++)value.point[i]=start[i]+delta[i]*value.fraction;
    *result=value;return RF_OK;
}
static int scene_npc_body_contact_amount(const scene_npc_body_contact *contact,float base,float *result)
{
    float value=(float)((double)base*contact->factor);
    if(!isfinite(value) || value<0)return RF_RANGE;
    *result=value;return RF_OK;
}
#endif
