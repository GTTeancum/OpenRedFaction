#ifndef RF_SCENE_NPC_RELOAD_PUBLICATION_INCLUDED
#define RF_SCENE_NPC_RELOAD_PUBLICATION_INCLUDED
#include <stddef.h>
/* Conditional RFNC reload publication seal, source design 2026-10-11.
 * Include immediately after scene_npc_checkpoint_restore.inc. The caller has
 * authenticated outgoing pending BEFORE demand, finished every candidate join,
 * and admitted both player sides as on foot. Begin precedes parked/audio/RFNS
 * publication; seal follows the second airborne identity check. Only a sealed
 * receipt permits the caller's exactly-once storage close and ordered publish.
 * No callback, demand, publication, rollback, or vehicle admission lives here.
 * Fixed adapters and the paused synchronous world-load contract are required.
 */
enum { SCENE_RELOAD_SEAL_REQUESTS=42 };
typedef struct scene_npc_reload_publication_row {
    uint32_t slot,uid,class_id,handle,registered,bone_count,medic;
    uint32_t retired,dead_pose,animation_present,reload_present,reload_weapon,reload_remaining,reload_deadline;
    campaign_model_owner model;
    rf_entity_collision_cache collision;
    rf_entity_playback_model resources;
    rf_entity_model_motions motions;
    rf_entity_skeleton skeleton;
    rf_entity_motion_selection selection;
    rf_motion_playback_state playback;
    rf_level_waypoint_path move_path;
    rf_entity_navigation_candidate *move_nodes[4];
    scene_medic_syringe_candidate medic_child;
    float (*matrices)[12],(*original_matrices)[12];
    uint16_t *generations,*original_generations;
    uint32_t pose_bytes,collision_bytes;
    unsigned char *pose_copy,*collision_copy;
} scene_npc_reload_publication_row;
typedef struct scene_npc_reload_publication_resource {
    uint32_t skeleton,id,cache,bytes;
    rf_motion_playback_resource *address,value;
    rf_entity_model_motion motion;
    const void *payload;
} scene_npc_reload_publication_resource;
typedef struct scene_npc_reload_publication_child {
    rf_clutter_base_owner *address;
    rf_clutter_base_owner value;
    uint32_t candidate;
} scene_npc_reload_publication_child;
typedef struct scene_npc_reload_publication_link {
    rf_object_link *address;rf_object_link value;
    const void *registered;uint32_t handle,candidate;
} scene_npc_reload_publication_link;
typedef struct scene_npc_reload_publication_receipt {
    uint64_t epoch;uint32_t bytes,count,resource_count,child_count,link_count,medic_count;
    const scene_npc_checkpoint_restore_stage *stage;
    unsigned char stage_header[offsetof(scene_npc_checkpoint_restore_stage,entries)];
    campaign_npc_body *owners;uint32_t owner_count,model_count,persistence_count;
    campaign_model_owner *models;rf_entity_collision_cache *collisions;
    rf_model_skeletal_registration *model_head;
    rf_entity_seeds seeds;rf_entity_poses poses;rf_entity_skeletons skeletons;
    rf_entity_playback_resources resources;rf_entity_motion_catalog motions;
    rf_entity_base_motions base_motions;
    rf_entity_render_models render_models;rf_entity_collision_models collision_models;
    rf_clutter_classes clutter_classes;
    rf_level_owned_navigation navigation;
    rf_runtime_event *events;rf_runtime_trigger *triggers;uint32_t event_count,trigger_count;
    void **motion_data;uint32_t *motion_sizes;uint32_t motion_count,motion_bytes;
    rf_clutter_base_owner **clutter_bodies;uint32_t clutter_count;
    rf_glare_base_owner **glares;uint32_t glare_count;
    rf_object_list clutter_list,candidate_list;
    unsigned char medic_resource[sizeof(scene_medic_syringe_resource)];
    scene_npc_reload_publication_row *rows;
    scene_npc_reload_publication_resource *clips;
    scene_npc_reload_publication_child *children;
    scene_npc_reload_publication_link *links;
} scene_npc_reload_publication_receipt;

static void scene_npc_reload_publication_close(scene_npc_reload_publication_receipt **receipt)
{if(receipt&&*receipt){free(*receipt);*receipt=NULL;}}
static uint32_t scene_npc_reload_publication_bytes(const scene_npc_reload_publication_receipt *receipt)
{return receipt?receipt->bytes:0;}

static int scene_npc_reload_publication_span(const void *base,uint64_t bytes,const void *at,uint64_t size)
{
    uintptr_t b=(uintptr_t)base,p=(uintptr_t)at;
    return base&&at&&bytes<=UINTPTR_MAX-b&&p>=b&&size<=bytes&&p-b<=bytes-size;
}
static int scene_npc_reload_publication_registry(void)
{
    uint32_t i,j,live=0;
    if(campaign_registry.count>RF_OBJECT_CAPACITY||campaign_registry.head>=RF_OBJECT_CAPACITY)return RF_FORMAT;
    for(i=0;i<RF_OBJECT_CAPACITY;i++)if(campaign_registry.slots[i].object){
        uint32_t handle=campaign_registry.slots[i].handle;
        if(!handle||handle==UINT32_MAX||(handle&0xffffu)!=i)return RF_FORMAT;++live;
    }else if(campaign_registry.slots[i].handle)return RF_FORMAT;
    if(live!=RF_OBJECT_CAPACITY-campaign_registry.count)return RF_FORMAT;
    for(i=0;i<campaign_registry.count;i++){
        uint32_t slot=campaign_registry.free_slots[(campaign_registry.head+i)%RF_OBJECT_CAPACITY];
        if(slot>=RF_OBJECT_CAPACITY||campaign_registry.slots[slot].object)return RF_FORMAT;
        for(j=0;j<i;j++)if(slot==campaign_registry.free_slots[(campaign_registry.head+j)%RF_OBJECT_CAPACITY])return RF_FORMAT;
    }
    return RF_OK;
}
/* No copied whole owner is written back. The only admitted preclose projection
 * is RFNS clearing an authenticated old seat's linked handle to -1. An incoming
 * reload can replace an old seated row when that old side has no pending reload;
 * the caller separately admitted the incoming side as standalone. */
static int scene_npc_reload_publication_projection(const campaign_npc_body *owner,
    const campaign_npc_body *before,uint32_t incoming)
{
    const size_t at=offsetof(campaign_npc_body,view)+offsetof(rf_entity_view,linked_handle);
    const size_t end=at+sizeof(owner->view.linked_handle);
    if(owner->combat_reload_pending>1||incoming>1)return RF_FORMAT;
    if(memcmp(owner,before,at)||memcmp((const unsigned char*)owner+end,
        (const unsigned char*)before+end,sizeof(*owner)-end))return RF_FORMAT;
    if(owner->view.linked_handle!=before->view.linked_handle&&
       (owner->combat_reload_pending||before->view.linked_handle==-1||owner->view.linked_handle!=-1))return RF_FORMAT;
    return RF_OK;
}
/* Resolve without following a retained model-ring pointer until it is proven
 * to be one of the current fixed array nodes. */
