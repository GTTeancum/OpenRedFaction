#ifndef RF_SCENE_PURSUIT_RETURN_RELATION_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_RELATION_INCLUDED
/* Uncalled relation seal for the composed late-close transaction. Caller
 * authenticates stage/world/registry allocation lifetime BEFORE every call,
 * retains this immutable record at a stable address, and separately seals
 * owner and target body/pose projections (or the player projection). This
 * record grants no selection, resource, storage-close or replay authority.
 * Candidate and receipt storage must be disjoint. No callbacks or allocation. */
typedef struct scene_pursuit_return_relation {
    uint32_t row,slot,uid,class_id,handle,profile,phase,retire_pending;
    uint32_t combat_active,combat_event,attack_target;
    uint32_t target_row,target_slot,target_uid,target_class,target_handle;
    uint32_t player;
    scene_pursuit_return_receipt incoming,outgoing;
    float target_health;
} scene_pursuit_return_relation;
_Static_assert(sizeof(void*)!=4||sizeof(scene_pursuit_return_relation)<=96,
    "Return relation exceeds proposed source allowance");

/* Prepare only already-selected return OWNER rows. Incoming NPC targets must
 * have a unique candidate row; outgoing targets are not dereferenced because
 * retirement does not publish a continuation for them. */
static int scene_pursuit_return_relation_prepare(
    const scene_npc_checkpoint_restore_stage *stage,uint32_t row,
    scene_pursuit_return_relation *out)
{
    scene_pursuit_return_relation r={0};
    const scene_npc_checkpoint_restore_entry *e;int status;
    if(!stage||!out||stage->pursuit_return_joined!=1||
       stage->count>RF_NPC_CHECKPOINT_MAX_COUNT||row>=stage->count)return RF_RANGE;
    e=stage->entries+row;
    if(!e->registered||!campaign_seeds.records.items||!campaign_seeds.items||
       e->slot>=campaign_seeds.records.count||e->saved.class_id>=campaign_seeds.class_count||
       (uint32_t)campaign_seeds.records.items[e->slot].record.uid!=e->saved.uid||
       campaign_seeds.items[e->slot].class_index!=e->saved.class_id)return RF_FORMAT;
    status=scene_pursuit_return_receipt_admit(&e->owner_before.pursuit_return);
    if(status)return status;
    status=scene_pursuit_return_prepared(stage,e);if(status)return status;
    if(!e->owner_before.pursuit_return.phase&&!e->pursuit_return_prepared.phase&&
       !e->pursuit_return_retire_pending)return RF_NOT_FOUND;
    r.row=row;r.slot=e->slot;r.uid=e->saved.uid;r.class_id=e->saved.class_id;
    r.handle=e->handle;r.profile=e->saved.pursuit_return_profile;
    r.phase=e->saved.pursuit_return_phase;r.retire_pending=e->pursuit_return_retire_pending;
    r.combat_active=e->saved.combat.active;r.combat_event=e->saved.combat.event;
    r.attack_target=e->attack_target_handle;
    r.incoming=e->pursuit_return_prepared;r.outgoing=e->owner_before.pursuit_return;
    r.target_row=UINT32_MAX;r.target_slot=UINT32_MAX;
    if(r.incoming.phase){
        r.target_handle=r.incoming.target;
        if(r.target_handle==campaign_player_object.handle)r.player=1;
        else{
            for(uint32_t i=0;i<stage->count;i++)if(stage->entries[i].handle==r.target_handle){
                if(r.target_row!=UINT32_MAX)return RF_FORMAT;
                r.target_row=i;
            }
            if(r.target_row==UINT32_MAX||r.target_row==row)return RF_FORMAT;
            {
                const scene_npc_checkpoint_restore_entry *t=stage->entries+r.target_row;
                if(!t->registered||t->saved.retired||t->saved.dead_pose||
                   t->crouch_retire_pending||t->pursuit_return_retire_pending||
                   !isfinite(t->saved.health)||t->saved.health<=0||
                   t->saved.uid!=r.combat_event||t->slot>=campaign_seeds.records.count||
                   t->saved.class_id>=campaign_seeds.class_count||
                   (uint32_t)campaign_seeds.records.items[t->slot].record.uid!=t->saved.uid||
                   campaign_seeds.items[t->slot].class_index!=t->saved.class_id)return RF_FORMAT;
                r.target_slot=t->slot;r.target_uid=t->saved.uid;
                r.target_class=t->saved.class_id;r.target_health=t->saved.health;
            }
        }
    }
    *out=r;return RF_OK;
}
/* Before assignment use assigned=0. After all assignment captures and before
 * successful close use assigned=1, after target/player deep seals. The live
 * return receipt is deliberately still outgoing until restore_finish. */
static int scene_pursuit_return_relation_check(
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_pursuit_return_relation *r,uint32_t assigned)
{
    scene_pursuit_return_relation current;const campaign_npc_body *o;int status;
    if(!r||assigned>1)return RF_RANGE;
    status=scene_pursuit_return_relation_prepare(stage,r->row,&current);if(status)return status;
    if(memcmp(&current,r,sizeof(current)))return RF_FORMAT;
    if(!assigned)return RF_OK;
    if(!campaign_npc_bodies||r->slot>=campaign_npc_body_count)return RF_FORMAT;
    o=campaign_npc_bodies+r->slot;
    status=scene_npc_reload_identity(r->slot,o);if(status)return status;
    if(o->registration.handle!=r->handle||
       memcmp(&o->pursuit_return,&r->outgoing,sizeof(r->outgoing)))return RF_FORMAT;
    if(r->incoming.phase){
        if(o->combat_target!=r->target_handle||o->combat_scripted!=r->incoming.combat_mode||
           !o->combat_alert)return RF_FORMAT;
        if(r->player){
            if(campaign_player_object.handle!=r->target_handle||
               campaign_player_object.view!=&campaign_player_view||
               rf_object_registry_lookup(&campaign_registry,r->target_handle)!=&campaign_player_object||
               rf_entity_lookup(&campaign_entities,(int32_t)r->target_handle)!=&campaign_player_view)
                return RF_FORMAT;
        }else{
            const campaign_npc_body *t;
            if(r->target_slot>=campaign_npc_body_count)return RF_FORMAT;
            t=campaign_npc_bodies+r->target_slot;
            status=scene_npc_reload_identity(r->target_slot,t);if(status)return status;
            if(t->registration.handle!=r->target_handle||
               memcmp(&t->damage.effects.health,&r->target_health,sizeof(r->target_health))||
               !isfinite(t->damage.effects.health)||t->damage.effects.health<=0)return RF_FORMAT;
        }
    }
    return RF_OK;
}
#endif
