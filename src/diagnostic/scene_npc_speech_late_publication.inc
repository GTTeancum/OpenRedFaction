#ifndef RF_SCENE_NPC_SPEECH_LATE_PUBLICATION_INCLUDED
#define RF_SCENE_NPC_SPEECH_LATE_PUBLICATION_INCLUDED
enum {
    SCENE_SPEECH_LATE_EMPTY=0,SCENE_SPEECH_LATE_PREPARED=1,
    SCENE_SPEECH_LATE_CAPTURED=2,SCENE_SPEECH_LATE_SEALED=3,
    SCENE_SPEECH_LATE_CLOSED=4,SCENE_SPEECH_LATE_FAILED=5
};
typedef struct scene_npc_speech_late {
    const scene_npc_checkpoint_restore_stage *stage;
    const scene_npc_reload_publication_receipt *publication;
    uint32_t stage_bytes,publication_bytes,potential,state;
} scene_npc_speech_late;
_Static_assert(sizeof(void*)!=4||sizeof(scene_npc_speech_late)==24,
    "Speech late gate Xbox32 size changed");

/* Before resource demand. Decoded incoming rows already passed RFNC semantics;
 * incoming clip residency is deliberately NOT required at this stage. */
static int scene_npc_speech_load_potential(const rf_npc_checkpoint_record *rows,
    uint32_t count,uint32_t *potential)
{
    uint32_t found=0;
    if(!potential||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_poses.items))||
       campaign_npc_body_count>campaign_poses.count)return RF_RANGE;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->speech_motion_mask&~SCENE_NPC_SPEECH_ALL)return RF_FORMAT;
        if(o->speech_motion_mask){
            if(!scene_npc_speech_checkpoint_mask(i,&o->selection,
                &campaign_poses.items[i].playback,NULL,o->object_flags,
                o->speech_motion_mask))return RF_FORMAT;
            found|=1u;
        }
    }
    for(uint32_t i=0;i<count;i++){
        if(rows[i].speech_motion_mask&~SCENE_NPC_SPEECH_ALL)return RF_FORMAT;
        if(rows[i].speech_motion_mask)found|=2u;
    }
    *potential=found;return RF_OK;
}

static int scene_npc_speech_late_lifetime(const scene_npc_speech_late *s)
{
    int status;
    if(!s||!s->stage||!s->publication||!s->potential||s->potential>3)
        return RF_FORMAT;
    status=scene_npc_reload_publication_lifetime(s->stage,s->publication);
    if(status)return status;
    status=scene_return_target_publication_layout(s->stage,s->publication,
        s->stage_bytes,s->publication_bytes);if(status)return status;
    /* Before any residual resource lookup, authenticate borrowed model arrays.
     * All rows participate in the one shared resource proof, not just speech. */
    for(uint32_t i=0;i<s->publication->count;i++){
        const scene_npc_reload_publication_row *r=s->publication->rows+i;
        uint32_t k=r->skeleton_id;
        if(!r->registered||k==UINT32_MAX)continue;
        if(!campaign_playback_resources.models||!campaign_motion_catalog.models||
           !campaign_skeletons.items||k>=campaign_playback_resources.model_count||
           k>=campaign_motion_catalog.model_count||k>=campaign_skeletons.count||
           memcmp(campaign_playback_resources.models+k,&r->resources,sizeof(r->resources))||
           memcmp(campaign_motion_catalog.models+k,&r->motions,sizeof(r->motions))||
           memcmp(campaign_skeletons.items+k,&r->skeleton,sizeof(r->skeleton)))
            return RF_FORMAT;
    }
    return RF_OK;
}

/* Call immediately after publication begin, before any publication callback.
 * Recover only the main retained-stage extent; separate syringe allocations
 * remain charged in stage->bytes and are not subtracted from budget costs. */