static int scene_npc_reload_publication_ring(void)
{
    const rf_model_skeletal_registration *p=campaign_model_head,*previous=NULL;
    uint32_t i,n=0,loaded=0;
    if(campaign_model_owner_count&&!campaign_model_owners)return RF_FORMAT;
    for(i=0;i<campaign_model_owner_count;i++)loaded+=!!campaign_model_owners[i].registration.loaded;
    if(!loaded)return p?RF_FORMAT:RF_OK;
    if(!p)return RF_FORMAT;
    do{
        for(i=0;i<campaign_model_owner_count;i++)if(p==&campaign_model_owners[i].registration)break;
        if(i==campaign_model_owner_count||++n>loaded||!p->loaded||!p->next||!p->previous||
           (previous&&p->previous!=previous))return RF_FORMAT;
        if(!campaign_model_owners[i].pose||p->active!=&campaign_model_owners[i].pose->playback.completion.active)return RF_FORMAT;
        previous=p;p=p->next;
    }while(p!=campaign_model_head);
    return n==loaded&&campaign_model_head->previous==previous?RF_OK:RF_FORMAT;
}
static int scene_npc_reload_publication_owner(const scene_npc_checkpoint_restore_entry *e)
{
    const campaign_npc_body *o;const rf_entity_pose *p;uint32_t i,j;uint64_t bone_offset=0,sphere_bytes;
    if(e->slot>=campaign_npc_body_count||e->slot>=campaign_poses.count||
       e->slot>=campaign_seeds.records.count||!campaign_npc_bodies||!campaign_poses.items||
       !campaign_seeds.records.items||!campaign_seeds.items||!campaign_seeds.classes||
       e->saved.class_id>=campaign_seeds.class_count||e->registered>1||
       e->saved.uid!=(uint32_t)campaign_seeds.records.items[e->slot].record.uid||
       e->saved.class_id!=campaign_seeds.items[e->slot].class_index||e->unowned_motion_mask)return RF_FORMAT;
    o=campaign_npc_bodies+e->slot;p=campaign_poses.items+e->slot;
    if(e->handle!=o->registration.handle||!o->persistence_registered||
       o->persistence_slot>=rf_scene_defeated_actors.count||
       rf_scene_defeated_actors.items[o->persistence_slot].uid!=e->saved.uid||
       o->death_item_attempted!=e->item_attempted_before||o->death_item_fault!=e->item_fault_before||
       memcmp(&rf_scene_defeated_actors.item_drops[o->persistence_slot],&e->item_drop_before,sizeof(e->item_drop_before))||
       memcmp(&rf_scene_defeated_actors.routes[o->persistence_slot],&e->route_before,sizeof(e->route_before))||
       scene_npc_reload_publication_projection(o,&e->owner_before,e->saved.reload_present)||
       memcmp(p,&e->pose_before,sizeof(*p)))return RF_FORMAT;
    if(e->registered){
        if(!e->handle||e->handle==UINT32_MAX||o->registration.object_kind||o->registration.view!=&o->view||
           (uint32_t)o->view.handle!=e->handle||o->damage.effects.handle!=e->handle||
           rf_object_registry_lookup(&campaign_registry,e->handle)!=&o->registration||
           rf_entity_lookup(&campaign_entities,(int32_t)e->handle)!=&o->view)return RF_FORMAT;
        sphere_bytes=(uint64_t)o->body.spheres.count*sizeof(*o->body.spheres.items);
        if(o->body.spheres.count>8||!!o->body.spheres.count!=!!o->body.spheres.items||
           o->body.spheres.allocated_bytes!=sizeof(o->body.spheres)+sphere_bytes||
           o->body.allocated_bytes!=sizeof(o->body)+sphere_bytes)return RF_FORMAT;
        if(o->body.spheres.items)for(i=0;i<campaign_npc_body_count;i++)
            if(i!=e->slot&&campaign_npc_bodies[i].body.spheres.items==o->body.spheres.items)return RF_FORMAT;
    }else{
        if(!e->saved.retired||!rf_scene_defeated_actors.items[o->persistence_slot].retired||
           o->registration.object_kind||o->registration.handle||o->registration.view||o->view.handle!=-1||
           o->body.spheres.items||o->body.spheres.count||o->body.spheres.allocated_bytes||o->body.allocated_bytes)return RF_FORMAT;
        for(i=0;i<RF_OBJECT_CAPACITY;i++)if(campaign_registry.slots[i].object==&o->registration)return RF_FORMAT;
        for(i=0;i<RF_OBJECT_SLOTS;i++)if(campaign_entities.slots[i]==&o->view)return RF_FORMAT;
    }
    if(p->playback.completion.active.count>16||e->playback.completion.active.count>16||p->bone_count>50||e->bone_count>50)return RF_FORMAT;
    for(i=0;i<p->playback.completion.active.count;i++){
        if(p->playback.completion.active.slots[i].motion<0)return RF_FORMAT;
        for(j=0;j<i;j++)if(p->playback.completion.active.slots[i].motion==p->playback.completion.active.slots[j].motion)return RF_FORMAT;
    }
    for(i=0;i<e->playback.completion.active.count;i++){
        if(e->playback.completion.active.slots[i].motion<0)return RF_FORMAT;
        for(j=0;j<i;j++)if(e->playback.completion.active.slots[i].motion==e->playback.completion.active.slots[j].motion)return RF_FORMAT;
    }
    if(!e->registered)return e->playback.completion.active.count||e->bone_count?RF_FORMAT:RF_OK;
    if(p->skeleton==UINT32_MAX)
        return p->bone_count||p->matrices||p->generations||p->overrides||
            p->playback.completion.active.count||e->playback.completion.active.count||e->bone_count?RF_FORMAT:RF_OK;
    if(!campaign_skeletons.items||p->skeleton>=campaign_skeletons.count||
       !campaign_playback_resources.models||p->skeleton>=campaign_playback_resources.model_count||
       !campaign_motion_catalog.models||p->skeleton>=campaign_motion_catalog.model_count||
       !campaign_skeletons.class_indices||e->saved.class_id>=campaign_skeletons.class_count||
       campaign_skeletons.class_indices[e->saved.class_id]!=p->skeleton||
       !p->bone_count||p->bone_count!=campaign_skeletons.items[p->skeleton].count||
       !campaign_skeletons.items[p->skeleton].bones||
       !scene_npc_reload_publication_span(campaign_poses.matrices,(uint64_t)campaign_poses.bone_count*48,p->matrices,(uint64_t)p->bone_count*48)||
       !scene_npc_reload_publication_span(campaign_poses.generations,(uint64_t)campaign_poses.bone_count*2,p->generations,(uint64_t)p->bone_count*2)||
       !scene_npc_reload_publication_span(campaign_poses.overrides,(uint64_t)campaign_poses.bone_count*sizeof(*p->overrides),p->overrides,(uint64_t)p->bone_count*sizeof(*p->overrides))||
       !campaign_model_owners||e->slot>=campaign_model_owner_count||
       campaign_model_owners[e->slot].owned||!campaign_model_owners[e->slot].registration.loaded||
       campaign_model_owners[e->slot].pose!=p||campaign_model_owners[e->slot].registration.active!=&p->playback.completion.active||
       o->selection.mapping.class_index!=e->saved.class_id||o->selection.mapping.skeleton!=p->skeleton)return RF_FORMAT;
    if(!e->saved.retired&&(e->selection.mapping.class_index!=e->saved.class_id||e->selection.mapping.skeleton!=p->skeleton))return RF_FORMAT;
    /* Fresh-scene poses retain the constructor's disjoint per-slot slices.
     * In particular, in-range aliases to another NPC's destination are invalid. */
    for(i=0;i<e->slot;i++)if(campaign_poses.items[i].skeleton!=UINT32_MAX)
        bone_offset+=campaign_poses.items[i].bone_count;
    if(bone_offset+p->bone_count>campaign_poses.bone_count||
       p->matrices!=campaign_poses.matrices+bone_offset||p->generations!=campaign_poses.generations+bone_offset||
       p->overrides!=campaign_poses.overrides+bone_offset)return RF_FORMAT;
    if(e->bone_count&&(e->bone_count!=p->bone_count||e->original_matrices!=p->matrices||e->original_generations!=p->generations||!e->matrices||!e->generations))return RF_FORMAT;
    if(campaign_collision_caches&&e->slot<campaign_model_owner_count){
        const rf_entity_collision_cache *c=campaign_collision_caches+e->slot;
        if(c->bone_count!=p->bone_count||c->skeleton!=p->skeleton||!c->matrices||
           c->generations!=(uint16_t*)((unsigned char*)c->matrices+p->bone_count*48)||
           c->allocated_bytes!=sizeof(*c)+p->bone_count*50)return RF_FORMAT;
        if(scene_npc_reload_publication_span(campaign_poses.matrices,(uint64_t)campaign_poses.bone_count*48,c->matrices,1)||
           scene_npc_reload_publication_span(campaign_poses.generations,(uint64_t)campaign_poses.bone_count*2,c->matrices,1)||
           scene_npc_reload_publication_span(campaign_poses.overrides,(uint64_t)campaign_poses.bone_count*sizeof(*p->overrides),c->matrices,1))return RF_FORMAT;
        for(i=0;i<campaign_model_owner_count;i++)if(i!=e->slot&&campaign_collision_caches[i].matrices==c->matrices)return RF_FORMAT;
    }
    return RF_OK;
}
/* Enumerate exact touched clip identities, including both mapped reload clips
 * even when action39 is absent from active playback or has zero slot refs. */
