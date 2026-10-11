#ifndef RF_SCENE_MEDIC_FLIGHT_VISIBILITY_INCLUDED
#define RF_SCENE_MEDIC_FLIGHT_VISIBILITY_INCLUDED
/*402180's two4991c0 calls over existing admitted scene owners. Private
 * synchronous views only: no model loads, static-vehicle approximations,
 * persistent snapshot cache, RNG, sight deadline or action mutation. */
typedef struct scene_flight_visibility {
    scene_stream *stream;campaign_npc_body *owner;
    const scene_medic_flight_threat_view *threat;
    rf_collision_visibility_object *objects;uint32_t count,actors,clutter,corpses;
    uint32_t registry_generation;
} scene_flight_visibility;
static int scene_flight_visibility_geometry_admit(const rf_collision_visibility_object *v)
{
    if(!isfinite(v->extent)||v->extent<0||!scene_flight_finite3(v->position)||
       !scene_flight_finite3(v->minimum)||!scene_flight_finite3(v->maximum))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(v->minimum[i]>v->maximum[i])return RF_FORMAT;
    /* Held syringe hand composition is raw finite skeletal output. */
    for(uint32_t i=0;i<9;i++)if(!isfinite(v->matrix[i]))return RF_FORMAT;
    return RF_OK;
}
static int scene_flight_npc_visibility_view(uint32_t slot,rf_collision_visibility_object *out)
{
    rf_collision_visibility_object value={0};const campaign_npc_body *o;int status;
    if(slot>=campaign_npc_body_count||!out)return RF_RANGE;o=campaign_npc_bodies+slot;
    if(!scene_flight_owner_live(o))return RF_NOT_FOUND;
    value.token=o->registration.handle;value.flags=o->object_flags;
    /*499282 reads180, not78;4992f4/310 use physical e4/fc, not published3c.
     * Missing posed storage remains a capability fact until source filters. */
    if(slot<campaign_model_owner_count&&campaign_model_owners&&campaign_model_owners[slot].registration.loaded&&
       campaign_model_owners[slot].pose&&!campaign_model_owners[slot].owned)
        value.model=&campaign_model_owners[slot].registration;
    value.extent=o->body.state.bounds.radius;
    memcpy(value.position,o->body.state.position,12);memcpy(value.matrix,o->body.state.orientation,36);
    memcpy(value.minimum,o->body.state.bounds.minimum,12);memcpy(value.maximum,o->body.state.bounds.maximum,12);
    status=scene_flight_visibility_geometry_admit(&value);if(status)return status;
    *out=value;return RF_OK;
}
/* The port's base-object list contains type4 clutter and type10 glares.
 * Project only known type4 owners, preserving actual insertion order. */
