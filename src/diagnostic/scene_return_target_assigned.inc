#ifndef RF_SCENE_RETURN_TARGET_ASSIGNED_INCLUDED
#define RF_SCENE_RETURN_TARGET_ASSIGNED_INCLUDED
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* Include after campaign_npc_body, campaign_model_owner and restore entry.
 * UNCALLED target-safety projection, not a complete owner/transaction seal.
 * All pointers/spans must first pass the parent's lifetime and candidate seal.
 * before headers and byte snapshots are independent immutable copies.
 * No function here follows a pointer stored in before->body.
 * Ordinary target-only rows: registered before, no old/new crouch, no dead
 * pose. Staged deferred retirement is explicitly rejected for parent handling.
 * Registry removal/presence, UID/class, dependencies, staged return fields and
 * resource-reference deltas remain caller checks. No callbacks or mutations. */
typedef struct scene_return_target_bytes {
    const void *data;
    size_t bytes;
} scene_return_target_bytes;
typedef struct scene_return_target_assignment {
    const scene_npc_checkpoint_restore_entry *candidate;
    const campaign_npc_body *before, *live;
    const rf_entity_pose *pose_before, *pose;
    const campaign_model_owner *model_before, *model;
    const rf_entity_collision_cache *cache_before, *cache;
    const float *normal_orientation;
    scene_return_target_bytes spheres, matrices, overrides, cache_matrices;
    uint32_t retirement_deferred;
} scene_return_target_assignment;

static inline int scene_return_target_bytes_equal(const void *live,
    const scene_return_target_bytes *saved, size_t bytes)
{
    if(saved->bytes!=bytes)return RF_FORMAT;
    if(!bytes)return RF_OK;
    if(!live||!saved->data||live==saved->data)return RF_FORMAT;
    return memcmp(live,saved->data,bytes)?RF_FORMAT:RF_OK;
}
static inline int scene_return_target_zero(const void *data,size_t bytes)
{
    const unsigned char *p=(const unsigned char *)data;
    for(size_t i=0;i<bytes;i++)if(p[i])return RF_FORMAT;
    return RF_OK;
}

/* RF_OK validates only the fields below. It neither admits a living target
 * from a retired row nor authorizes close/replay. There is no output to alter.
 * Snapshot array lengths are bytes, not capacity promises for untrusted input.
 * Existing source caps: eight spheres, fifty bones. No new allocation/cap. */