static int scene_npc_reload_publication_request(const scene_npc_checkpoint_restore_entry *e,
    uint32_t request,uint32_t *skeleton,int32_t *id)
{
    const rf_entity_pose *p=&e->pose_before;int32_t state;
    *skeleton=p->skeleton;*id=-1;
    if(!e->registered)return RF_NOT_FOUND;
    if(p->skeleton==UINT32_MAX)return e->owner_before.combat_reload_pending||e->saved.reload_present?RF_FORMAT:RF_NOT_FOUND;
    if(request<16){if(request<p->playback.completion.active.count)*id=p->playback.completion.active.slots[request].motion;}
    else if(request<32){if(request-16<e->playback.completion.active.count)*id=e->playback.completion.active.slots[request-16].motion;}
    else if(request<38){
        uint32_t old=request<35,k=(request-32)%3;
        const rf_motion_controller *c=old?&p->controller:&e->saved.controller;
        const rf_entity_motion_selection *s=old?&e->owner_before.selection:&e->selection;
        if(!old&&(!e->saved.animation_present||e->saved.retired))return RF_NOT_FOUND;
        state=k==0?c->current:k==1?c->next:c->override_enabled?c->override_state:-1;
        if(state>=23||state< -1)return RF_FORMAT;
        if(state>=0)*id=s->mapping.states[state];
    }else if(request==38){if(e->owner_before.script_animation.active)*id=e->owner_before.script_animation.motion;}
    else if(request==39){if(e->saved.animation_present&&e->saved.script_animation.active)*id=e->saved.script_animation.motion;}
    else if(request==40){if(e->owner_before.combat_reload_pending)*id=e->owner_before.selection.mapping.actions[39];}
    else if(request==41){if(e->saved.reload_present)*id=e->selection.mapping.actions[39];}
    else return RF_RANGE;
    if(*id< -1)return RF_FORMAT;
    if((request==40&&e->owner_before.combat_reload_pending)||(request==41&&e->saved.reload_present))
        return *id<0?RF_FORMAT:RF_OK;
    return *id<0?RF_NOT_FOUND:RF_OK;
}
static uint32_t scene_npc_reload_publication_resident_request(uint32_t request)
{
    /* Old controller/script records are replaced, not sampled. Their metadata
     * identity is sealed, but they need no invented zero-slot payload demand.
     * Actual old slots and both logical reload owners always retain residency. */
    return !((request>=32&&request<35)||request==38);
}
static int scene_npc_reload_publication_clip(uint32_t skeleton,int32_t id,uint32_t resident,
    scene_npc_reload_publication_resource *copy)
{
    const rf_entity_playback_model *m;const rf_entity_model_motion *a;uint32_t cache;
    const rf_motion_playback_resource *r;
    if(id<0||!campaign_playback_resources.models||skeleton>=campaign_playback_resources.model_count||
       !campaign_motion_catalog.models||skeleton>=campaign_motion_catalog.model_count)return RF_FORMAT;
    m=campaign_playback_resources.models+skeleton;
    if(!m->resources||!m->cache_ids||(uint32_t)id>=m->count||
       !scene_npc_reload_publication_span(campaign_playback_resources.resources,
           (uint64_t)campaign_playback_resources.resource_count*sizeof(*m->resources),m->resources,(uint64_t)m->count*sizeof(*m->resources))||
       !scene_npc_reload_publication_span(campaign_playback_resources.cache_ids,
           (uint64_t)campaign_playback_resources.resource_count*sizeof(*m->cache_ids),m->cache_ids,(uint64_t)m->count*sizeof(*m->cache_ids))||
       !campaign_motion_catalog.models[skeleton].items||(uint32_t)id>=campaign_motion_catalog.models[skeleton].count)return RF_FORMAT;
    r=m->resources+id;a=campaign_motion_catalog.models[skeleton].items+id;cache=m->cache_ids[id];
    if(!a->file.archive||!a->file.entry.size||!campaign_npc_motion_data||!campaign_npc_motion_sizes||cache>=campaign_npc_motion_count||
       (resident&&(!campaign_npc_motion_data[cache]||a->file.resident!=campaign_npc_motion_data[cache]))||
       (a->file.resident&&a->file.resident!=campaign_npc_motion_data[cache])||
       (campaign_npc_motion_data[cache]?a->file.entry.size!=campaign_npc_motion_sizes[cache]:campaign_npc_motion_sizes[cache]!=0)||
       r->references<0||r->looping!=a->looping||memcmp(&r->comparison,&a->comparison,sizeof(r->comparison))||
       memcmp(r->markers,a->markers,sizeof(r->markers)))return RF_FORMAT;
    if(copy){copy->skeleton=skeleton;copy->id=(uint32_t)id;copy->cache=cache;copy->bytes=campaign_npc_motion_sizes[cache];
        copy->address=m->resources+id;copy->value=*r;copy->motion=*a;copy->payload=campaign_npc_motion_data[cache];}
    return RF_OK;
}
/* Bounded scans avoid a second unaccounted identity map. Deduplication is by
 * real skeleton/motion identity, never merely by a coincident clip number. */
