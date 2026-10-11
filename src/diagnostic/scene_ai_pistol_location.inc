#ifndef RF_SCENE_AI_PISTOL_LOCATION_INCLUDED
#define RF_SCENE_AI_PISTOL_LOCATION_INCLUDED
/* NPC-owned ordinary pistol:48be00 ->49a420 spheres ->42ce00 CSPH scalar.
 * No PLAYER triangle query, target560 invention, persistent shot or physics
 * publication. The existing finite instantaneous ray freezes target motion. */
typedef struct scene_ai_pistol_shot {
    uint64_t epoch;
    uint32_t active,source,slot,cls,affiliation,flags,intended,intended_slot,scripted,target,once;
    int32_t weapon,kind;
    float base,spread,start[3],basis[9],speed,radius,planes[4][4];
    campaign_single_fire_request single;
} scene_ai_pistol_shot;
typedef struct scene_ai_pistol_contact {
    scene_npc_body_contact body;
    const rf_physics_sphere *sphere_owner;uint32_t sphere_count;
    rf_physics_sphere spheres[3];
} scene_ai_pistol_contact;

static campaign_npc_body *scene_ai_pistol_identity(uint32_t slot,uint32_t handle)
{
    campaign_npc_body *o;
    if(!campaign_npc_bodies || slot>=campaign_npc_body_count)return NULL;
    o=campaign_npc_bodies+slot;
    if(o->registration.object_kind!=0 || o->view.type!=0 ||
       o->registration.handle!=handle || o->registration.view!=&o->view ||
       (uint32_t)o->view.handle!=handle || o->damage.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&o->view)return NULL;
    return o;
}
static campaign_npc_body *scene_ai_pistol_source(const scene_ai_pistol_shot *s)
{
    campaign_npc_body *o;
    if(!s->active || s->epoch!=scene_robot_death_epoch)return NULL;
    o=scene_ai_pistol_identity(s->slot,s->source);
    if(!o || !campaign_seeds.items || s->slot>=campaign_seeds.records.count ||
       campaign_seeds.items[s->slot].class_index!=s->cls ||
       (o->object_flags&8u) || o->view.linked_handle!=-1 ||
       o->damage.effects.affiliation!=s->affiliation)return NULL;
    /* Accepted shots do not require a still-living or still-equipped source. */
    return o;
}
static int scene_ai_pistol_capture(uint32_t slot,int32_t weapon,
    const campaign_enemy_weapon_selection *selected,float base,uint32_t intended,
    uint32_t intended_slot,uint32_t once,scene_ai_pistol_shot *out)
{
    scene_ai_pistol_shot s={0};campaign_npc_body *o;int32_t reset;uint32_t cls;const char *name;
    *out=s;
    if(weapon!=campaign_pistol_id || selected->slot!=0 || selected->melee ||
       selected->penetrates_world || selected->reserve_fed || !selected->ammo ||
       selected->primary!=campaign_primary || selected->primary->piercing_enabled)return RF_OK;
    if(!campaign_npc_bodies || slot>=campaign_npc_body_count || !campaign_seeds.items ||
       !campaign_seeds.records.items || slot>=campaign_seeds.records.count)return RF_RANGE;
    o=campaign_npc_bodies+slot;cls=campaign_seeds.items[slot].class_index;
    if(!campaign_seeds.classes || cls>=campaign_seeds.class_count)return RF_FORMAT;
    name=campaign_seeds.records.items[slot].record.class_name;
    if(rf_emitter_name_lookup(&name,1,"guard1")!=0 ||
       (o->object_flags&8u) || o->view.linked_handle!=-1 ||
       (campaign_seeds.classes[cls].physics.flags2&0x100u))return RF_OK;
    if(!scene_ai_pistol_identity(slot,o->registration.handle))return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || !scene_player_body_geometry_ready[0] ||
       rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[0])!=weapon ||
       !isfinite(base) || base<0 || selected->primary->damage_kind!=1)return RF_FORMAT;
    reset=rf_weapon_name_find(&campaign_weapon_reset.names,campaign_weapon_names[0]);
    if(reset<0 || (uint32_t)reset>=campaign_weapon_reset.names.count)return RF_FORMAT;
    s.epoch=scene_robot_death_epoch;s.active=1;s.source=o->registration.handle;s.slot=slot;s.cls=cls;
    s.affiliation=o->damage.effects.affiliation;s.flags=campaign_weapon_reset.definitions[reset].flags_268;
    s.weapon=weapon;s.kind=selected->primary->damage_kind;s.base=base;
    s.spread=selected->primary->ai_spread_degrees;s.speed=scene_player_body_geometry[0].speed;
    s.radius=scene_player_body_geometry[0].collision_radius;s.intended=intended;s.intended_slot=intended_slot;
    s.scripted=o->combat_scripted;s.target=o->combat_target;s.once=once;
    if(once){if(!o->script_once.count)return RF_FORMAT;s.single=o->script_once.requests[0];}
    memcpy(s.start,o->eye_position,12);memcpy(s.basis,o->look.orientation,36);
    if(!isfinite(s.speed) || s.speed<=0 || !isfinite(s.radius) || s.radius<=0 ||
       !isfinite(s.spread) || s.spread<0 || s.spread>90 || !(s.flags&0x20u))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(!isfinite(s.start[i]))return RF_FORMAT;
    for(uint32_t i=0;i<9;i++)if(!isfinite(s.basis[i]))return RF_FORMAT;
    *out=s;return RF_OK;
}
static int scene_ai_pistol_ray(scene_ai_pistol_shot *s,const float delta[3])
{
    rf_collision_projectile_plane_source planes;int status;
    if(s->spread>0){status=rf_entity_navigation_basis(delta,(float (*)[3])s->basis);if(status)return status;}
    memcpy(planes.position,s->start,12);memcpy(planes.basis,s->basis,36);planes.speed=s->speed;
    rf_collision_projectile_planes(&planes,s->planes);
    for(uint32_t i=0;i<16;i++)if(!isfinite(s->planes[i/4][i%4]))return RF_FORMAT;
    return RF_OK;
}
static void scene_ai_pistol_snapshot(uint32_t slot,scene_ai_pistol_contact *c)
{
    campaign_npc_body *o=campaign_npc_bodies+slot;scene_npc_body_contact *b=&c->body;
    memset(c,0,sizeof(*c));b->slot=slot;b->handle=o->registration.handle;b->owner=o;
    b->factor=1;b->fraction=1;b->index=UINT32_MAX;b->pain_action=-1;
    b->outcome=SCENE_BODY_CONTACT_FALLBACK;
    b->cls=campaign_seeds.items[slot].class_index;
    b->object_flags=o->object_flags;b->body_flags=o->body.state.flags;b->flags_810=o->view.flags_810;
    b->flags_814=o->damage.effects.flags_814;b->affiliation=o->damage.effects.affiliation;
    b->linked_handle=o->view.linked_handle;b->armor=o->damage.effects.armor;b->bounds=o->body.state.bounds;
    memcpy(b->position,o->body.state.position,12);memcpy(b->basis,o->body.state.orientation,36);
    memcpy(b->published,o->published,12);
}
static uint32_t scene_ai_pistol_player(uint32_t handle)
{
    return campaign_player_object.handle==handle && campaign_player_object.view==&campaign_player_view &&
        rf_object_registry_lookup(&campaign_registry,handle)==&campaign_player_object &&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&campaign_player_view &&
        scene_actor_body.allocated_bytes && !(campaign_player_view.flags_7c&(2u|0x4000u)) &&
        !(campaign_player_view.flags_810&1u) && campaign_player_damage.state.effects.health>0;
}
static uint32_t scene_ai_pistol_contact_live(const scene_ai_pistol_shot *s,const scene_ai_pistol_contact *c)
{
    const scene_npc_body_contact *b=&c->body;campaign_npc_body *o;rf_entity_pose *pose=NULL;
    if(!scene_ai_pistol_source(s))return 0; /* Epoch before every retained-pointer read. */
    if(!b->handle)return 1; /* No actor selected; cover may still consume. */
    if(b->slot==UINT32_MAX)return scene_ai_pistol_player(b->handle);
    o=scene_ai_pistol_identity(b->slot,b->handle);
    if(!o || o!=b->owner || !o->body.allocated_bytes || !(o->damage.effects.health>0) ||
       (o->object_flags&(2u|0x4000u)) || (o->view.flags_810&1u) ||
       !campaign_seeds.items || b->slot>=campaign_seeds.records.count ||
       campaign_seeds.items[b->slot].class_index!=b->cls ||
       o->object_flags!=b->object_flags || o->body.state.flags!=b->body_flags ||
       o->view.flags_810!=b->flags_810 || o->damage.effects.flags_814!=b->flags_814 ||
       o->damage.effects.affiliation!=b->affiliation || o->view.linked_handle!=b->linked_handle ||
       o->damage.effects.armor!=b->armor || memcmp(o->body.state.position,b->position,12) ||
       memcmp(o->body.state.orientation,b->basis,36) || memcmp(o->published,b->published,12) ||
       memcmp(&o->body.state.bounds,&b->bounds,sizeof(b->bounds)))return 0;
    if(b->outcome!=SCENE_BODY_CONTACT_HIT)return 1;
    if(b->reaction_frame && (!campaign_model_owners || b->slot>=campaign_model_owner_count ||
       memcmp(campaign_model_owners[b->slot].basis,b->published_basis,36)))return 0;
    if(!campaign_seeds.classes || b->cls>=campaign_seeds.class_count ||
       campaign_seeds.classes[b->cls].physics.flags!=b->class_flags ||
       !scene_npc_hit_location_classes || !scene_npc_hit_location_classes[b->cls].enabled ||
       scene_npc_hit_location_classes[b->cls].skeleton!=b->skeleton ||
       o->body.spheres.items!=c->sphere_owner || o->body.spheres.count!=c->sphere_count ||
       memcmp(o->body.spheres.items,c->spheres,c->sphere_count*sizeof(*c->spheres)) ||
       campaign_model_pose(b->slot,&pose) || pose!=b->pose || !pose ||
       pose->skeleton!=b->skeleton || pose->playback.generation!=b->generation)return 0;
    return 1;
}
/* Normal12mm branch4c5e93..4c5fbd: published3c/48 reaction frame,
 * separate from physicale4/fc contact. Pistol binding872114 is not one of
 *4c5d95..4c5dcb's exceptional descriptors. Only828 is represented here. */
