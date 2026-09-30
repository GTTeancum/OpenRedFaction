/* Static entity owners, deliberately separate from skeletal NPC poses.
 * Include after scene_turret_models.inc, registries, base weapon declarations
 * and scene_npc_vitals_apply. Open only after model resources are ready.
 * This slice supplies identity, geometry and damage, not firing AI or saves. */
enum {SCENE_TURRET_OWNER_LIMIT=128};
typedef struct scene_turret_owner {
    uint32_t uid,seed,class_index,live_model,dead_model,model,dead,material;
    rf_entity_view view;rf_registered_entity_view registration;
    rf_entity_damage_state damage;float damage_factors[11];
    rf_entity_material surface;
    float position[3],basis[9];
    uint32_t target,fire_due; /* Reserved for the parent's ordinary AI binding. */
} scene_turret_owner;
static void scene_turret_death_effects_queue(scene_turret_owner *);
static scene_turret_owner *scene_turrets;
static uint32_t scene_turret_count;
uint32_t rf_scene_turret_owners[8]; /* count,bytes,damage calls,deaths,queries,hits,last UID,errors */
static scene_turret_owner *scene_turret_lookup(uint32_t handle)
{
    uint32_t i;void *registered=rf_object_registry_lookup(&campaign_registry,handle);
    if(!registered)return NULL;
    for(i=0;i<scene_turret_count;i++) {
        scene_turret_owner *o=scene_turrets+i;
        if(o->registration.view==&o->view && o->registration.handle==handle &&
           registered==&o->registration && rf_entity_lookup(&campaign_entities,(int32_t)handle)==&o->view)return o;
    }
    return NULL;
}
static uint32_t scene_turret_uid_handle(uint32_t uid)
{
    uint32_t i;
    for(i=0;i<scene_turret_count;i++)if(scene_turrets[i].uid==uid &&
        scene_turret_lookup(scene_turrets[i].registration.handle)==scene_turrets+i)return scene_turrets[i].registration.handle;
    return UINT32_MAX;
}
static int scene_turrets_close(void)
{
    uint32_t i;int status;
    /* Never free a wrapper still referenced by a mismatched registration. */
    for(i=0;i<scene_turret_count;i++)if(scene_turrets[i].registration.view &&
        scene_turret_lookup(scene_turrets[i].registration.handle)!=scene_turrets+i)return RF_FORMAT;
    for(i=0;i<scene_turret_count;i++)if(scene_turrets[i].registration.view) {
        status=rf_entity_view_unregister(&campaign_registry,&campaign_entities,&scene_turrets[i].registration);
        if(status)return status;
    }
    free(scene_turrets);scene_turrets=NULL;scene_turret_count=0;return RF_OK;
}
static int scene_turrets_open(rf_vpp *tables)
{
    rf_entity_physics_config configs[SCENE_TURRET_CLASS_LIMIT];uint32_t loaded=0,count=0,i,j,n=0;int status;
    if(!tables || scene_turrets || scene_turret_count)return RF_RANGE;
    if(campaign_seeds.records.count && (!campaign_seeds.records.items || !campaign_seeds.items || !campaign_seeds.classes))return RF_RANGE;
    memset(rf_scene_turret_owners,0,sizeof(rf_scene_turret_owners));
    for(i=0;i<campaign_seeds.records.count;i++)for(j=0;j<scene_turret_model_pair_count;j++)
        if(campaign_seeds.items[i].class_index==scene_turret_model_pairs[j].class_index){++count;break;}
    if(count>SCENE_TURRET_OWNER_LIMIT)return RF_RANGE;
    if(!count)return RF_OK;
    scene_turrets=calloc(count,sizeof(*scene_turrets));if(!scene_turrets)return RF_IO;scene_turret_count=count;
    for(i=0;i<campaign_seeds.records.count;i++) {
        uint32_t cls=campaign_seeds.items[i].class_index;
        const rf_entity_seed_class *definition;const rf_level_entity *record;
        rf_entity_creation_vitals_state vitals={0};rf_level_entity_vitals authored;scene_turret_owner *o;
        for(j=0;j<scene_turret_model_pair_count;j++)if(cls==scene_turret_model_pairs[j].class_index)break;
        if(j==scene_turret_model_pair_count)continue;
        if(cls>=campaign_seeds.class_count){status=RF_FORMAT;goto fail;}
        definition=campaign_seeds.classes+cls;record=&campaign_seeds.records.items[i].record;
        if(!(loaded&(1u<<j))) {
            status=rf_entity_physics_config_load(tables,record->class_name,512*1024,configs+j);if(status)goto fail;
            loaded|=1u<<j;
        }
        o=scene_turrets+n++;o->uid=(uint32_t)record->uid;o->seed=i;o->class_index=cls;
        if(o->uid==UINT32_MAX){status=RF_FORMAT;goto fail;}
        for(uint32_t prior=0;prior+1<n;prior++)if(scene_turrets[prior].uid==o->uid){status=RF_FORMAT;goto fail;}
        status=scene_turret_model_indices(cls,&o->live_model,&o->dead_model);if(status)goto fail;
        o->model=o->live_model;o->material=configs[j].material.index;o->surface=configs[j].material;
        for(uint32_t axis=0;axis<3;axis++)if(!isfinite(record->position[axis])){status=RF_FORMAT;goto fail;}
        memcpy(o->position,record->position,12);memcpy(o->basis,record->orientation,36);
        for(uint32_t axis=0;axis<9;axis++)if(!isfinite(o->basis[axis])){status=RF_FORMAT;goto fail;}
        vitals.object_flags=rf_entity_creation_object_flags(campaign_seeds.items[i].spawn.creation_flags,definition->model_kind);
        if(vitals.object_flags&0x4000u)vitals.object_flags|=0x8000u;
        vitals.object_flags|=0x06000000u;if(!(definition->physics.flags2&1u))vitals.object_flags|=0x20000u;
        rf_entity_creation_vitals(&vitals,&definition->vitals,0);
        status=rf_level_entity_vitals_read(campaign_seeds.records.items+i,&authored);if(status)goto fail;
        status=scene_npc_vitals_apply(&authored,&vitals,&definition->vitals);if(status)goto fail;
        o->damage.effects.health=vitals.health;o->damage.effects.armor=vitals.armor;
        o->damage.effects.class_health=definition->vitals.health;o->damage.effects.class_armor=definition->vitals.armor;
        o->damage.effects.class_flags_728=definition->physics.flags2;
        o->damage.effects.affiliation=campaign_seeds.items[i].spawn.friendliness;
        o->damage.effects.voice=o->damage.responsible_handle=o->damage.burn_source=UINT32_MAX;
        memcpy(o->damage_factors,definition->damage_factors,sizeof(o->damage_factors));
        o->view.handle=-1;o->view.type=0;o->view.class_type=(int32_t)definition->physics.use_kind;
        o->view.flags_7c=vitals.object_flags;o->view.linked_handle=-1;o->view.weapons[0]=o->view.weapons[1]=-1;
        if(campaign_seeds.items[i].spawn.endgame_if_killed)o->view.flags_810|=0x00400000u;
        if(campaign_base_motions.classes && cls<campaign_base_motions.class_count) {
            o->view.weapons[0]=campaign_base_motions.classes[cls].default_weapons.primary;
            o->view.weapons[1]=campaign_base_motions.classes[cls].default_weapons.secondary;
        }
        if(vitals.health<=0){o->dead=1;o->model=o->dead_model;o->view.flags_810|=1u;}
        status=rf_entity_view_register(&campaign_registry,&campaign_entities,&o->view,&o->registration);if(status)goto fail;
        o->damage.effects.handle=o->registration.handle;o->damage.effects.flags_810=o->view.flags_810;
    }
    rf_scene_turret_owners[0]=count;rf_scene_turret_owners[1]=count*sizeof(*scene_turrets);return RF_OK;
fail:
    ++rf_scene_turret_owners[7];(void)scene_turrets_close();return status;
}
static void scene_turret_world_vector(const float basis[9],const float local[3],float world[3])
{
    uint32_t i;for(i=0;i<3;i++)world[i]=(float)((double)basis[6+i]*local[2]+(double)basis[3+i]*local[1]+(double)basis[i]*local[0]);
}
static int scene_turret_target(uint32_t handle,float point[3],uint32_t *affiliation,uint32_t *alive)
{
    scene_turret_owner *o=scene_turret_lookup(handle);float pose[12],offset[3];int status;
    if(!point || !affiliation || !alive)return RF_RANGE;if(!o)return RF_NOT_FOUND;
    status=scene_turret_model_tag(o->live_model,"eye",o->position,o->basis,pose);
    if(status==RF_NOT_FOUND) {
        scene_turret_world_vector(o->basis,scene_turret_models[o->live_model].render.bound,offset);
        for(uint32_t i=0;i<3;i++)point[i]=o->position[i]+offset[i];
    }else if(status)return status;else memcpy(point,pose+9,12);
    *affiliation=o->damage.effects.affiliation;
    *alive=!o->dead && o->damage.effects.health>0 && !(o->view.flags_7c&(2u|0x4000u));return RF_OK;
}
typedef struct scene_turret_damage_context {
    scene_turret_owner *owner;rf_damage_object object;const rf_damage_effect_backend *effects;uint32_t clock;int status;
} scene_turret_damage_context;
static rf_damage_object *scene_turret_damage_lookup(void *context,uint32_t handle)
{
    scene_turret_damage_context *c=context;
    return handle==c->owner->registration.handle && scene_turret_lookup(handle)==c->owner?&c->object:NULL;
}
static uint32_t scene_turret_damage_predicate(void *context,uint32_t stage,uint32_t handle,const rf_damage_object *object)
{
    scene_turret_damage_context *c=context;(void)handle;(void)object;
    if(stage==0)return 1;
    if(stage==1)return rf_entity_armor_immunity(c->owner->damage.effects.armor,
        campaign_seeds.classes[c->owner->class_index].physics.flags,c->owner->damage.effects.flags_814);
    return 0; /* These owners have no player possession in this slice. */
}
static float scene_turret_damage_effect(void *context,rf_damage_object *object,float amount,uint32_t source,int32_t kind,uint32_t extra)
{
    scene_turret_damage_context *c=context;float result=0;
    c->status=rf_entity_damage_sp(&c->owner->damage,amount,kind,source,(int32_t)extra,
        kind<0?1:c->owner->damage_factors[kind],c->clock,c->effects,&result);
    object->health=c->owner->damage.effects.health;return result;
}
static int scene_turret_damage_receive(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock,const rf_damage_effect_backend *effects,float *result)
{
    scene_turret_owner *o=scene_turret_lookup(handle);scene_turret_damage_context c={0};
    rf_damage_backend backend={scene_turret_damage_lookup,scene_turret_damage_predicate,scene_turret_damage_effect,&c};
    float value=0;int status;
    if(!request || !effects || !result || request->kind< -1 || request->kind>10 ||
       !effects->predicate || !effects->resolve_uid || !effects->source || !effects->create_burn ||
       !effects->random || !effects->notify || !effects->playing || !effects->play_kind6 ||
       !isfinite(request->amount) || request->amount<0 || !isfinite(difficulty) || difficulty<0)return RF_RANGE;
    if(!o)return RF_NOT_FOUND;if(o->dead){*result=0;return RF_OK;}
    ++rf_scene_turret_owners[2];rf_scene_turret_owners[6]=o->uid;
    o->damage.effects.flags_810=o->view.flags_810;o->damage.effects.flags_814=o->view.flags_814;
    c.owner=o;c.effects=effects;c.clock=clock;c.object=(rf_damage_object){0,o->view.flags_7c,o->damage.effects.health};
    status=rf_damage_dispatch_sp(handle,request,difficulty,&backend,&value);
    o->view.flags_7c=c.object.flags;o->damage.effects.health=c.object.health;
    o->view.flags_810=o->damage.effects.flags_810;o->view.flags_814=o->damage.effects.flags_814;
    if(o->damage.effects.health<=0) {
        o->dead=1;o->model=o->dead_model;o->view.flags_810|=1u;o->damage.effects.flags_810=o->view.flags_810;
        o->target=o->fire_due=0;++rf_scene_turret_owners[3];
        scene_turret_death_effects_queue(o);
    }
    if(c.status)status=c.status;
    if(status){++rf_scene_turret_owners[7];return status;}*result=value;return RF_OK;
}
/* Exact retained static triangles, for hitscan (radius0) or swept projectiles.
 * Caller supplies the nearest world/actor hit so cover and tie order survive.
 * Returns full target identity; parent assigns its shared contact namespace. */