static int scene_npc_reload_publication_first(const scene_npc_checkpoint_restore_stage *stage,
    uint32_t row,uint32_t request,uint32_t skeleton,int32_t id)
{
    uint32_t i,j,s;int32_t m;int status;
    for(i=0;i<=row;i++)for(j=0;j<(i==row?request:SCENE_RELOAD_SEAL_REQUESTS);j++){
        status=scene_npc_reload_publication_request(stage->entries+i,j,&s,&m);
        if(status==RF_NOT_FOUND)continue;if(status)return -1;
        if(s==skeleton&&m==id)return 0;
    }
    return 1;
}
/* Exact old-decrement/new-increment order from restore_assign. Initial excess
 * is outside ownership and survives untouched; no saturated decrement is used
 * as a proof. A checked prefix is required even when the final total fits. */
static int scene_npc_reload_publication_references(const scene_npc_checkpoint_restore_stage *stage,
    const rf_motion_playback_resource *resource)
{
    uint32_t i,j;uint64_t outgoing=0,incoming=0;int64_t value=resource->references;
    if(value<0)return RF_FORMAT;
    for(i=0;i<stage->count;i++)if(stage->entries[i].registered){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        const rf_entity_pose *p=campaign_poses.items+e->slot;
        if(p->skeleton==UINT32_MAX)continue;
        const rf_motion_playback_resource *base=campaign_playback_resources.models[p->skeleton].resources;
        for(j=0;j<p->playback.completion.active.count;j++)outgoing+=base+p->playback.completion.active.slots[j].motion==resource;
        for(j=0;j<e->playback.completion.active.count;j++)incoming+=base+e->playback.completion.active.slots[j].motion==resource;
    }
    if(outgoing>(uint64_t)value)return RF_FORMAT;
    for(i=0;i<stage->count;i++)if(stage->entries[i].registered){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        const rf_entity_pose *p=campaign_poses.items+e->slot;
        if(p->skeleton==UINT32_MAX)continue;
        const rf_motion_playback_resource *base=campaign_playback_resources.models[p->skeleton].resources;
        for(j=0;j<p->playback.completion.active.count;j++)if(base+p->playback.completion.active.slots[j].motion==resource){if(value<=0)return RF_FORMAT;--value;}
        for(j=0;j<e->playback.completion.active.count;j++)if(base+e->playback.completion.active.slots[j].motion==resource){if(value>=INT32_MAX)return RF_RANGE;++value;}
    }
    return value==(int64_t)resource->references-(int64_t)outgoing+(int64_t)incoming?RF_OK:RF_FORMAT;
}
/* Both base-list families have their state at allocation offset zero. Resolve
 * an intrusive node through a real full-handle registry owner before reading
 * the node. This also covers vehicle-owned glare nodes outside the authored
 * glare array. It never guesses a node by walking an unauthenticated pointer. */