static int scene_npc_speech_late_begin(scene_npc_speech_late *out,
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *publication,uint32_t potential)
{
    scene_npc_speech_late next={0};uint64_t children=0,extent;
    uint32_t found=0;int status;
    if(!out||out->state||!stage||!publication||!potential||potential>3)
        return RF_RANGE;
    status=scene_npc_reload_publication_lifetime(stage,publication);if(status)return status;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        if(e->medic_owner_snapshot&&e->medic_child.child)
            children+=e->medic_child.allocated_bytes;
        if(e->owner_before.speech_motion_mask)found|=1u;
        if(e->saved.speech_motion_mask)found|=2u;
        status=scene_npc_speech_prepared(stage,e);if(status)return status;
    }
    if(found!=potential||children>stage->bytes)return RF_FORMAT;
    extent=stage->bytes-children;
    if(extent>UINT32_MAX||extent<sizeof(*stage)+(uint64_t)stage->count*sizeof(*stage->entries))
        return RF_FORMAT;
    next.stage=stage;next.publication=publication;
    next.stage_bytes=(uint32_t)extent;next.publication_bytes=publication->bytes;
    next.potential=potential;next.state=SCENE_SPEECH_LATE_PREPARED;
    status=scene_npc_speech_late_lifetime(&next);if(status)return status;
    *out=next;return RF_OK;
}

/* Deterministic speech projection of restore_assign. Unlike the live capture
 * helper, this deliberately tolerates an old word naming now-replaced slots.
 * Incoming proof uses saved ownership plus actual assigned playback; it never
 * changes the old live word to make a validator pass. */
static int scene_npc_speech_late_assigned(const scene_npc_speech_late *s)
{
    int status=scene_npc_speech_late_lifetime(s);if(status)return status;
    for(uint32_t i=0;i<s->publication->count;i++){
        const scene_npc_reload_publication_row *r=s->publication->rows+i;
        const scene_npc_speech_publication_projection *q=&r->speech;
        const scene_npc_checkpoint_restore_entry *e=s->stage->entries+i;
        const campaign_npc_body *o;const campaign_model_owner *m;
        const rf_entity_pose *p;const unsigned char *copy;
        rf_motion_controller controller;uint32_t n,registered;
        if(e->slot!=r->slot||r->slot>=campaign_npc_body_count||
           r->slot>=campaign_poses.count||!campaign_npc_bodies||!campaign_poses.items)
            return RF_FORMAT;
        status=scene_npc_speech_publication_candidate(e,r);if(status)return status;
        o=campaign_npc_bodies+r->slot;
        if(o->speech_motion_mask!=q->before)return RF_FORMAT;
        if(!(q->incoming|q->before))continue;
        if(!r->registered||!campaign_model_owners||r->slot>=campaign_model_owner_count||
           !o->persistence_registered||o->persistence_slot>=rf_scene_defeated_actors.count||
           rf_scene_defeated_actors.items[o->persistence_slot].uid!=r->uid||
           rf_scene_defeated_actors.items[o->persistence_slot].retired!=r->retired)
            return RF_FORMAT;
        registered=!r->retired||r->crouch_retire_pending||r->pursuit_return_retire_pending;
        if(registered){
            if(o->registration.handle!=r->handle||o->registration.object_kind||
               o->registration.view!=&o->view||(uint32_t)o->view.handle!=r->handle||
               o->damage.effects.handle!=r->handle||
               rf_object_registry_lookup(&campaign_registry,r->handle)!=&o->registration||
               rf_entity_lookup(&campaign_entities,(int32_t)r->handle)!=&o->view)
                return RF_FORMAT;
        }else{
            if(o->registration.handle||o->registration.view||o->view.handle!=-1)
                return RF_FORMAT;
            for(uint32_t j=0;j<RF_OBJECT_CAPACITY;j++)
                if(campaign_registry.slots[j].object==&o->registration)return RF_FORMAT;
            for(uint32_t j=0;j<RF_OBJECT_SLOTS;j++)
                if(campaign_entities.slots[j]==&o->view)return RF_FORMAT;
        }
        p=campaign_poses.items+r->slot;m=campaign_model_owners+r->slot;
        n=r->skeleton.count;
        if(!n||n>50||p!=r->model.pose||m->pose!=p||m->owned!=r->model.owned||
           memcmp(&m->registration,&r->model.registration,sizeof(m->registration))||
           p->skeleton!=r->skeleton_id||p->bone_count!=n||
           p->matrices!=q->matrices||p->generations!=q->generations||
           p->overrides!=q->overrides||!p->matrices||!p->generations||!p->overrides||
           memcmp(&p->playback,&r->playback,sizeof(r->playback))||
           o->view.weapons[0]!=q->primary||
           memcmp(&o->damage.effects.health,&q->health,sizeof(q->health))||
           o->object_flags!=(q->flags|(r->retired?(2u|0x4000u):0u))||
           o->view.flags_7c!=o->object_flags)return RF_FORMAT;
        if(!r->retired&&memcmp(&o->selection,&r->selection,sizeof(r->selection)))
            return RF_FORMAT;
        if(r->animation_present){
            controller=q->controller;
            if(memcmp(&o->script_animation,r->alarm_script,sizeof(r->alarm_script)))
                return RF_FORMAT;
        }else{
            static const uint32_t zero_script[4]={0,0,0,0};
            memset(&controller,0,sizeof(controller));controller.override_state=-1;
            if(memcmp(&o->script_animation,zero_script,sizeof(zero_script)))return RF_FORMAT;
        }
        if(memcmp(&p->controller,&controller,sizeof(controller)))return RF_FORMAT;
        copy=r->pose_copy;
        if(r->bone_count){
            const unsigned char *candidate=copy+n*(50+sizeof(*p->overrides));
            if(r->bone_count!=n||!e->matrices||!e->generations||
               memcmp(e->matrices,candidate,n*48)||memcmp(p->matrices,candidate,n*48)||
               memcmp(e->generations,candidate+n*48,n*2)||
               memcmp(p->generations,candidate+n*48,n*2))return RF_FORMAT;
        }else{
            if(memcmp(p->matrices,copy,n*48))return RF_FORMAT;
            for(uint32_t j=0;j<n;j++)
                if(p->generations[j]!=(uint16_t)(p->playback.generation-1u))return RF_FORMAT;
        }
        if(memcmp(p->overrides,copy+n*50,n*sizeof(*p->overrides)))return RF_FORMAT;
        if(q->incoming){
            if(!registered||((o->view.flags_810|o->damage.effects.flags_810)&1u)||
               !scene_npc_speech_checkpoint_mask(r->slot,&r->selection,&p->playback,
                   &e->saved,q->flags,q->incoming))return RF_FORMAT;
            /* Candidate demand allowed zero references. Actual assigned slots
             * must now contribute a real positive reference; the shared barrier
             * separately proves the exact global old/new delta once. */
            for(uint32_t j=0;j<p->playback.completion.active.count;j++){
                int32_t id=p->playback.completion.active.slots[j].motion;
                if(id<0||(uint32_t)id>=r->resources.count||
                   r->resources.resources[id].references<=0)return RF_FORMAT;
            }
        }
    }
    return scene_npc_reload_publication_ring();
}