static int scene_turret_contact_query(uint32_t source,const float start[3],const float delta[3],float radius,float limit,
    uint32_t *handle,rf_collision_ray_hit *out,uint32_t *matched)
{
    uint32_t i,j,found=0,chosen=UINT32_MAX;rf_collision_ray_hit result={0};
    if(!start || !delta || !handle || !out || !matched || !isfinite(radius) || radius<0 ||
       !isfinite(limit) || limit<0 || limit>1)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(start[i]) || !isfinite(delta[i]))return RF_RANGE;
    for(i=0;i<scene_turret_count;i++) {
        scene_turret_owner *o=scene_turrets+i;const rf_model_collision_resource *geometry;
        rf_collision_model_part_query query={0};rf_collision_model_response_hit hit={0};float point[3],normal[3];uint32_t accepted;
        if(o->registration.handle==source || o->dead || (o->view.flags_7c&(2u|0x4000u)) ||
           scene_turret_lookup(o->registration.handle)!=o)continue;
        if(o->model>=scene_turret_model_count)return RF_FORMAT;
        geometry=&scene_turret_models[o->model].collision;
        memcpy(query.input.origin,o->position,12);memcpy(query.input.matrix,o->basis,36);
        memcpy(query.input.start,start,12);memcpy(query.input.displacement,delta,12);query.input.radius=radius;
        hit.time=limit;++rf_scene_turret_owners[4];
        accepted=rf_collision_model_trace(geometry->parts,&geometry->part_count,&query,&hit,0);
        if(!accepted || hit.time<0 || !(hit.time<limit))continue;
        scene_turret_world_vector(o->basis,hit.point,point);scene_turret_world_vector(o->basis,hit.normal,normal);
        result.fraction=hit.time;memcpy(result.normal,normal,12);
        for(j=0;j<3;j++)result.point[j]=point[j]+o->position[j];
        chosen=o->registration.handle;found=1;limit=hit.time;
        ++rf_scene_turret_owners[5];rf_scene_turret_owners[6]=o->uid;
    }
    *matched=found;if(found){*handle=chosen;*out=result;}return RF_OK;
}