static int scene_npc_reload_publication_link_owner(const rf_object_link *link,const void **owner,uint32_t *handle)
{
    uint32_t i;uintptr_t p=(uintptr_t)link;
    if(!link)return RF_FORMAT;
    for(i=0;i<RF_OBJECT_CAPACITY;i++)if(campaign_registry.slots[i].object){
        const void *registered=campaign_registry.slots[i].object;uintptr_t b=(uintptr_t)registered;
        if(b<=UINTPTR_MAX-sizeof(rf_clutter_base_owner)&&p==b+offsetof(rf_clutter_base_owner,object_link)){
            const rf_clutter_base_owner *c=registered;
            if(c->state.token!=(uint32_t)b||c->state.handle!=campaign_registry.slots[i].handle)return RF_FORMAT;
            *owner=registered;*handle=c->state.handle;return RF_OK;
        }
        if(b<=UINTPTR_MAX-sizeof(rf_glare_base_owner)&&p==b+offsetof(rf_glare_base_owner,object_link)){
            const rf_glare_base_owner *g=registered;
            if(g->kind!=10||g->handle!=campaign_registry.slots[i].handle)return RF_FORMAT;
            *owner=registered;*handle=g->handle;return RF_OK;
        }
    }
    return RF_FORMAT;
}
static int scene_npc_reload_publication_list(const rf_object_list *list,uint32_t candidate,
    scene_npc_reload_publication_link *copy)
{
    const rf_object_link *node,*previous;uint32_t i;int status;
    if(!list||list->count>RF_OBJECT_CAPACITY||!list->sentinel.next||!list->sentinel.previous)return RF_FORMAT;
    node=list->sentinel.next;previous=&list->sentinel;
    for(i=0;i<list->count;i++){
        const void *registered;uint32_t handle;
        if(!node||node==&list->sentinel)return RF_FORMAT;
        status=scene_npc_reload_publication_link_owner(node,&registered,&handle);if(status)return status;
        if(node->previous!=previous||!node->next)return RF_FORMAT;
        if(copy){copy[i].address=(rf_object_link*)node;copy[i].value=*node;copy[i].registered=registered;
            copy[i].handle=handle;copy[i].candidate=candidate;}
        previous=node;node=node->next;
    }
    return node==&list->sentinel&&list->sentinel.previous==previous?RF_OK:RF_FORMAT;
}
static int scene_npc_reload_publication_child_check(const rf_clutter_base_owner *child)
{
    uint32_t i;uint64_t spheres;
    if(!child)return RF_OK;
    if(!scene_medic_syringe_child_valid(child)||!child->state.handle||child->state.handle==UINT32_MAX||
       child->state.model!=child->attachment.model||child->state.link.next||child->state.link.previous||
       child->allocated_bytes<sizeof(*child))return RF_FORMAT;
    spheres=(uint64_t)child->body.spheres.count*sizeof(*child->body.spheres.items);
    if(sizeof(child->body.spheres)+spheres!=child->body.spheres.allocated_bytes||!!spheres!=!!child->body.spheres.items||
       child->body.allocated_bytes!=sizeof(child->body)+spheres||
       child->allocated_bytes!=sizeof(*child)+spheres)return RF_FORMAT;
    for(i=0;i<campaign_npc_body_count;i++)if(child->state.handle==campaign_npc_bodies[i].registration.handle||
       (const void*)child==(const void*)(campaign_npc_bodies+i)||
       (child->body.spheres.items&&child->body.spheres.items==campaign_npc_bodies[i].body.spheres.items))return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++)if(child->state.handle==campaign_events.items[i].handle)return RF_FORMAT;
    for(i=0;i<campaign_triggers.count;i++)if(child->state.handle==campaign_triggers.items[i].handle)return RF_FORMAT;
    for(i=0;i<RF_OBJECT_SLOTS;i++)if((const void*)campaign_entities.slots[i]==(const void*)&child->state)return RF_FORMAT;
    return RF_OK;
}
static int scene_npc_reload_publication_children(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *receipt)
{
    uint32_t i,j,old_count=0,new_count=0;uint64_t old_bytes=0,new_bytes=0;int status;
    if(!receipt->medic_count)return RF_OK;
    if(!receipt->child_count){
        for(i=0;i<stage->count;i++)if(stage->entries[i].medic_owner_snapshot){
            const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
            if(campaign_npc_bodies[e->slot].syringe.child||e->medic_child.child)return RF_FORMAT;
            status=scene_medic_syringe_restore_validate(campaign_npc_bodies+e->slot,&e->medic_child);if(status)return status;
        }
        return RF_OK;
    }
    if(memcmp(receipt->medic_resource,&scene_medic_syringe_resource,sizeof(scene_medic_syringe_resource))||
       !scene_medic_syringe_resource.ready||
       scene_medic_syringe_resource.shared.filename!=scene_medic_syringe_resource.model.name||
       scene_medic_syringe_resource.shared.resource!=&scene_medic_syringe_resource.model.resource||
       scene_medic_syringe_resource.class_index>=campaign_clutter_classes.count||!campaign_clutter_classes.items||
       (scene_medic_syringe_resource.model.resource.sphere_count&&!scene_medic_syringe_resource.model.resource.spheres))return RF_FORMAT;
    /* Authenticate all retained child allocations before a child/list-neighbor
     * dereference. Parent retirement later deliberately does not enter here. */
    for(i=0;i<receipt->child_count;i++){
        const scene_npc_reload_publication_child *c=receipt->children+i;
        if(rf_object_registry_lookup(&campaign_registry,c->value.state.handle)!=&c->address->state)return RF_FORMAT;
    }
    for(i=0;i<receipt->link_count;i++)if(rf_object_registry_lookup(&campaign_registry,receipt->links[i].handle)!=receipt->links[i].registered)return RF_FORMAT;
    if(memcmp(&receipt->clutter_list,&campaign_clutter_objects,sizeof(receipt->clutter_list))||
       memcmp(&receipt->candidate_list,&scene_medic_syringe_candidates,sizeof(receipt->candidate_list)))return RF_FORMAT;
    for(i=0;i<receipt->link_count;i++){
        const scene_npc_reload_publication_link *link=receipt->links+i;
        if(memcmp(link->address,&link->value,sizeof(link->value)))return RF_FORMAT;
    }
    status=scene_npc_reload_publication_list(&campaign_clutter_objects,0,NULL);if(status)return status;
    status=scene_npc_reload_publication_list(&scene_medic_syringe_candidates,1,NULL);if(status)return status;
    for(i=0;i<receipt->child_count;i++){
        const scene_npc_reload_publication_child *c=receipt->children+i;uint32_t found=0,parents=0;
        if(memcmp(c->address,&c->value,sizeof(c->value)))return RF_FORMAT;
        status=scene_npc_reload_publication_child_check(c->address);if(status)return status;
        for(j=0;j<i;j++)if(c->address==receipt->children[j].address||c->value.state.handle==receipt->children[j].value.state.handle||
           (c->value.body.spheres.items&&c->value.body.spheres.items==receipt->children[j].value.body.spheres.items))return RF_FORMAT;
        for(j=0;j<receipt->link_count;j++)if(receipt->links[j].address==&c->address->object_link){
            if(receipt->links[j].candidate!=c->candidate)return RF_FORMAT;++found;
        }
        if(found!=1)return RF_FORMAT;
        for(j=0;j<campaign_npc_body_count;j++)parents+=campaign_npc_bodies[j].syringe.child==c->address;
        if(parents!=(c->candidate?0u:1u))return RF_FORMAT;
        if(c->candidate){++new_count;new_bytes+=c->value.allocated_bytes;}
        else {++old_count;old_bytes+=c->value.allocated_bytes;}
    }
    /* One decrement per old close on success, one per candidate on failure.
     * Check their disjoint union too, preserving every outside contribution. */
    if((uint64_t)old_count+new_count>scene_medic_syringe_resource.shared.references||
       old_bytes+new_bytes>scene_medic_syringe_resource.body_bytes||
       old_count>campaign_clutter_objects.count||new_count!=scene_medic_syringe_candidates.count||
       (uint64_t)campaign_clutter_objects.count-old_count+new_count>RF_OBJECT_CAPACITY)return RF_FORMAT;
    for(i=0;i<stage->count;i++)if(stage->entries[i].medic_owner_snapshot){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        status=scene_medic_syringe_restore_validate(campaign_npc_bodies+e->slot,&e->medic_child);if(status)return status;
    }
    return RF_OK;
}
static int scene_npc_reload_publication_lifetime(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *r)
{
    /* These comparisons precede the first retained stage/row/pose/resource
     * dereference. Global owning headers themselves are static scene storage. */
    if(!r||!stage||r->epoch!=scene_robot_death_epoch||r->stage!=stage||
       r->owners!=campaign_npc_bodies||r->owner_count!=campaign_npc_body_count||
       r->models!=campaign_model_owners||r->model_count!=campaign_model_owner_count||
       r->collisions!=campaign_collision_caches||r->model_head!=campaign_model_head||
       r->persistence_count!=rf_scene_defeated_actors.count||
       memcmp(&r->seeds,&campaign_seeds,sizeof(r->seeds))||memcmp(&r->poses,&campaign_poses,sizeof(r->poses))||
       memcmp(&r->skeletons,&campaign_skeletons,sizeof(r->skeletons))||
       memcmp(&r->resources,&campaign_playback_resources,sizeof(r->resources))||
       memcmp(&r->motions,&campaign_motion_catalog,sizeof(r->motions))||
       memcmp(&r->base_motions,&campaign_base_motions,sizeof(r->base_motions))||
       memcmp(&r->render_models,&campaign_render_models,sizeof(r->render_models))||
       memcmp(&r->collision_models,&campaign_collision_models,sizeof(r->collision_models))||
       memcmp(&r->clutter_classes,&campaign_clutter_classes,sizeof(r->clutter_classes))||
       memcmp(&r->navigation,&campaign_navigation,sizeof(r->navigation))||
       r->events!=campaign_events.items||r->event_count!=campaign_events.count||
       r->triggers!=campaign_triggers.items||r->trigger_count!=campaign_triggers.count||
       r->motion_data!=campaign_npc_motion_data||r->motion_sizes!=campaign_npc_motion_sizes||
       r->motion_count!=campaign_npc_motion_count||r->motion_bytes!=campaign_npc_motion_bytes||
       r->clutter_bodies!=campaign_clutter_bodies||r->clutter_count!=campaign_clutter_records.count||
       r->glares!=campaign_glare_instances||r->glare_count!=campaign_glare_instance_count)return RF_FORMAT;
    if(memcmp(r->stage_header,stage,sizeof(r->stage_header))||stage->count!=r->count||!stage->fresh_boot)return RF_FORMAT;
    return RF_OK;
}
static int scene_npc_reload_publication_seal(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *receipt)
{
    uint32_t i,j;int status=scene_npc_reload_publication_lifetime(stage,receipt);if(status)return status;
    status=scene_npc_reload_publication_registry();if(status)return status;
    /* First authenticate row IDs and all borrowed per-skeleton array headers.
     * A later resource lookup must not read through a replaced model pointer. */
    for(i=0;i<receipt->count;i++){
        const scene_npc_reload_publication_row *r=receipt->rows+i;
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;const rf_entity_pose *p;
        if(e->slot!=r->slot||e->saved.uid!=r->uid||e->saved.class_id!=r->class_id||e->handle!=r->handle||
           e->registered!=r->registered||e->bone_count!=r->bone_count||e->medic_owner_snapshot!=r->medic||
           e->saved.retired!=r->retired||e->saved.dead_pose!=r->dead_pose||e->saved.animation_present!=r->animation_present||
           e->saved.reload_present!=r->reload_present||e->saved.reload_weapon!=r->reload_weapon||
           e->saved.reload_remaining!=r->reload_remaining||e->reload_deadline!=r->reload_deadline||
           e->matrices!=r->matrices||e->generations!=r->generations||e->original_matrices!=r->original_matrices||
           e->original_generations!=r->original_generations||memcmp(&e->selection,&r->selection,sizeof(r->selection))||
           memcmp(&e->playback,&r->playback,sizeof(r->playback))||memcmp(&e->move_path,&r->move_path,sizeof(r->move_path))||
           memcmp(e->move_nodes,r->move_nodes,sizeof(r->move_nodes))||memcmp(&e->medic_child,&r->medic_child,sizeof(r->medic_child)))return RF_FORMAT;
        status=scene_npc_reload_publication_owner(e);if(status)return status;p=campaign_poses.items+e->slot;
        if(e->slot<campaign_model_owner_count&&
           (memcmp(campaign_model_owners+e->slot,&r->model,sizeof(r->model))||
            (campaign_collision_caches&&memcmp(campaign_collision_caches+e->slot,&r->collision,sizeof(r->collision)))))return RF_FORMAT;
        if(e->registered&&p->skeleton!=UINT32_MAX){
            if(p->skeleton>=campaign_playback_resources.model_count||p->skeleton>=campaign_motion_catalog.model_count||
               memcmp(campaign_playback_resources.models+p->skeleton,&r->resources,sizeof(r->resources))||
               memcmp(campaign_motion_catalog.models+p->skeleton,&r->motions,sizeof(r->motions))||
               memcmp(campaign_skeletons.items+p->skeleton,&r->skeleton,sizeof(r->skeleton)))return RF_FORMAT;
            const unsigned char *copy=r->pose_copy;uint32_t n=p->bone_count;
            if(memcmp(p->matrices,copy,n*48))return RF_FORMAT;copy+=n*48;
            if(memcmp(p->generations,copy,n*2))return RF_FORMAT;copy+=n*2;
            if(memcmp(p->overrides,copy,n*sizeof(*p->overrides)))return RF_FORMAT;copy+=n*sizeof(*p->overrides);
            if(e->bone_count){if(memcmp(e->matrices,copy,e->bone_count*48))return RF_FORMAT;copy+=e->bone_count*48;
                if(memcmp(e->generations,copy,e->bone_count*2))return RF_FORMAT;}
            if(r->collision_bytes&&memcmp(campaign_collision_caches[e->slot].generations,r->collision_copy,r->collision_bytes))return RF_FORMAT;
        }
    }
    status=scene_npc_reload_publication_ring();if(status)return status;
    for(i=0;i<receipt->resource_count;i++){
        const scene_npc_reload_publication_resource *r=receipt->clips+i;
        const rf_entity_playback_model *m=campaign_playback_resources.models+r->skeleton;
        status=scene_npc_reload_publication_clip(r->skeleton,(int32_t)r->id,0,NULL);if(status)return status;
        if(m->resources+r->id!=r->address||m->cache_ids[r->id]!=r->cache||
           campaign_npc_motion_data[r->cache]!=r->payload||campaign_npc_motion_sizes[r->cache]!=r->bytes||
           memcmp(r->address,&r->value,sizeof(r->value))||
           memcmp(campaign_motion_catalog.models[r->skeleton].items+r->id,&r->motion,sizeof(r->motion)))return RF_FORMAT;
    }
    /* Re-enumerate all requests: candidate/live slots cannot acquire an
     * unsealed resource through a changed independent script/controller join. */
    for(i=0;i<stage->count;i++)for(j=0;j<SCENE_RELOAD_SEAL_REQUESTS;j++){
        uint32_t skeleton,k;int32_t id;
        status=scene_npc_reload_publication_request(stage->entries+i,j,&skeleton,&id);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        status=scene_npc_reload_publication_clip(skeleton,id,scene_npc_reload_publication_resident_request(j),NULL);if(status)return status;
        for(k=0;k<receipt->resource_count;k++)if(receipt->clips[k].skeleton==skeleton&&receipt->clips[k].id==(uint32_t)id)break;
        if(k==receipt->resource_count)return RF_FORMAT;
    }
    for(i=0;i<receipt->resource_count;i++){
        status=scene_npc_reload_publication_references(stage,receipt->clips[i].address);if(status)return status;
    }
    return scene_npc_reload_publication_children(stage,receipt);
}
static int scene_npc_reload_publication_begin(const scene_npc_checkpoint_restore_stage *stage,uint32_t budget,
    scene_npc_reload_publication_receipt **out)
{
    scene_npc_reload_publication_receipt *r=NULL;unsigned char *storage;
    uint64_t bytes,pose_bytes=0,child_bytes=0,stage_storage;uint32_t i,j,clips=0,children=0,links=0,medics=0;
    uint32_t at_clip=0,at_child=0;int status;
    if(!out||*out||!stage||!stage->fresh_boot||stage->count>RF_NPC_CHECKPOINT_MAX_COUNT||
       campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||campaign_model_owner_count>RF_CAMPAIGN_ACTOR_SLOTS||
       !stage->flight_joined||(stage->count&&(!campaign_npc_bodies||!campaign_poses.items)))return RF_RANGE;
    status=scene_npc_reload_publication_registry();if(status)return status;
    /* The caller has completed ordinary restore validation and every joined
     * component admission. Do not recapture a whole row or replay those joins;
     * only their exact touched lifetime projections are sealed here. */
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;const rf_entity_pose *p;
        status=scene_npc_reload_publication_owner(e);if(status)return status;p=campaign_poses.items+e->slot;
        for(j=0;j<i;j++)if(stage->entries[j].slot==e->slot||stage->entries[j].saved.uid==e->saved.uid||
           (e->registered&&stage->entries[j].registered&&stage->entries[j].handle==e->handle))return RF_FORMAT;
        if(e->registered&&p->skeleton!=UINT32_MAX){
            pose_bytes+=(uint64_t)p->bone_count*(50+sizeof(*p->overrides))+(uint64_t)e->bone_count*50;
            if(campaign_collision_caches&&e->slot<campaign_model_owner_count)
                pose_bytes+=(uint64_t)campaign_collision_caches[e->slot].bone_count*2;
        }
        if(e->medic_owner_snapshot){
            const rf_clutter_base_owner *old=campaign_npc_bodies[e->slot].syringe.child;
            ++medics;
            status=scene_medic_syringe_restore_validate(campaign_npc_bodies+e->slot,&e->medic_child);if(status)return status;
            if(old){status=scene_npc_reload_publication_child_check(old);if(status)return status;++children;}
            if(e->medic_child.child){
                status=scene_npc_reload_publication_child_check(e->medic_child.child);if(status)return status;
                if(e->medic_child.allocated_bytes!=e->medic_child.child->allocated_bytes)return RF_FORMAT;
                ++children;child_bytes+=e->medic_child.allocated_bytes;
            }else if(e->medic_child.allocated_bytes)return RF_FORMAT;
        }
        for(j=0;j<SCENE_RELOAD_SEAL_REQUESTS;j++){
            uint32_t skeleton;int32_t id;int first;
            status=scene_npc_reload_publication_request(e,j,&skeleton,&id);
            if(status==RF_NOT_FOUND)continue;if(status)return status;
            status=scene_npc_reload_publication_clip(skeleton,id,scene_npc_reload_publication_resident_request(j),NULL);if(status)return status;
            first=scene_npc_reload_publication_first(stage,i,j,skeleton,id);if(first<0)return RF_FORMAT;
            clips+=(uint32_t)first;
        }
    }
    status=scene_npc_reload_publication_ring();if(status)return status;
    if(child_bytes>stage->bytes)return RF_FORMAT;stage_storage=stage->bytes-child_bytes;
    if(stage_storage<sizeof(*stage)+(uint64_t)stage->count*sizeof(*stage->entries))return RF_FORMAT;
    /* Candidate matrices/generations must belong to this retained allocation,
     * beyond its fixed rows. No live pose destination may be borrowed instead. */
    for(i=0;i<stage->count;i++)if(stage->entries[i].bone_count){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        const void *base=stage->entries+stage->count;
        uint64_t available=stage_storage-(sizeof(*stage)+(uint64_t)stage->count*sizeof(*stage->entries));
        if(!scene_npc_reload_publication_span(base,available,e->matrices,(uint64_t)e->bone_count*48)||
           !scene_npc_reload_publication_span(base,available,e->generations,(uint64_t)e->bone_count*2)||
           (void*)e->generations!=(unsigned char*)e->matrices+e->bone_count*48)return RF_FORMAT;
        for(j=0;j<i;j++)if(stage->entries[j].bone_count){
            const scene_npc_checkpoint_restore_entry *a=stage->entries+j;
            uintptr_t one=(uintptr_t)e->matrices,two=(uintptr_t)a->matrices;
            if(one<two+(uint64_t)a->bone_count*50&&two<one+(uint64_t)e->bone_count*50)return RF_FORMAT;
        }
    }
    if(children){
        if(campaign_clutter_objects.count>RF_OBJECT_CAPACITY||scene_medic_syringe_candidates.count>RF_OBJECT_CAPACITY||
           (uint64_t)campaign_clutter_objects.count+scene_medic_syringe_candidates.count>RF_OBJECT_CAPACITY)return RF_FORMAT;
        links=campaign_clutter_objects.count+scene_medic_syringe_candidates.count;
        status=scene_npc_reload_publication_list(&campaign_clutter_objects,0,NULL);if(status)return status;
        status=scene_npc_reload_publication_list(&scene_medic_syringe_candidates,1,NULL);if(status)return status;
    }
    bytes=sizeof(*r)+(uint64_t)stage->count*sizeof(*r->rows)+(uint64_t)clips*sizeof(*r->clips)+
        (uint64_t)children*sizeof(*r->children)+(uint64_t)links*sizeof(*r->links)+pose_bytes;
    if(bytes>budget||bytes>UINT32_MAX)return RF_RANGE;
    r=calloc(1,(size_t)bytes);if(!r)return RF_IO;
    r->bytes=(uint32_t)bytes;r->epoch=scene_robot_death_epoch;r->stage=stage;r->count=stage->count;
    r->resource_count=clips;r->child_count=children;r->link_count=links;r->medic_count=medics;
    memcpy(r->stage_header,stage,sizeof(r->stage_header));
    r->owners=campaign_npc_bodies;r->owner_count=campaign_npc_body_count;
    r->models=campaign_model_owners;r->model_count=campaign_model_owner_count;r->collisions=campaign_collision_caches;
    r->model_head=campaign_model_head;r->persistence_count=rf_scene_defeated_actors.count;