static int scene_ai_pistol_reaction_prepare(scene_npc_body_contact *b)
{
    float difference[3],side;uint32_t axis;
    b->pain_action=-1;b->reaction_frame=0;
    if(b->outcome!=SCENE_BODY_CONTACT_HIT || !(b->class_flags&0x20000u) ||
       (b->flags_810&1u) || b->index>1)return RF_OK;
    if(!campaign_model_owners || b->slot>=campaign_model_owner_count)return RF_FORMAT;
    memcpy(b->published_basis,campaign_model_owners[b->slot].basis,36);
    for(uint32_t i=0;i<9;i++)if(!isfinite(b->published_basis[i]))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++) {
        difference[i]=(float)((double)b->point[i]-b->published[i]);
        if(!isfinite(difference[i]))return RF_RANGE;
    }
    axis=b->index==1?6u:0u;
    side=(float)(((double)difference[2]*b->published_basis[axis+2]+
        (double)difference[1]*b->published_basis[axis+1])+
        (double)difference[0]*b->published_basis[axis]);
    if(!isfinite(side))return RF_RANGE;
    b->pain_action=b->index==1?(side>0?24:25):(side>0?27:26);
    b->reaction_frame=1;return RF_OK;
}
/* Response velocity is unused. This pure callback cannot dispatch gameplay. */
static const float *scene_ai_pistol_no_velocity(void *context,uint32_t handle)
{(void)context;(void)handle;return NULL;}
static int scene_ai_pistol_body(uint32_t slot,const scene_ai_pistol_shot *s,
    const float delta[3],scene_ai_pistol_contact *c)
{
    campaign_npc_body *o;scene_npc_body_contact *b=&c->body;
    const scene_npc_hit_location_class *d;rf_entity_pose *pose=NULL;
    rf_collision_projectile_eligibility eligibility={0};int status;
    if(!scene_ai_pistol_source(s))return RF_NOT_FOUND;
    if(!campaign_seeds.items || slot>=campaign_seeds.records.count)return RF_RANGE;
    scene_ai_pistol_snapshot(slot,c);o=campaign_npc_bodies+slot;
    if(!campaign_seeds.classes || b->cls>=campaign_seeds.class_count)return RF_FORMAT;
    b->class_flags=campaign_seeds.classes[b->cls].physics.flags;
    if(!scene_hit_location_class_named(campaign_seeds.records.items[slot].record.class_name))goto fallback;
    if(b->handle==s->source){b->outcome=SCENE_BODY_CONTACT_REJECT;return RF_OK;}
    {const rf_entity_view *linked=rf_entity_lookup(&campaign_entities,b->linked_handle);
     if(linked && linked->class_type==1){b->outcome=SCENE_BODY_CONTACT_REJECT;return RF_OK;}}
    if(!scene_npc_hit_location_classes || !scene_npc_hit_location_classes[b->cls].enabled)return RF_FORMAT;
    d=scene_npc_hit_location_classes+b->cls;
    /* The unsupported0/0 case is prior policy, never a guessed target560.
     * Linked bodies include the separate4c5a52 owner and48aaf0 miss overlaps. */
    if((!s->affiliation && !b->affiliation) || b->linked_handle!=-1 ||
       (b->object_flags&8u) || (b->body_flags&0x400u) ||
       (campaign_player_object.view && campaign_player_view.linked_handle==(int32_t)b->handle) ||
       rf_entity_armor_immunity(b->armor,b->class_flags,b->flags_814))goto fallback;
    if(!(b->class_flags&0x20000u) || d->count!=3 || o->body.spheres.count!=3 ||
       !o->body.spheres.items || !isfinite(o->body.state.mass) || o->body.state.mass<=0)return RF_FORMAT;
    memcpy(eligibility.projectile_position,s->start,12);memcpy(eligibility.projectile_forward,s->basis+6,12);
    memcpy(eligibility.target_position,b->published,12);memcpy(eligibility.target_next_position,b->position,12);
    eligibility.projectile_extent=s->radius;eligibility.target_extent=b->bounds.radius;
    eligibility.definition_flags=s->flags;eligibility.target_kind=0;eligibility.target_field_1f8=b->affiliation;
    eligibility.target_handle=b->handle;eligibility.owner_present=1;eligibility.owner_field_1f8=s->affiliation;
    eligibility.owner_target_560=UINT32_MAX;eligibility.mode=1;memcpy(eligibility.planes,s->planes,sizeof(eligibility.planes));
    if(!isfinite(b->bounds.radius) || b->bounds.radius<0)return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(!isfinite(b->published[i]) || !isfinite(b->position[i]))return RF_FORMAT;
    if(!rf_collision_projectile_eligible(&eligibility)){b->outcome=SCENE_BODY_CONTACT_REJECT;return RF_OK;}
    b->outcome=SCENE_BODY_CONTACT_MISS;c->sphere_owner=o->body.spheres.items;c->sphere_count=3;
    memcpy(c->spheres,o->body.spheres.items,sizeof(c->spheres));
    {
        rf_collision_actor_general_response p={0},t={0};rf_physics_sphere sphere={0};
        sphere.radius=s->radius;sphere.parameter_10=-1;
        p.kind=2;p.extent=s->radius;p.actor.spheres=&sphere;p.actor.sphere_count=1;p.actor.contact.time=1;
        /* Factory4c7934 default flags; this actual pistol adds no later tweak.
         * One sphere < three and kind2, with neither body400, prove that only
         * target mass is read (reciprocal branch suppressed49a9af). This is a
         * deliberately narrower use than the general API positive-masses
         * contract: unused source mass remains0, not an invented real mass. */
        p.actor.body_flags=0x80000870u;
        t.kind=0;t.extent=b->bounds.radius;t.actor.handle=b->handle;t.actor.mass=o->body.state.mass;
        t.actor.body_flags=b->body_flags;t.actor.spheres=c->spheres;t.actor.sphere_count=3;t.actor.contact.time=1;
        memcpy(p.orientation,s->basis,36);memcpy(p.next_orientation,s->basis,36);
        memcpy(t.orientation,b->basis,36);memcpy(t.next_orientation,b->basis,36);
        for(uint32_t i=0;i<9;i++)if(!isfinite(b->basis[i]))return RF_FORMAT;
        for(uint32_t i=0;i<3;i++) {
            float end=(float)((double)s->start[i]+delta[i]);
            if(!isfinite(delta[i]) || !isfinite(end) || !isfinite(b->bounds.minimum[i]) ||
               !isfinite(b->bounds.maximum[i]))return RF_FORMAT;
            p.actor.position[i]=s->start[i];p.actor.next_position[i]=end;
            p.actor.minimum[i]=(float)((double)(s->start[i]<end?s->start[i]:end)-s->radius);
            p.actor.maximum[i]=(float)((double)(s->start[i]>end?s->start[i]:end)+s->radius);
            if(!isfinite(p.actor.minimum[i]) || !isfinite(p.actor.maximum[i]))return RF_RANGE;
            t.actor.position[i]=t.actor.next_position[i]=b->position[i];
            t.actor.minimum[i]=b->bounds.minimum[i];t.actor.maximum[i]=b->bounds.maximum[i];
        }
        for(uint32_t n=0;n<3;n++) {
            if(!isfinite(c->spheres[n].radius) || c->spheres[n].radius<=0)return RF_FORMAT;
            for(uint32_t k=0;k<3;k++)if(!isfinite(c->spheres[n].center[k]))return RF_FORMAT;
        }
        if(p.kind!=2 || p.actor.sphere_count!=1 || t.actor.sphere_count!=3 ||
           p.actor.sphere_count>=t.actor.sphere_count ||
           ((p.actor.body_flags|t.actor.body_flags)&0x400u))return RF_FORMAT;
        if(!rf_collision_actors_general_response(&p,&t,scene_ai_pistol_no_velocity,NULL))return RF_OK;
        if(!isfinite(p.actor.contact.time) || p.actor.contact.time<0 || p.actor.contact.time>=1 ||
           p.actor.contact.handle!=b->handle)return RF_FORMAT;
        b->fraction=p.actor.contact.time;memcpy(b->point,p.actor.contact.point,12);
        for(uint32_t i=0;i<3;i++)if(!isfinite(b->point[i]))return RF_RANGE;
    }
    status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || pose->skeleton!=d->skeleton || !campaign_render_models.items ||
       pose->skeleton>=campaign_render_models.count)return RF_FORMAT;
    status=campaign_model_pose_ready(slot,pose);if(status)return status;
    b->skeleton=d->skeleton;b->pose=pose;b->generation=pose->playback.generation;
    status=scene_npc_body_contact_scalar(d,pose,b);if(status)return status;
    b->outcome=SCENE_BODY_CONTACT_HIT;b->matched=1;
    return scene_ai_pistol_reaction_prepare(b);
 fallback:
    b->matched=combat_body(s->start,delta,&o->body,1,&b->fraction);
    if(b->matched)for(uint32_t i=0;i<3;i++)b->point[i]=s->start[i]+delta[i]*b->fraction;
    return RF_OK;
}
static int scene_ai_pistol_actor_select(scene_stream *stream,const scene_ai_pistol_shot *s,
    const float delta[3],const float player_eye[3],scene_ai_actor_ray_selection *out,scene_ai_pistol_contact *receipt)
{
    scene_ai_actor_ray_selection selected={0};scene_ai_pistol_contact candidate;
    float end[3];int status;
    memset(receipt,0,sizeof(*receipt));selected.victim_slot=UINT32_MAX;selected.fraction=selected.body_fraction=1;
    if(!scene_ai_pistol_source(s))return RF_NOT_FOUND;
    for(uint32_t i=0;i<3;i++){end[i]=s->start[i]+delta[i];if(!isfinite(end[i]))return RF_RANGE;}
    if(scene_ai_pistol_player(campaign_player_object.handle) && !scene_ai_actor_ray_related(s->source,campaign_player_object.handle)) {
        selected.body_hit=combat_body(s->start,delta,&scene_actor_body,1,&selected.body_fraction);
        if(!selected.body_hit)selected.body_fraction=1;
        selected.actor_hit=selected.body_hit;selected.fraction=selected.body_fraction;
        status=scene_player_shield_query(stream,player_eye,actor_look.eye_orientation,s->start,end,1,
            &selected.player_shield,&selected.shield_hit);if(status)return status;
        if(selected.shield_hit){selected.actor_hit=1;selected.fraction=selected.player_shield.hit.time;}
        if(selected.actor_hit){receipt->body.slot=UINT32_MAX;receipt->body.handle=campaign_player_object.handle;receipt->body.factor=1;}
    }
    for(uint32_t i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *o=campaign_npc_bodies+i;scene_ai_shield_ray_selection shield={0};
        if(!scene_ai_pistol_identity(i,o->registration.handle) || !o->body.allocated_bytes ||
           !(o->damage.effects.health>0) || (o->object_flags&(2u|0x4000u)) || (o->view.flags_810&1u) ||
           scene_ai_actor_ray_related(s->source,o->registration.handle))continue;
        status=scene_ai_pistol_body(i,s,delta,&candidate);if(status)return status;
        /* Original body rejection does not make an independently held shield
         * disappear. Only that shield may win; it carries body_hit false. */
        status=scene_ai_shield_ray_select(i,s->start,end,candidate.body.matched,candidate.body.fraction,&shield);if(status)return status;
        if(shield.kind==SCENE_AI_SHIELD_RAY_MISS || (selected.actor_hit && shield.fraction>=selected.fraction))continue;
        selected.victim=o;selected.victim_slot=i;selected.actor_hit=1;selected.body_hit=candidate.body.matched;
        selected.body_fraction=candidate.body.fraction;selected.fraction=shield.fraction;
        selected.npc_shield=shield;selected.shield_hit=0;memset(&selected.player_shield,0,sizeof(selected.player_shield));
        *receipt=candidate;
    }
    if(!scene_ai_pistol_contact_live(s,receipt))return RF_NOT_FOUND;
    *out=selected;return RF_OK;
}
#endif