static int scene_flight_clutter_visibility_view(const rf_object_link *link,
    rf_collision_visibility_object *out,const rf_model_collision_resource **geometry,uint32_t *family)
{
    const rf_clutter_base_owner *o=NULL;const void *model=NULL;
    const rf_model_collision_resource *resource=NULL;rf_collision_visibility_object value={0};int status;
    if(!link||!out||!geometry||!family)return RF_RANGE;
    *family=0;*geometry=NULL;
    for(uint32_t i=0;i<campaign_clutter_records.count;i++)if(campaign_clutter_bodies&&campaign_clutter_bodies[i]&&
        link==&campaign_clutter_bodies[i]->object_link){
        uint32_t slot;if(!campaign_clutter_model_slots)return RF_FORMAT;
        slot=campaign_clutter_model_slots[i];
        if(slot>=campaign_clutter_model_count||!campaign_clutter_models||!campaign_clutter_shared)return RF_FORMAT;
        o=campaign_clutter_bodies[i];model=campaign_clutter_shared+slot;
        resource=&campaign_clutter_models[slot].collision;break;
    }
    if(!o)for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const rf_clutter_base_owner *child=campaign_npc_bodies[i].syringe.child;
        if(child&&link==&child->object_link){
            if(!scene_medic_syringe_child_valid(child))return RF_NOT_FOUND;
            o=child;model=&scene_medic_syringe_resource.shared;
            resource=&scene_medic_syringe_resource.model.collision;break;
        }
    }
    if(!o){
        for(uint32_t i=0;i<campaign_glare_instance_count;i++)if(campaign_glare_instances&&campaign_glare_instances[i]&&
            link==&campaign_glare_instances[i]->object_link){
            const rf_glare_base_owner *glare=campaign_glare_instances[i];
            if(glare->kind!=10||rf_object_registry_lookup(&campaign_registry,glare->handle)!=&glare->state)return RF_NOT_FOUND;
            *family=2;return RF_OK; /* Separate original glare family, never type4. */
        }
        return RF_OK; /* Unknown base-list ownership makes flight unsupported. */
    }
    if(rf_object_registry_lookup(&campaign_registry,o->state.handle)!=&o->state||
       o->attachment.model!=(uint32_t)(uintptr_t)model)return RF_NOT_FOUND;
    value.token=o->state.handle;value.flags=o->state.flags;value.model=model;
    value.extent=o->body.state.bounds.radius;
    memcpy(value.position,o->body.state.position,12);memcpy(value.matrix,o->body.state.orientation,36);
    memcpy(value.minimum,o->body.state.bounds.minimum,12);memcpy(value.maximum,o->body.state.bounds.maximum,12);
    status=scene_flight_visibility_geometry_admit(&value);if(status)return status;
    *out=value;*geometry=resource;*family=1;return RF_OK;
}
static int scene_flight_clutter_by_token(uint32_t token,rf_collision_visibility_object *out,
    const rf_model_collision_resource **geometry)
{
    const rf_object_link *link=campaign_clutter_objects.sentinel.next,*previous=&campaign_clutter_objects.sentinel;
    uint32_t visited=0;
    while(link&&link!=&campaign_clutter_objects.sentinel){
        rf_collision_visibility_object value;const rf_model_collision_resource *resource;uint32_t family;int status;
        if(++visited>RF_OBJECT_CAPACITY)return RF_FORMAT;
        status=scene_flight_clutter_visibility_view(link,&value,&resource,&family);if(status)return status;
        if(!family)return RF_NOT_FOUND;
        if(link->previous!=previous)return RF_FORMAT;
        if(family==1&&value.token==token){*out=value;*geometry=resource;return RF_OK;}
        previous=link;link=link->next;
    }
    return RF_NOT_FOUND;
}
static int scene_flight_corpse_view(uint32_t slot,rf_collision_visibility_object *out)
{
    const rf_corpse_owned *owned;const rf_corpse *corpse;const campaign_model_owner *model;
    rf_collision_visibility_object value={0};uint32_t index;int status;
    if(slot>=RF_CORPSE_CAPACITY||!out)return RF_RANGE;
    owned=campaign_live_corpses.slots+slot;corpse=&owned->corpse;
    if(owned->construction!=RF_CORPSE_CONSTRUCT_COMPLETE||corpse->deletion.lifecycle||
       rf_object_registry_lookup(&campaign_registry,corpse->deletion.handle)!=corpse->deletion.registered_object||
       corpse->deletion.registered_object!=corpse)return RF_NOT_FOUND;
    value.token=corpse->deletion.handle;value.flags=corpse->update.fade.object_flags_7c;
    value.extent=owned->body.state.bounds.radius;
    memcpy(value.position,owned->body.state.position,12);memcpy(value.matrix,owned->body.state.orientation,36);
    memcpy(value.minimum,owned->body.state.bounds.minimum,12);memcpy(value.maximum,owned->body.state.bounds.maximum,12);
    if(corpse->update.model){
        index=corpse->update.model-1;if(index>=campaign_model_owner_count)return RF_FORMAT;
        if(campaign_model_owners){
            model=campaign_model_owners+index;
            if(model->owned&&model->registration.loaded&&model->pose)value.model=&model->registration;
        }
    }
    status=scene_flight_visibility_geometry_admit(&value);if(status)return status;
    *out=value;return RF_OK;
}
static int scene_flight_visibility_extent(const campaign_npc_body *owner,
    const scene_medic_flight_threat_view *threat,float *out)
{
    float extent;
    if(!owner||!threat||!out)return RF_RANGE;
    if(!isfinite(owner->model_radius_78)||!isfinite(threat->model_radius)||
       owner->model_radius_78<0||threat->model_radius<0)return RF_FORMAT;
    extent=(float)((double)(owner->model_radius_78>threat->model_radius?owner->model_radius_78:threat->model_radius)+(double).1f);
    if(!isfinite(extent))return RF_FORMAT;*out=extent;return RF_OK;
}
static uint32_t scene_flight_visibility_candidate(const rf_collision_visibility_object *v,uint32_t family,
    float extent,uint32_t first,uint32_t second)
{
    return !(v->flags&0x4000u)&&!(family!=1&&(v->flags&2u))&&
        v->token!=first&&v->token!=second&&v->extent>=extent;
}
static uint32_t scene_flight_posed_capable(uint32_t slot,uint32_t owned)
{
    const campaign_model_owner *model;const rf_entity_pose *pose;const rf_entity_collision_cache *cache;
    if(slot>=campaign_model_owner_count||!campaign_model_owners||!campaign_collision_caches||
       !campaign_collision_models.items||!campaign_render_models.items)return 0;
    model=campaign_model_owners+slot;pose=model->pose;cache=campaign_collision_caches+slot;
    if(!model->registration.loaded||!!model->owned!=owned||!pose||
       !model->registration.next||!model->registration.previous||model->registration.active!=&pose->playback.completion.active||
       !pose->bone_count||pose->bone_count>50||!pose->matrices||!pose->generations||
       !cache->matrices||!cache->generations||cache->bone_count!=pose->bone_count||cache->skeleton!=pose->skeleton||
       pose->skeleton>=campaign_collision_models.count||pose->skeleton>=campaign_render_models.count||
       !campaign_render_models.items[pose->skeleton].stored)return 0;
    return !campaign_collision_models.items[pose->skeleton].batch_count||
        (campaign_collision_models.items[pose->skeleton].batches&&campaign_model_query_scratch);
}
static int scene_medic_flight_visibility_admit(scene_stream *stream,const campaign_npc_body *owner,
    const scene_medic_flight_threat_view *threat,uint32_t *supported)
{
    uint64_t capacity=(uint64_t)campaign_npc_body_count+campaign_clutter_objects.count+RF_CORPSE_CAPACITY;
    float extent;int status;
    if(!stream||!stream->collision||!owner||!threat||!supported)return RF_RANGE;
    *supported=0;
    if(!scene_flight_owner_live(owner)||!scene_flight_threat_live(threat))return RF_NOT_FOUND;
    /* Same bounded temporary observation storage, admitted before action/RNG.
     * A later real calloc failure is still RF_IO, never an ordinary miss. */
    if(capacity>128*1024/sizeof(rf_collision_visibility_object))return RF_OK;
    status=scene_flight_visibility_extent(owner,threat,&extent);if(status)return status;
    /* Passive wrappers have real flags/identity but no retained physical180.
     * Apply proved source exclusions first; never invent a render-radius180. */
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *p=campaign_passive_vehicles+i;
        if(rf_object_registry_lookup(&campaign_registry,p->handle)!=p||
           (p->damage.object_flags&(2u|0x4000u))||p->handle==owner->registration.handle||p->handle==threat->handle)continue;
        return RF_OK;
    }
    for(uint32_t i=0;i<RF_OBJECT_SLOTS;i++){
        const rf_entity_view *view=campaign_entities.slots[i];uint32_t known=0;
        if(!view)continue;
        if(view==&campaign_player_view&&campaign_player_object.view==view&&
           rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==view&&
           rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object)continue;
        for(uint32_t j=0;j<campaign_npc_body_count;j++)if(view==&campaign_npc_bodies[j].view){
            rf_collision_visibility_object object;
            status=scene_flight_npc_visibility_view(j,&object);if(status)return status;
            if(scene_flight_visibility_candidate(&object,0,extent,owner->registration.handle,threat->handle)&&
               !scene_flight_posed_capable(j,0))return RF_OK;
            known=1;break;
        }
        if(!known){
            if(view->flags_7c&(2u|0x4000u))continue;
            if((uint32_t)view->handle==owner->registration.handle||(uint32_t)view->handle==threat->handle)continue;
            /* Foreign compact views lack a qualified physical180/model view. */
            return RF_OK;
        }
    }
    {
        const rf_object_link *link=campaign_clutter_objects.sentinel.next,*previous=&campaign_clutter_objects.sentinel;
        uint32_t visited=0;
        if(!link)return campaign_clutter_objects.count?RF_FORMAT:RF_OK;
        while(link!=&campaign_clutter_objects.sentinel){
            rf_collision_visibility_object object;const rf_model_collision_resource *geometry;uint32_t family;
            if(++visited>RF_OBJECT_CAPACITY)return RF_FORMAT;
            status=scene_flight_clutter_visibility_view(link,&object,&geometry,&family);if(status)return status;
            if(!family)return RF_OK;
            if(link->previous!=previous)return RF_FORMAT;
            if(family==1&&scene_flight_visibility_candidate(&object,1,extent,owner->registration.handle,threat->handle)&&
               (!geometry||!geometry->accounted_bytes||(geometry->part_count&&!geometry->parts)))return RF_OK;
            previous=link;link=link->next;if(!link)return RF_FORMAT;
        }
        if(previous!=campaign_clutter_objects.sentinel.previous||visited!=campaign_clutter_objects.count)return RF_FORMAT;
    }
    {
        const rf_corpse_list_link *link=campaign_live_corpse_head.next,*previous=&campaign_live_corpse_head;
        uint32_t visited=0;
        if(!link){if(campaign_live_corpse_count)return RF_FORMAT;}
        else while(link!=&campaign_live_corpse_head){
            uint32_t slot;rf_collision_visibility_object object;
            if(++visited>RF_CORPSE_CAPACITY)return RF_FORMAT;
            for(slot=0;slot<RF_CORPSE_CAPACITY;slot++)if(link==&campaign_live_corpses.slots[slot].corpse.deletion.corpse_link)break;
            if(slot==RF_CORPSE_CAPACITY||link->previous!=previous)return RF_FORMAT;
            status=scene_flight_corpse_view(slot,&object);if(status)return status;
            if(scene_flight_visibility_candidate(&object,2,extent,owner->registration.handle,threat->handle)){
                uint32_t model=campaign_live_corpses.slots[slot].corpse.update.model;
                if(!model||!scene_flight_posed_capable(model-1,1))return RF_OK;
            }
            previous=link;link=link->next;if(!link)return RF_FORMAT;
        }
        if(visited!=campaign_live_corpse_count||(link&&campaign_live_corpse_head.previous!=previous))return RF_FORMAT;
    }
    *supported=1;return RF_OK;
}
static int scene_flight_visibility_stable(const scene_flight_visibility *c)
{
    uint32_t supported;int status;
    if(!scene_flight_owner_live(c->owner)||!scene_flight_threat_live(c->threat)||
       campaign_registry.generation!=c->registry_generation)return RF_NOT_FOUND;
    status=scene_medic_flight_visibility_admit(c->stream,c->owner,c->threat,&supported);
    if(status)return status;return supported?RF_OK:RF_NOT_FOUND;
}
static int scene_flight_visibility_model(void *context,const rf_collision_visibility_object *object,
    rf_collision_model_part_query *query,rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *accepted)
{
    scene_flight_visibility *c=context;uint32_t index=UINT32_MAX;int status;
    if(!object||!query||!hit||!accepted)return RF_RANGE;
    for(uint32_t i=0;i<c->count;i++)if(object==c->objects+i){index=i;break;}
    if(index==UINT32_MAX)return RF_RANGE;
    status=scene_flight_visibility_stable(c);if(status)return status;
    if(index<c->actors){
        rf_collision_visibility_object current;uint32_t slot;
        for(slot=0;slot<campaign_npc_body_count;slot++)if(campaign_npc_bodies[slot].registration.view&&
            campaign_npc_bodies[slot].registration.handle==object->token)break;
        if(slot==campaign_npc_body_count)return RF_NOT_FOUND;
        status=scene_flight_npc_visibility_view(slot,&current);if(status)return status;
        if(memcmp(&current,object,sizeof(*object)))return RF_NOT_FOUND;
        if(!object->model)return RF_NOT_FOUND;
        status=rf_scene_npc_visibility_model(NULL,object,query,hit,reset,accepted);
        if(!status){status=scene_flight_npc_visibility_view(slot,&current);if(!status&&memcmp(&current,object,sizeof(current)))status=RF_NOT_FOUND;}
    }else if(index<c->actors+c->clutter){
        rf_collision_visibility_object current;const rf_model_collision_resource *geometry,*again;
        rf_collision_model_query_view view={0};rf_collision_model_query_backend backend={campaign_clutter_query_geometry,NULL};
        status=scene_flight_clutter_by_token(object->token,&current,&geometry);if(status)return status;
        if(memcmp(&current,object,sizeof(*object))||!geometry)return RF_NOT_FOUND;
        /*4991c0 does not skip clutter flag2. The ordinary damage collision
         * wrapper does, so bind the real owner's immutable geometry directly. */
        view.kind=1;view.geometry=geometry;
        *accepted=rf_collision_model_query_all(&view,query,hit,reset,&backend);
        status=scene_flight_clutter_by_token(object->token,&current,&again);
        if(!status&&(memcmp(&current,object,sizeof(current))||again!=geometry))status=RF_NOT_FOUND;
    }else{
        uint32_t slot;rf_collision_visibility_object current;
        for(slot=0;slot<RF_CORPSE_CAPACITY;slot++)if(campaign_live_corpses.slots[slot].corpse.deletion.handle==object->token&&
            !campaign_live_corpses.slots[slot].corpse.deletion.lifecycle&&
            campaign_live_corpses.slots[slot].construction==RF_CORPSE_CONSTRUCT_COMPLETE)break;
        if(slot==RF_CORPSE_CAPACITY)return RF_NOT_FOUND;
        status=scene_flight_corpse_view(slot,&current);if(status)return status;
        if(memcmp(&current,object,sizeof(current))||!object->model)return RF_NOT_FOUND;
        status=rf_scene_model_collision_query(campaign_live_corpses.slots[slot].corpse.update.model-1,
            query,hit,reset,accepted);
        if(!status){status=scene_flight_corpse_view(slot,&current);if(!status&&memcmp(&current,object,sizeof(current)))status=RF_NOT_FOUND;}
    }
    if(status)return status;return scene_flight_visibility_stable(c);
}
static int scene_flight_visibility_world(void *context,const float start[3],const float end[3],
    uint32_t flags,const void *optional,uint32_t *blocked)
{
    scene_flight_visibility *c=context;int status;
    if(flags!=0x27u||optional)return RF_RANGE;
    status=scene_flight_visibility_stable(c);if(status)return status;
    status=rf_geometry_collision_ray(c->stream->collision,&campaign_movers,start,end,flags,NULL,blocked);
    if(status)return status;return scene_flight_visibility_stable(c);
}
static int scene_flight_visibility_fill(scene_flight_visibility *c,uint32_t capacity)
{
    uint32_t n=0,visited=0;int status;
    rf_corpse_list_link *link,*previous;
    /* Current successful retained-actor registrations preserve serialized
     * order. No handle/UID sorting and no inferred original factory parity. */
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.view){
        if(n>=capacity)return RF_RANGE;
        status=scene_flight_npc_visibility_view(i,c->objects+n);if(status)return status;
        ++n;
    }
    c->actors=n;
    /* Type4 projection of the actual base list includes generated syringes;
     * known type10 glare links are a different original family. */
    {
        const rf_object_link *item=campaign_clutter_objects.sentinel.next,*prior=&campaign_clutter_objects.sentinel;
        uint32_t visited_clutter=0;
        if(!item)return RF_FORMAT;
        while(item!=&campaign_clutter_objects.sentinel){
            const rf_model_collision_resource *geometry;uint32_t family;rf_collision_visibility_object object;
            if(++visited_clutter>RF_OBJECT_CAPACITY)return RF_FORMAT;
            status=scene_flight_clutter_visibility_view(item,&object,&geometry,&family);if(status)return status;
            if(!family)return RF_NOT_FOUND;
            if(item->previous!=prior)return RF_FORMAT;
            if(family==1){if(n>=capacity)return RF_RANGE;c->objects[n++]=object;}
            prior=item;item=item->next;if(!item)return RF_FORMAT;
        }
        if(prior!=campaign_clutter_objects.sentinel.previous||visited_clutter!=campaign_clutter_objects.count)return RF_FORMAT;
    }
    c->clutter=n-c->actors;
    /* Corpses reuse pool slots. Their actual linked-list insertion order is
     * authoritative, not numeric pool order. Validate before each dereference. */
    previous=&campaign_live_corpse_head;link=previous->next;
    if(!link){c->count=n;return campaign_live_corpse_count?RF_FORMAT:RF_OK;}
    while(link!=&campaign_live_corpse_head){
        uint32_t slot;
        if(++visited>RF_CORPSE_CAPACITY||n>=capacity)return RF_FORMAT;
        for(slot=0;slot<RF_CORPSE_CAPACITY;slot++)if(link==&campaign_live_corpses.slots[slot].corpse.deletion.corpse_link)break;
        if(slot==RF_CORPSE_CAPACITY||link->previous!=previous)return RF_FORMAT;
        status=scene_flight_corpse_view(slot,c->objects+n);if(status)return status;
        ++n;previous=link;link=link->next;if(!link)return RF_FORMAT;
    }
    if(campaign_live_corpse_head.previous!=previous||visited!=campaign_live_corpse_count)return RF_FORMAT;
    c->corpses=visited;c->count=n;return RF_OK;
}
static int scene_medic_flight_observe(scene_stream *stream,campaign_npc_body *owner,
    const scene_medic_flight_threat_view *threat,uint32_t visibility[2])
{
    scene_flight_visibility c={0};rf_collision_visibility_list lists[3];
    rf_collision_visibility_backend backend={scene_flight_visibility_model,scene_flight_visibility_world,&c};
    uint64_t capacity=(uint64_t)campaign_npc_body_count+campaign_clutter_objects.count+RF_CORPSE_CAPACITY;
    uint32_t supported,blocked,values[2];float extent;int status;
    if(!stream||!owner||!threat||!visibility)return RF_RANGE;
    status=scene_medic_flight_visibility_admit(stream,owner,threat,&supported);if(status)return status;
    if(!supported||!scene_flight_owner_live(owner)||!scene_flight_threat_live(threat))return RF_NOT_FOUND;
    if(capacity>128*1024/sizeof(*c.objects))return RF_RANGE;
    status=scene_flight_visibility_extent(owner,threat,&extent);if(status)return status;
    c.stream=stream;c.owner=owner;c.threat=threat;c.registry_generation=campaign_registry.generation;
    c.objects=calloc((size_t)capacity,sizeof(*c.objects));if(!c.objects)return RF_IO;
    status=scene_flight_visibility_fill(&c,(uint32_t)capacity);if(status)goto done;
    lists[0]=(rf_collision_visibility_list){c.objects,c.actors};
    lists[1]=(rf_collision_visibility_list){c.objects+c.actors,c.clutter};
    lists[2]=(rf_collision_visibility_list){c.objects+c.actors+c.clutter,c.corpses};
    status=rf_collision_visibility(lists,owner->eye_position,threat->position,extent,0x27,
        owner->registration.handle,threat->handle,NULL,&backend,&blocked);if(status)goto done;
    values[0]=(blocked&255u)!=1u;
    status=scene_flight_visibility_stable(&c);if(status)goto done;
    status=rf_collision_visibility(lists,owner->eye_position,threat->eye,extent,0x27,
        owner->registration.handle,threat->handle,NULL,&backend,&blocked);if(status)goto done;
    values[1]=(blocked&255u)!=1u;
    status=scene_flight_visibility_stable(&c);if(!status){visibility[0]=values[0];visibility[1]=values[1];}
done:
    free(c.objects);return status;
}
#endif