#define SCENE_RELOAD_SEAL_HEADER(member,global) memcpy(&r->member,&global,sizeof(r->member))
    SCENE_RELOAD_SEAL_HEADER(seeds,campaign_seeds);SCENE_RELOAD_SEAL_HEADER(poses,campaign_poses);
    SCENE_RELOAD_SEAL_HEADER(skeletons,campaign_skeletons);SCENE_RELOAD_SEAL_HEADER(resources,campaign_playback_resources);
    SCENE_RELOAD_SEAL_HEADER(motions,campaign_motion_catalog);SCENE_RELOAD_SEAL_HEADER(base_motions,campaign_base_motions);
    SCENE_RELOAD_SEAL_HEADER(render_models,campaign_render_models);SCENE_RELOAD_SEAL_HEADER(collision_models,campaign_collision_models);
    SCENE_RELOAD_SEAL_HEADER(clutter_classes,campaign_clutter_classes);
    SCENE_RELOAD_SEAL_HEADER(navigation,campaign_navigation);SCENE_RELOAD_SEAL_HEADER(clutter_list,campaign_clutter_objects);
    SCENE_RELOAD_SEAL_HEADER(candidate_list,scene_medic_syringe_candidates);
#undef SCENE_RELOAD_SEAL_HEADER
    r->events=campaign_events.items;r->event_count=campaign_events.count;r->triggers=campaign_triggers.items;r->trigger_count=campaign_triggers.count;
    r->motion_data=campaign_npc_motion_data;r->motion_sizes=campaign_npc_motion_sizes;r->motion_count=campaign_npc_motion_count;r->motion_bytes=campaign_npc_motion_bytes;
    r->clutter_bodies=campaign_clutter_bodies;r->clutter_count=campaign_clutter_records.count;r->glares=campaign_glare_instances;r->glare_count=campaign_glare_instance_count;
    memcpy(r->medic_resource,&scene_medic_syringe_resource,sizeof(r->medic_resource));
    storage=(unsigned char*)(r+1);r->rows=(scene_npc_reload_publication_row*)storage;storage+=stage->count*sizeof(*r->rows);
    r->clips=(scene_npc_reload_publication_resource*)storage;storage+=clips*sizeof(*r->clips);
    r->children=(scene_npc_reload_publication_child*)storage;storage+=children*sizeof(*r->children);
    r->links=(scene_npc_reload_publication_link*)storage;storage+=links*sizeof(*r->links);
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;const rf_entity_pose *p=campaign_poses.items+e->slot;
        scene_npc_reload_publication_row *row=r->rows+i;
        row->slot=e->slot;row->uid=e->saved.uid;row->class_id=e->saved.class_id;row->handle=e->handle;
        row->registered=e->registered;row->bone_count=e->bone_count;row->medic=e->medic_owner_snapshot;
        row->retired=e->saved.retired;row->dead_pose=e->saved.dead_pose;row->animation_present=e->saved.animation_present;
        row->reload_present=e->saved.reload_present;row->reload_weapon=e->saved.reload_weapon;
        row->reload_remaining=e->saved.reload_remaining;row->reload_deadline=e->reload_deadline;
        row->matrices=e->matrices;row->generations=e->generations;row->original_matrices=e->original_matrices;row->original_generations=e->original_generations;
        memcpy(&row->selection,&e->selection,sizeof(row->selection));memcpy(&row->playback,&e->playback,sizeof(row->playback));
        memcpy(&row->move_path,&e->move_path,sizeof(row->move_path));memcpy(row->move_nodes,e->move_nodes,sizeof(row->move_nodes));
        memcpy(&row->medic_child,&e->medic_child,sizeof(row->medic_child));
        if(e->slot<campaign_model_owner_count){memcpy(&row->model,campaign_model_owners+e->slot,sizeof(row->model));
            if(campaign_collision_caches)memcpy(&row->collision,campaign_collision_caches+e->slot,sizeof(row->collision));}
        if(e->registered&&p->skeleton!=UINT32_MAX){
            if(p->skeleton>=campaign_playback_resources.model_count||p->skeleton>=campaign_motion_catalog.model_count){status=RF_FORMAT;goto fail;}
            memcpy(&row->resources,campaign_playback_resources.models+p->skeleton,sizeof(row->resources));
            memcpy(&row->motions,campaign_motion_catalog.models+p->skeleton,sizeof(row->motions));
            memcpy(&row->skeleton,campaign_skeletons.items+p->skeleton,sizeof(row->skeleton));
            row->pose_copy=storage;row->pose_bytes=p->bone_count*(50+sizeof(*p->overrides))+e->bone_count*50;
            memcpy(storage,p->matrices,p->bone_count*48);storage+=p->bone_count*48;
            memcpy(storage,p->generations,p->bone_count*2);storage+=p->bone_count*2;
            memcpy(storage,p->overrides,p->bone_count*sizeof(*p->overrides));storage+=p->bone_count*sizeof(*p->overrides);
            if(e->bone_count){memcpy(storage,e->matrices,e->bone_count*48);storage+=e->bone_count*48;
                memcpy(storage,e->generations,e->bone_count*2);storage+=e->bone_count*2;}
            if(campaign_collision_caches&&e->slot<campaign_model_owner_count){
                row->collision_copy=storage;row->collision_bytes=campaign_collision_caches[e->slot].bone_count*2;
                memcpy(storage,campaign_collision_caches[e->slot].generations,row->collision_bytes);storage+=row->collision_bytes;
            }
        }
        for(j=0;j<SCENE_RELOAD_SEAL_REQUESTS;j++){
            uint32_t skeleton;int32_t id;int first;
            status=scene_npc_reload_publication_request(e,j,&skeleton,&id);if(status==RF_NOT_FOUND)continue;if(status)goto fail;
            first=scene_npc_reload_publication_first(stage,i,j,skeleton,id);if(first<0){status=RF_FORMAT;goto fail;}
            if(first){if(at_clip>=clips){status=RF_FORMAT;goto fail;}
                status=scene_npc_reload_publication_clip(skeleton,id,scene_npc_reload_publication_resident_request(j),r->clips+at_clip++);if(status)goto fail;}
        }
        if(e->medic_owner_snapshot)for(j=0;j<2;j++){
            rf_clutter_base_owner *child=j?e->medic_child.child:campaign_npc_bodies[e->slot].syringe.child;
            if(child){if(at_child>=children){status=RF_FORMAT;goto fail;}
                r->children[at_child].address=child;r->children[at_child].candidate=j;
                memcpy(&r->children[at_child++].value,child,sizeof(*child));}
        }
    }
    if(at_clip!=clips||at_child!=children||storage!=(unsigned char*)r+r->bytes){status=RF_FORMAT;goto fail;}
    if(children){
        status=scene_npc_reload_publication_list(&campaign_clutter_objects,0,r->links);if(status)goto fail;
        status=scene_npc_reload_publication_list(&scene_medic_syringe_candidates,1,r->links+campaign_clutter_objects.count);if(status)goto fail;
    }
    status=scene_npc_reload_publication_seal(stage,r);if(status)goto fail;
    *out=r;return RF_OK;
fail:scene_npc_reload_publication_close(&r);return status;
}
#endif
