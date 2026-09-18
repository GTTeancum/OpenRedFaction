#ifndef RF_SCENE_AI_MODE_PERSISTENCE_INCLUDED
#define RF_SCENE_AI_MODE_PERSISTENCE_INCLUDED
/* In-memory level revisit only, not RFCP disk schema. Existing actor key owns
 * slot lifetime. No process pointer, handle or previous-level timer is saved.
 * 32bytes/slot =64KiB at2048 slots; caller resets with new campaign store. */
typedef struct campaign_ai_saved_mode {
    uint32_t level,uid;int32_t action;
    uint32_t offset,count,cursor,section_hash,flags;
} campaign_ai_saved_mode;
static campaign_ai_saved_mode campaign_ai_saved_modes[RF_CAMPAIGN_ACTOR_SLOTS];
static void campaign_ai_modes_reset(void)
{memset(campaign_ai_saved_modes,0,sizeof(campaign_ai_saved_modes));}
static void campaign_ai_mode_capture(const campaign_npc_body *owner)
{
    campaign_ai_saved_mode saved={0};uint32_t slot;uintptr_t base,address;uint64_t bytes;
    if(!owner || !owner->persistence_registered)return;
    slot=owner->persistence_slot;if(slot>=rf_scene_defeated_actors.count || slot>=RF_CAMPAIGN_ACTOR_SLOTS)return;
    saved.level=rf_scene_defeated_actors.items[slot].level;saved.uid=rf_scene_defeated_actors.items[slot].uid;
    saved.action=owner->ai_mode.action_280;
    if(saved.action==0)saved.action=-1; /* Unset constructor means ordinary policy. */
    if(saved.action!=1 && saved.action!=2 && saved.action!=4 && saved.action!=11 && saved.action!=-1)goto done;
    saved.flags=1;
    if(owner->script_move.path.indices && owner->script_move.path.count && campaign_waypoints) {
        base=(uintptr_t)campaign_waypoints;address=(uintptr_t)owner->script_move.path.indices;
        bytes=(uint64_t)owner->script_move.path.count*4;
        if(address>=base && address-base<=campaign_waypoint_bytes && bytes<=campaign_waypoint_bytes-(address-base) &&
           owner->script_move.path_index<owner->script_move.path.count && owner->script_move.path_mode<=2) {
            saved.offset=(uint32_t)(address-base);saved.count=owner->script_move.path.count;
            saved.cursor=owner->script_move.path_index;
            saved.section_hash=npc_hash_bytes(2166136261u,campaign_waypoints,campaign_waypoint_bytes);
            saved.flags|=2u|(owner->script_move.path_mode<<2)|(!!owner->script_move.path_reverse<<4)|
                (!!owner->script_move.active<<5);
        }
    }
    if(saved.action==4 && !(saved.flags&2))saved.flags=0;
done:campaign_ai_saved_modes[slot]=saved;
}
static int campaign_ai_mode_restore(campaign_npc_body *owner,int32_t now)
{
    const campaign_ai_saved_mode *saved;uint32_t slot,j,k,node;rf_level_waypoint_path path={0};int status;
    if(!owner || !owner->persistence_registered)return RF_NOT_FOUND;
    slot=owner->persistence_slot;if(slot>=rf_scene_defeated_actors.count || slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_RANGE;
    saved=campaign_ai_saved_modes+slot;
    if(!(saved->flags&1) || rf_scene_defeated_actors.items[slot].retired ||
       saved->level!=rf_scene_defeated_actors.items[slot].level || saved->uid!=rf_scene_defeated_actors.items[slot].uid)return RF_NOT_FOUND;
    if(saved->flags&2) {
        if(!campaign_waypoints || !campaign_navigation.nodes ||
           saved->offset>campaign_waypoint_bytes || (uint64_t)saved->count*4>campaign_waypoint_bytes-saved->offset ||
           saved->section_hash!=npc_hash_bytes(2166136261u,campaign_waypoints,campaign_waypoint_bytes))return RF_NOT_FOUND;
        path.indices=campaign_waypoints+saved->offset;path.count=saved->count;
        if(!path.count || saved->cursor>=path.count)return RF_FORMAT;
        for(j=0;j<path.count;j++){node=rf_level_waypoint_node(&path,j);if(node>=campaign_navigation.count)return RF_NOT_FOUND;
            for(k=0;k<3;k++)if(!isfinite(campaign_navigation.nodes[node].candidate.position[k]))return RF_RANGE;}
    }
    /* Validate service exclusions before publishing the rebound route. */
    if(now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(owner->ai_mode.action_280==13 || owner->view.linked_handle!=-1)return RF_NOT_FOUND;
    if(path.count) {
        owner->script_move.path=path;owner->script_move.path_index=saved->cursor;
        owner->script_move.path_mode=(saved->flags>>2)&3;owner->script_move.path_reverse=(saved->flags>>4)&1;
    }
    status=campaign_ai_mode_apply(owner,saved->action,now);if(status)return status;
    if(saved->action==4 && !(saved->flags&32)){owner->script_move.active=0;owner->script_move.stop=1;}
    return RF_OK;
}
#endif
