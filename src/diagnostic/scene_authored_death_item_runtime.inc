#ifndef RF_SCENE_AUTHORED_DEATH_ITEM_RUNTIME_INC
#define RF_SCENE_AUTHORED_DEATH_ITEM_RUNTIME_INC
/*4200c6 configured drop. The helper mutates a private receipt; final adjusted
 * position and class default quantity publish together in the actor ledger. */
typedef struct scene_death_item_context {
    scene_stream *stream;const scene_pickup_resource *resource;
    int32_t id;uint32_t owner,created,create_attempted;rf_entity_death_drop_item receipt;
} scene_death_item_context;
static int scene_death_item_query(void *context,const float start[3],const float delta[3],rf_entity_death_drop_hit *out)
{
    scene_death_item_context *c=context;rf_geometry_world_sweep_hit hit={0};uint32_t matched=0;int status;
    if(!c||!c->stream||!c->stream->collision||!out)return RF_RANGE;
    memset(out,0,sizeof(*out));
    status=rf_geometry_collision_world_sweep(c->stream->collision,0x2000u,start,delta,.15f,1,&hit,&matched);
    if(status)return status;if(!matched)return RF_OK;
    if(hit.hits>INT32_MAX)return RF_RANGE;
    out->count=(int32_t)hit.hits;memcpy(out->point,hit.hit.point,12);memcpy(out->normal,hit.hit.normal,12);
    return RF_OK;
}
static rf_entity_death_drop_item *scene_death_item_create(void *context,int32_t id,uint32_t owner,const float point[3])
{
    scene_death_item_context *c=context;rf_entity_death_drop_item *item=&c->receipt;
    c->create_attempted=1;if(id!=c->id||owner!=c->owner||c->created)return NULL;
    memset(item,0,sizeof(*item));item->model=(uint32_t)id;item->name=pickup_classes[scene_death_item_kind(id)];
    memcpy(item->position,point,12);memcpy(item->base_position,point,12);c->created=1;return item;
}
static int scene_death_item_bounds(void *context,uint32_t model,float *maximum_x)
{
    scene_death_item_context *c=context;uint32_t i;float value;
    if(!maximum_x||model!=(uint32_t)c->id||!c->resource->model.part_count)return RF_RANGE;
    /* Static503310 ->53c467 aggregates all source-part maximum bounds. */
    value=c->resource->model.parts[0].maximum[0];
    for(i=1;i<c->resource->model.part_count;i++)if(c->resource->model.parts[i].maximum[0]>value)
        value=c->resource->model.parts[i].maximum[0];
    if(!isfinite(value))return RF_RANGE;*maximum_x=value;return RF_OK;
}
static int scene_death_item_live_identity(const campaign_npc_body *owner)
{
    uint32_t actor,slot;int current,status;
    if(!owner||!campaign_npc_bodies||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS)return RF_RANGE;
    for(actor=0;actor<campaign_npc_body_count&&campaign_npc_bodies+actor!=owner;actor++){}
    if(actor==campaign_npc_body_count||!campaign_seeds.records.items||actor>=campaign_seeds.records.count||
       !owner->persistence_registered||owner->registration.view!=&owner->view||
       rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)!=&owner->view)return RF_FORMAT;
    slot=owner->persistence_slot;status=scene_death_item_current_slot(slot,&current);if(status)return status;
    if(!current||rf_scene_defeated_actors.items[slot].uid!=(uint32_t)campaign_seeds.records.items[actor].record.uid||
       !isfinite(owner->damage.effects.health)||owner->damage.effects.health>0||!(owner->view.flags_810&1u))return RF_FORMAT;
    return RF_OK;
}
static int scene_death_item_emit(scene_stream *stream,campaign_npc_body *owner)
{
    scene_death_item_context context={0};rf_entity_death_drop_source source={0};rf_entity_death_drop_item *created=NULL;
    rf_entity_death_drop_backend backend={scene_death_item_query,scene_death_item_create,scene_death_item_bounds,&context};
    int kind,status;uint32_t slot;
    if(!owner)return RF_RANGE;
    if(owner->death_item_attempted)return owner->death_item_fault;
    owner->death_item_attempted=1;
    if(owner->death.item_82c==-1)return RF_OK;
    kind=scene_death_item_kind(owner->death.item_82c);slot=owner->persistence_slot;
    if(kind<0||!stream||!stream->pickup_resources||!owner->persistence_registered||
       slot>=rf_scene_defeated_actors.count||!owner->registration.view||owner->damage.effects.health>0){status=RF_FORMAT;goto fail;}
    status=scene_death_item_live_identity(owner);if(status)goto fail;
    status=scene_death_item_owner_admit(owner,rf_scene_defeated_actors.item_drops+slot);if(status)goto fail;
    if(rf_scene_defeated_actors.item_drops[slot].state)return RF_OK;
    context.stream=stream;context.resource=stream->pickup_resources+kind-1;context.id=owner->death.item_82c;
    context.owner=owner->registration.handle;
    status=scene_death_item_resource_admit(context.resource,context.id);if(status)goto fail;
    source.item=context.id;source.handle=context.owner;source.extent_7c4=owner->death_item_extent_7c4;
    memcpy(source.position,owner->body.state.position,12);memcpy(source.owned,owner->inventory.owned,64);
    status=rf_entity_death_drop(&source,&backend,&created);if(status)goto fail;
    /* No hit or no owned weapon is the original legitimate absence. */
    if(!created){if(context.create_attempted){status=RF_FORMAT;goto fail;}return RF_OK;}
    if(created!=&context.receipt||!context.created){status=RF_FORMAT;goto fail;}
    status=scene_death_item_live_identity(owner);if(status)goto fail;
    if(owner->registration.handle!=context.owner||owner->persistence_slot!=slot||owner->death.item_82c!=context.id||
       rf_scene_defeated_actors.item_drops[slot].state){status=RF_FORMAT;goto fail;}
    status=rf_campaign_actor_item_drop_emit(&rf_scene_defeated_actors,slot,context.id,
        context.resource->definition.count,created->position);if(status)goto fail;
    return RF_OK;
fail:
    owner->death_item_fault=status;return status;
}
#endif