static inline int scene_return_target_assigned(
    const scene_return_target_assignment *a)
{
    const scene_npc_checkpoint_restore_entry *e;
    const campaign_npc_body *o,*old;
    const rf_entity_pose *p,*prior;
    const campaign_model_owner *m,*pm;
    rf_motion_controller controller;
    rf_physics_support_contact support;
    rf_collision_contact_extra contact;
    uint32_t flags,damage_flags,flags_7d0,reacquire,i;
    int status;
    if(!a||!a->candidate||!a->before||!a->live||!a->pose_before||
       !a->pose||!a->model_before||!a->model||a->retirement_deferred>1)
        return RF_RANGE;
    e=a->candidate;o=a->live;old=a->before;p=a->pose;prior=a->pose_before;
    m=a->model;pm=a->model_before;
    if(old==o||prior==p||pm==m)return RF_FORMAT;
    if(a->retirement_deferred||e->crouch_retire_pending||
       e->crouch_shape_present||e->saved.crouch.flags||old->crouch.flags||
       (old->view.flags_810&0x400u)||e->saved.dead_pose)
        return RF_NOT_FOUND;
    if(e->registered!=1||e->saved.retired>1||e->saved.animation_present>1||
       e->saved.airborne.present>1||e->movement_resolved>1||
       !e->handle||e->handle==UINT32_MAX||old->registration.handle!=e->handle||
       old->registration.view!=&o->view||(uint32_t)old->view.handle!=e->handle||
       old->damage.effects.handle!=e->handle||old->view.linked_handle!=-1||
       old->view.occupant_count||old->attachment_75c!=-1||
       !old->persistence_registered)return RF_FORMAT;
    if(o->persistence_slot!=old->persistence_slot||
       o->persistence_registered!=old->persistence_registered||
       o->damage.effects.handle!=old->damage.effects.handle||
       o->view.linked_handle!=old->view.linked_handle||
       o->view.occupant_count!=old->view.occupant_count||
       o->attachment_75c!=old->attachment_75c||
       o->controller_handle!=e->controller_handle||
       memcmp(&o->crouch,&old->crouch,sizeof(o->crouch))||
       o->unholster.stance_7bc!=old->unholster.stance_7bc||
       o->stance_clock_7b4!=old->stance_clock_7b4||
       memcmp(o->published,e->published,sizeof(e->published))||
       memcmp(o->previous,e->published,sizeof(e->published))||
       memcmp(&o->damage.effects.health,&e->saved.health,sizeof(float))||
       memcmp(&o->damage.effects.armor,&e->saved.armor,sizeof(float))||
       o->damage.effects.affiliation!=e->saved.affiliation||
       memcmp(&o->inventory,&e->saved.inventory,sizeof(o->inventory))||
       o->view.weapons[0]!=e->saved.primary||o->view.weapons[1]!=e->saved.secondary||
       o->scripted_physics_seen!=e->saved.physics.present||
       o->scripted_physics!=e->saved.physics.scripted)return RF_FORMAT;

    flags=old->view.flags_810;damage_flags=old->damage.effects.flags_810;
    flags_7d0=old->view.flags_7d0;
    if(e->saved.retired){
        /* 1147: unregister + body_close; never touch old sphere storage. */
        if(scene_return_target_zero(&o->registration,sizeof(o->registration))||
           o->view.handle!=-1||scene_return_target_zero(&o->body,sizeof(o->body)))
            return RF_FORMAT;
        flags|=1u;damage_flags=flags;
        if(memcmp(&o->room,&old->room,sizeof(o->room))||
           memcmp(&o->selection,&old->selection,sizeof(o->selection))||
           memcmp(o->eye_position,old->eye_position,sizeof(o->eye_position))||
           memcmp(&o->support,&old->support,sizeof(o->support))||
           memcmp(&o->collision_contact,&old->collision_contact,sizeof(o->collision_contact))||
           memcmp(o->support_velocity,old->support_velocity,sizeof(o->support_velocity))||
           memcmp(&o->movement,&old->movement,sizeof(o->movement))||
           o->movement_slot!=old->movement_slot||
           o->movement_orientation!=old->movement_orientation)return RF_FORMAT;
    }else{
        if(!isfinite(e->saved.health)||e->saved.health<=0||(old->view.flags_810&1u)||
           memcmp(&o->registration,&old->registration,sizeof(o->registration))||
           o->view.handle!=old->view.handle||o->body.allocated_bytes!=old->body.allocated_bytes||
           memcmp(&o->body.spheres,&old->body.spheres,sizeof(o->body.spheres))||
           old->body.spheres.count>8||
           memcmp(&o->body.state,&e->body,sizeof(e->body))||
           memcmp(&o->room,&e->room,sizeof(e->room))||
           memcmp(&o->selection,&e->selection,sizeof(e->selection))||
           memcmp(o->eye_position,e->eye,sizeof(e->eye)))return RF_FORMAT;
        status=scene_return_target_bytes_equal(o->body.spheres.items,&a->spheres,
            (size_t)o->body.spheres.count*sizeof(*o->body.spheres.items));
        if(status)return status;
        flags=(flags&~0x800u)|((e->saved.holster&1u)?0x800u:0u);
        damage_flags=(damage_flags&~0x800u)|(flags&0x800u);
        flags_7d0=(flags_7d0&~0x200u)|((e->saved.holster&2u)?0x200u:0u);
        support=e->support;contact=old->collision_contact;
        if(e->saved.airborne.present){
            support.handle=0;support.material=-1;
            memset(&contact,0,sizeof(contact));
            contact.material=contact.reference=UINT32_MAX;
        }
        if(memcmp(&o->support,&support,sizeof(support))||
           memcmp(&o->collision_contact,&contact,sizeof(contact))||
           memcmp(o->support_velocity,e->saved.airborne.present?
               e->saved.airborne.inherited_velocity:e->saved.support_velocity,
               sizeof(o->support_velocity)))return RF_FORMAT;
        if(e->movement_resolved){
            if(!a->normal_orientation||
               memcmp(&o->movement,&e->movement,sizeof(e->movement))||
               o->movement_slot!=e->movement_slot||
               o->movement_orientation!=a->normal_orientation)return RF_FORMAT;
        }else if(memcmp(&o->movement,&old->movement,sizeof(o->movement))||
                 o->movement_slot!=old->movement_slot||
                 o->movement_orientation!=old->movement_orientation)return RF_FORMAT;
    }
    reacquire=!e->saved.retired&&!e->support.handle&&e->support.material!=-1;
    if(!e->saved.retired&&e->saved.airborne.present)reacquire=0;
    if(o->object_flags!=(e->flags|(e->saved.retired?(2u|0x4000u):0u))||
       o->view.flags_7c!=o->object_flags||o->view.flags_810!=flags||
       o->damage.effects.flags_810!=damage_flags||o->view.flags_7d0!=flags_7d0||
       o->piece_reacquire!=reacquire||
       scene_return_target_zero(&o->piece_support,sizeof(o->piece_support)))return RF_FORMAT;

    /* Registered-before rows publish pose even when their body retires. */
    if(p->skeleton!=prior->skeleton||p->bone_count!=prior->bone_count||
       p->bone_count>50||e->bone_count>50||
       (p->bone_count&&(!p->matrices||!p->generations))||p->matrices!=prior->matrices||
       p->generations!=prior->generations||p->overrides!=prior->overrides||
       memcmp(&p->playback,&e->playback,sizeof(e->playback)))return RF_FORMAT;
    if(e->saved.animation_present)controller=e->saved.controller;
    else{memset(&controller,0,sizeof(controller));controller.override_state=-1;}
    if(memcmp(&p->controller,&controller,sizeof(controller)))return RF_FORMAT;
    if(e->bone_count){
        if(e->bone_count!=p->bone_count||!p->matrices||!p->generations||
           !e->matrices||!e->generations||p->matrices==e->matrices||
           p->generations==e->generations||
           memcmp(p->matrices,e->matrices,(size_t)e->bone_count*48)||
           memcmp(p->generations,e->generations,(size_t)e->bone_count*2))return RF_FORMAT;
    }else{
        status=scene_return_target_bytes_equal(p->matrices,&a->matrices,
            p->matrices?(size_t)p->bone_count*48:0);if(status)return status;
        if(p->generations)for(i=0;i<p->bone_count;i++)
            if(p->generations[i]!=(uint16_t)(p->playback.generation-1u))return RF_FORMAT;
    }
    status=scene_return_target_bytes_equal(p->overrides,&a->overrides,
        p->overrides?(size_t)p->bone_count*sizeof(*p->overrides):0);
    if(status)return status;
    if(memcmp(&m->registration,&pm->registration,sizeof(m->registration))||
       m->pose!=pm->pose||m->owned!=pm->owned||m->appearance!=pm->appearance||
       (pm->registration.loaded&&(m->pose!=p||
         m->registration.active!=&p->playback.completion.active)))return RF_FORMAT;
    if(memcmp(m->position,e->saved.retired?pm->position:e->published,sizeof(m->position))||
       memcmp(m->basis,e->saved.retired?pm->basis:e->basis,sizeof(m->basis))||
       m->room!=(e->saved.retired?pm->room:e->room.room?e->room.room-1u:UINT32_MAX))
        return RF_FORMAT;

    /* Cache header/matrices stay fixed; assign invalidates stamps only. */
    if(!!a->cache!=!!a->cache_before)return RF_FORMAT;
    if(a->cache){
        if(a->cache==a->cache_before||
           memcmp(a->cache,a->cache_before,sizeof(*a->cache))||
           a->cache->bone_count>50||
           (a->cache->bone_count&&(!a->cache->matrices||!a->cache->generations)))return RF_FORMAT;
        status=scene_return_target_bytes_equal(a->cache->matrices,&a->cache_matrices,
            a->cache->matrices?(size_t)a->cache->bone_count*48:0);
        if(status)return status;
        if(a->cache->generations)for(i=0;i<a->cache->bone_count;i++)
            if(a->cache->generations[i]!=(uint16_t)(p->playback.generation-1u))return RF_FORMAT;
    }else if(a->cache_matrices.bytes)return RF_FORMAT;
    return RF_OK;
}
#endif