static int scene_npc_speech_late_capture(scene_npc_speech_late *s)
{
    int status;
    if(!s||s->state!=SCENE_SPEECH_LATE_PREPARED){
        if(s)s->state=SCENE_SPEECH_LATE_FAILED;return RF_FORMAT;
    }
    s->state=SCENE_SPEECH_LATE_FAILED;
    status=scene_npc_speech_late_assigned(s);if(status)return status;
    s->state=SCENE_SPEECH_LATE_CAPTURED;return RF_OK;
}
static int scene_npc_speech_late_seal(scene_npc_speech_late *s)
{
    int status;
    if(!s||s->state!=SCENE_SPEECH_LATE_CAPTURED){
        if(s)s->state=SCENE_SPEECH_LATE_FAILED;return RF_FORMAT;
    }
    s->state=SCENE_SPEECH_LATE_FAILED;
    status=scene_npc_speech_late_assigned(s);if(status)return status;
    s->state=SCENE_SPEECH_LATE_SEALED;return RF_OK;
}
/* No borrowed reads or callbacks here. No validation/resource operation may
 * run after close_begin, on either native close success or failure. */
static int scene_npc_speech_late_close_begin(scene_npc_speech_late *s,
    uint32_t resource_state)
{
    if(!s||s->state!=SCENE_SPEECH_LATE_SEALED||resource_state!=3){
        if(s)s->state=SCENE_SPEECH_LATE_FAILED;return RF_FORMAT;
    }
    s->state=SCENE_SPEECH_LATE_CLOSED;return RF_OK;
}
#endif
