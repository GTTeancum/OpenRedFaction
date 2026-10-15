#ifndef RF_SCENE_PURSUIT_RETURN_LATE_SELECTION_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_LATE_SELECTION_INCLUDED
/* Uncalled proposal. Select retained NPC rows for a future return late-close
 * receipt, independently of crouch. This does not authenticate lifetime,
 * capture assigned state, seal player state, or authorize storage close.
 * Caller owns a stable, fully prepared stage and must first prove its lifetime.
 * No allocation, callback, actor mutation, or resource lookup is performed.
 * Only incoming targets survive final publication; outgoing return targets
 * are not dereferenced by retirement and do not gain a new continuation. */
enum {
    SCENE_RETURN_LATE_OWNER=1u,
    SCENE_RETURN_LATE_TARGET=2u
};
/* Uncalled pre-demand selector for the loader. Unlike the prepared-row
 * selector below, this runs before resource residency or stage construction.
 * rows must be the complete successfully decoded incoming RFNC rows; caller
 * authenticates the live body array and keeps output disjoint from inputs.
 * Bits 1/2 mean outgoing/incoming potential, not authorization to assign or
 * close storage. Every live receipt is checked, including malformed absence
 * and transient phase 4. Do not dereference outgoing target handles here:
 * retiring a continuation does not require its old target to remain alive.
 * Full candidate/source/tactical/player/seat joins still run after preparation.
 * Invalid input cannot silently choose the legacy lane or alter the output. */
static int scene_pursuit_return_load_potential(
    const rf_npc_checkpoint_record *rows,uint32_t count,uint32_t *potential)
{
    uint32_t found=0;int status;
    if(!potential||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT||
       (campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;
    for(uint32_t slot=0;slot<campaign_npc_body_count;slot++){
        const scene_pursuit_return_receipt *r=&campaign_npc_bodies[slot].pursuit_return;
        status=scene_pursuit_return_receipt_admit(r);if(status)return status;
        if(r->phase)found|=1u;
    }
    for(uint32_t i=0;i<count;i++){
        const rf_npc_checkpoint_record *r=rows+i;
        if(r->pursuit_return_profile>1||r->pursuit_return_phase>3||
           (!r->pursuit_return_profile&&r->pursuit_return_phase))return RF_FORMAT;
        if(r->pursuit_return_phase)found|=2u;
    }
    *potential=found;return RF_OK;
}
static int scene_pursuit_return_late_selection(
    const scene_npc_checkpoint_restore_stage *stage,uint32_t row,
    uint32_t player_handle,uint32_t *flags,uint32_t *needs_player)
{
    uint32_t selected=0,player=0;
    const scene_npc_checkpoint_restore_entry *e;
    if(!stage||!flags||!needs_player||flags==needs_player||
       row>=stage->count||stage->count>RF_NPC_CHECKPOINT_MAX_COUNT||
       !player_handle||player_handle==UINT32_MAX||stage->pursuit_return_joined!=1)
        return RF_RANGE;
    e=stage->entries+row;
    if(scene_pursuit_return_receipt_admit(&e->owner_before.pursuit_return))return RF_FORMAT;
    if(e->owner_before.pursuit_return.phase||e->pursuit_return_prepared.phase||
       e->pursuit_return_retire_pending)selected|=SCENE_RETURN_LATE_OWNER;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *owner=stage->entries+i;
        const scene_pursuit_return_receipt *r=&owner->pursuit_return_prepared;
        uint32_t found=UINT32_MAX;
        if(scene_pursuit_return_receipt_admit(r))return RF_FORMAT;
        if(!r->phase){
            if(r->target||r->combat_mode)return RF_FORMAT;
            continue;
        }
        if(!owner->registered||owner->saved.retired||!r->target||
           r->target==UINT32_MAX||r->target==owner->handle)return RF_FORMAT;
        if(r->target==player_handle){player=1;continue;}
        for(uint32_t j=0;j<stage->count;j++)if(stage->entries[j].handle==r->target){
            if(found!=UINT32_MAX)return RF_FORMAT;
            found=j;
        }
        if(found==UINT32_MAX)return RF_FORMAT;
        {
            const scene_npc_checkpoint_restore_entry *target=stage->entries+found;
            if(!target->registered||target->saved.retired||target->saved.dead_pose||
               !isfinite(target->saved.health)||target->saved.health<=0||
               target->saved.uid!=owner->saved.combat.event)return RF_FORMAT;
        }
        if(found==row)selected|=SCENE_RETURN_LATE_TARGET;
    }
    /* Output is published only after all target relations pass. A target-only
     * row still needs its own pre-assignment body/pose capture and projection;
     * the current crouch assigned validator can return early for such a row. */
    *flags=selected;*needs_player=player;return RF_OK;
}
#endif
