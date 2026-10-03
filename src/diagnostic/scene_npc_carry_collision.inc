#ifndef RF_SCENE_NPC_CARRY_COLLISION_INCLUDED
#define RF_SCENE_NPC_CARRY_COLLISION_INCLUDED
/* Include after rf_scene_npc_body_sweep and before campaign_npc_mover_carry.
 * The ordinary carry query has already hit its own upward-facing support.
 * Retry the SAME complete actor query, excluding exactly that support rather
 * than replacing the mover set with an empty world. This synchronous query
 * only invokes collision/material readers; no event, physics commit or yield
 * is allowed until the temporary exclusion has been restored. */
uint32_t rf_scene_npc_carry_collision[8]; /* retries,mover,passive,blocked,actor,support,other hit,error */
static int campaign_npc_carry_collision_retry(scene_stream *stream,campaign_npc_body *owner,
    const rf_physics_body_state *proposal,rf_collision_body_sphere *scratch,uint32_t capacity,
    rf_geometry_body_hit *hit,uint32_t *blocked)
{
    rf_geometry_body_hit result={0};uint32_t i,found=0,saved_flags=0,*flags=NULL;
    int32_t saved_link=0;uint32_t passive=0,support;int status;
    if(!stream||!owner||!proposal||!hit||!blocked||!*blocked||
       hit->contact.object_id!=owner->support.handle||hit->contact.normal[1]<.5f)return RF_RANGE;
    support=owner->support.handle;if(!support)return RF_NOT_FOUND;
    for(i=0;i<campaign_mover_count;i++)if(campaign_mover_wrappers[i].handle==support) {
        if(i>=campaign_movers.count||!campaign_movers.poses||!campaign_movers.views||
           campaign_movers.views[i].object_id!=support||
           campaign_mover_wrappers[i].pose!=campaign_movers.poses+i||
           rf_object_registry_lookup(&campaign_registry,support)!=campaign_mover_wrappers+i)return RF_FORMAT;
        flags=&campaign_movers.poses[i].flags;break;
    }
    if(!flags)for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==support) {
        if(rf_object_registry_lookup(&campaign_registry,support)!=campaign_passive_vehicles+i)return RF_FORMAT;
        passive=1;break;
    }
    if(!flags&&!passive)return RF_NOT_FOUND;
    ++rf_scene_npc_carry_collision[0];++rf_scene_npc_carry_collision[passive?2:1];
    rf_scene_npc_carry_collision[4]=owner->registration.handle;rf_scene_npc_carry_collision[5]=support;
    if(flags){saved_flags=*flags;*flags|=0x40000u;}
    else {saved_link=owner->view.linked_handle;owner->view.linked_handle=(int32_t)support;}
    status=rf_scene_npc_body_sweep(stream->collision,owner->registration.handle,proposal,
        0x460,scratch,capacity,&result,&found);
    if(flags)*flags=saved_flags;else owner->view.linked_handle=saved_link;
    /* No early return above this line: flags/link must be restored on failure. */
    if(status){rf_scene_npc_carry_collision[7]=(uint32_t)status;return status;}
    if(found){*hit=result;++rf_scene_npc_carry_collision[3];rf_scene_npc_carry_collision[6]=result.contact.object_id;}
    *blocked=found;return RF_OK;
}
#endif
