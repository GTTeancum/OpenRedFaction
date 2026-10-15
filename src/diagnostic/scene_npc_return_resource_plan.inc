#ifndef RF_SCENE_NPC_RETURN_RESOURCE_PLAN_INCLUDED
#define RF_SCENE_NPC_RETURN_RESOURCE_PLAN_INCLUDED
#include "rf/entity_assets.h"

/* Uncalled composition helper. Pure enumeration: no actor, demand callback,
 * resource counter, timer, playback or controller access. See CALLER-CONTRACT.txt.
 * Only the explicitly admitted armed standalone ordinary cohort is supported. */
enum {
    SCENE_NPC_RETURN_CLIP_RECOVERY = 1u,
    SCENE_NPC_RETURN_CLIP_IDLE = 2u,
    SCENE_NPC_RETURN_CLIP_MOVE = 4u
};
typedef struct scene_npc_return_resource_clip {
    uint32_t skeleton, motion, reasons;
} scene_npc_return_resource_clip;
typedef struct scene_npc_return_resource_plan {
    uint32_t class_index, skeleton;
    int32_t selection_weapon;
    uint32_t count, absent;
    /* Cardinality of action40/state0/state2, not a new runtime memory cap. */
    scene_npc_return_resource_clip clips[3];
} scene_npc_return_resource_plan;

/* -1 is source-defined absence, NOT a failed demand. Every emitted clip is
 * required to be resident if its future operation is retained. Deduplicate
 * registration identity here; the caller deduplicates shared cache allocations. */
static inline int scene_npc_return_resource_plan_add(
    scene_npc_return_resource_plan *p, int32_t motion,
    uint32_t model_count, uint32_t reason)
{
    uint32_t i;
    if(motion < -1)return RF_FORMAT;
    if(motion == -1){p->absent |= reason;return RF_OK;}
    if((uint32_t)motion >= model_count)return RF_RANGE;
    for(i=0;i<p->count;++i)if(p->clips[i].motion==(uint32_t)motion){
        p->clips[i].reasons |= reason;return RF_OK;
    }
    if(p->count >= 3u)return RF_RANGE;
    p->clips[p->count].skeleton=p->skeleton;
    p->clips[p->count].motion=(uint32_t)motion;
    p->clips[p->count].reasons=reason;
    ++p->count;return RF_OK;
}

/* mapping is a caller-owned, already weapon-resolved snapshot, NOT a class
 * base mapping or a borrowed live actor selection. model_skeleton/count belong
 * to that snapshot's playback/catalog registration. selected_weapon is the
 * canonical mapping weapon (including the machine-pistol-special alias).
 * primary_weapon is the actor's actual inventory ID; it need not equal the
 * canonical alias. Parent must prove that mapping was resolved for that ID.
 *
 * include_recovery covers future 408f20, even if today's timer is pending.
 * include_return_states covers the retained ordinary state0/state2 continuation.
 * No gate simulation or inference from a failed recovery is performed.
 * Output remains unchanged on error; there are no callbacks or effects. */
static inline int scene_npc_return_resources_plan(
    const rf_entity_motion_mapping *mapping,
    uint32_t class_index, uint32_t model_skeleton, uint32_t model_count,
    int32_t primary_weapon, int32_t selected_weapon, int32_t linked_handle,
    uint32_t include_recovery, uint32_t include_return_states,
    scene_npc_return_resource_plan *out)
{
    scene_npc_return_resource_plan p={0};
    int status;
    if(!mapping || !out || include_recovery>1u || include_return_states>1u)
        return RF_RANGE;
    if(primary_weapon<0 || selected_weapon<0 || linked_handle!=-1)
        return RF_NOT_FOUND;
    if(mapping->class_index!=class_index || mapping->skeleton!=model_skeleton ||
       model_skeleton==UINT32_MAX || mapping->weapon!=selected_weapon)
        return RF_FORMAT;
    p.class_index=class_index;p.skeleton=model_skeleton;
    p.selection_weapon=selected_weapon;
    /* 408f20 -> 428c90(action40); absence suppresses that start. Resets
     * 4091d0/409210 only inspect/stop existing clips, adding no future IDs. */
    if(include_recovery){
        status=scene_npc_return_resource_plan_add(&p,mapping->actions[40],
            model_count,SCENE_NPC_RETURN_CLIP_RECOVERY);
        if(status)return status;
    }
    /* Supplied retained continuation; missing state mappings remain absent
     * as in rf_motion_apply_controller. Never substitute state0 for state2. */
    if(include_return_states){
        status=scene_npc_return_resource_plan_add(&p,mapping->states[0],
            model_count,SCENE_NPC_RETURN_CLIP_IDLE);
        if(status)return status;
        status=scene_npc_return_resource_plan_add(&p,mapping->states[2],
            model_count,SCENE_NPC_RETURN_CLIP_MOVE);
        if(status)return status;
    }
    *out=p;return RF_OK;
}
#endif
