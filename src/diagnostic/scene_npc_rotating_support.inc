/* Frame-relative support carry for ordinary NPCs. Include immediately before
 * campaign_npc_mover_carry. The caller invokes this once per NPC per completed
 * mover interval, before either idle or scripted ground movement. This is
 * position carry, not a seat assignment or a change to the actor's facing.
 * Collision admission remains owned by campaign_npc_mover_carry. */
#ifndef RF_SCENE_NPC_ROTATING_SUPPORT_INCLUDED
#define RF_SCENE_NPC_ROTATING_SUPPORT_INCLUDED

uint32_t rf_scene_npc_rotating_support[8]; /* proposals,rotating,last actor,host,target XYZ,error */

/* Row-vector basis convention: recover the point in the old host frame, then
 * place it in the new host frame. Pure translation intentionally retains the
 * old arithmetic path; avoid introducing matrix roundoff on every idle lift.
 * Exported for a bounded compiled-NXDK mathematical check. */
int rf_scene_npc_support_interval_target(const scene_mover_interval *interval,
    const float position[3],float seconds,float target[3],float velocity[3])
{
    float next[3],speed[3],local[3];uint32_t i,j;
    if(!interval||!position||!target||!velocity||!isfinite(seconds)||seconds<=0)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(position[i])||!isfinite(interval->start[i])||
        !isfinite(interval->end[i]))return RF_FORMAT;
    if(interval->changed&2u) {
        for(i=0;i<9;i++)if(!isfinite(interval->matrix[i])||!isfinite(interval->end_matrix[i]))return RF_FORMAT;
        for(i=0;i<3;i++) {
            local[i]=0;
            for(j=0;j<3;j++)local[i]+=(position[j]-interval->start[j])*interval->matrix[i*3+j];
        }
        for(i=0;i<3;i++) {
            next[i]=interval->end[i];
            for(j=0;j<3;j++)next[i]+=local[j]*interval->end_matrix[j*3+i];
        }
    } else for(i=0;i<3;i++)next[i]=position[i]+interval->end[i]-interval->start[i];
    for(i=0;i<3;i++) {
        speed[i]=(next[i]-position[i])/seconds;
        if(!isfinite(next[i])||!isfinite(speed[i]))return RF_RANGE;
    }
    memcpy(target,next,sizeof(next));memcpy(velocity,speed,sizeof(speed));return RF_OK;
}

/* Ground acceptance has already placed the actor in the END frame. Recover
 * that point in the previous START frame, then negate the backwards velocity.
 * Reuse the existing basis convention/finite checks without advancing motion.
 * This also works for a newly acquired rotating support: no prior support
 * binding or cached carry velocity is required. Output is unchanged on error. */
int rf_scene_npc_support_interval_velocity(const scene_mover_interval *interval,
    const float accepted_position[3],float seconds,float velocity[3])
{
    scene_mover_interval reverse;float previous[3],backwards[3],result[3];uint32_t i;int status;
    if(!interval||!accepted_position||!velocity)return RF_RANGE;
    reverse=*interval;
    memcpy(reverse.start,interval->end,12);memcpy(reverse.end,interval->start,12);
    memcpy(reverse.matrix,interval->end_matrix,36);memcpy(reverse.end_matrix,interval->matrix,36);
    status=rf_scene_npc_support_interval_target(&reverse,accepted_position,seconds,previous,backwards);
    if(status)return status;
    for(i=0;i<3;i++)result[i]=-backwards[i];
    memcpy(velocity,result,12);return RF_OK;
}

/* Called by campaign_npc_support_contact AFTER rf_physics_support_accept
 * assigns body.position and the newly accepted support.handle. Collision's
 * contact.velocity is only a mover-center velocity; replacing a rotational
 * carry cache with it would erase the actor's launch velocity every grounded
 * tick. Static, translating, fragment and passive contacts retain their
 * existing semantics. Do not inspect piece_support: it can still name the old
 * contact until the caller publishes its new fragment binding after this call. */
static int campaign_npc_rotating_support_contact(const campaign_npc_body *owner,
    const float contact[3],uint32_t moving,float velocity[3])
{
    float result[3]={0};uint32_t i;int status;
    if(!owner||!velocity||moving>1||(moving&&!contact))return RF_RANGE;
    if(moving)for(i=0;i<3;i++) {
        if(!isfinite(contact[i]))return RF_FORMAT;result[i]=contact[i];
    }
    if(moving&&owner->support.handle&&campaign_mover_intervals&&campaign_mover_interval_seconds>0) {
        if(campaign_mover_count&&!campaign_mover_wrappers)return RF_FORMAT;
        for(i=0;i<campaign_mover_count;i++) {
            const scene_mover_interval *interval=campaign_mover_intervals+i;
            const rf_group_registered_mover *mover=campaign_mover_wrappers+i;
            if(interval->handle!=owner->support.handle||!(interval->changed&2u))continue;
            if(mover->handle!=interval->handle||!mover->pose||
               rf_object_registry_lookup(&campaign_registry,interval->handle)!=mover)return RF_NOT_FOUND;
            status=rf_scene_npc_support_interval_velocity(interval,owner->body.state.position,
                campaign_mover_interval_seconds,result);
            if(status){rf_scene_npc_rotating_support[7]=(uint32_t)status;return status;}
            break;
        }
    }
    memcpy(velocity,result,12);return RF_OK;
}

/* A grounded actor may carry a restored support handle. Resolve that full
 * registry generation afresh; never infer attachment from geometric proximity.
 * Fragment/passive-vehicle support continues through its existing path. */
static int campaign_npc_rotating_support_target(campaign_npc_body *owner,
    float elapsed,float target[3])
{
    uint32_t i;int status;
    if(!owner||!target||!isfinite(elapsed)||elapsed<=0)return RF_RANGE;
    if(owner->piece_support.tag||!owner->support.handle||
       !campaign_mover_intervals||campaign_mover_interval_seconds<=0)return RF_OK;
    for(i=0;i<campaign_mover_count;i++) {
        const scene_mover_interval *interval=campaign_mover_intervals+i;
        const rf_group_registered_mover *mover=campaign_mover_wrappers+i;
        if(interval->handle!=owner->support.handle||!(interval->changed&2u))continue;
        if(mover->handle!=interval->handle||
           rf_object_registry_lookup(&campaign_registry,interval->handle)!=mover)return RF_NOT_FOUND;
        /* The existing scene runs one full-interval carry per actor. Reject a
         * future substep caller rather than replaying the entire rotation. */
        if(elapsed!=campaign_mover_interval_seconds)return RF_RANGE;
        ++rf_scene_npc_rotating_support[0];
        status=rf_scene_npc_support_interval_target(interval,owner->body.state.position,
            elapsed,target,owner->support_velocity);
        if(status){rf_scene_npc_rotating_support[7]=(uint32_t)status;return status;}
        ++rf_scene_npc_rotating_support[1];rf_scene_npc_rotating_support[2]=owner->registration.handle;
        rf_scene_npc_rotating_support[3]=interval->handle;
        memcpy(rf_scene_npc_rotating_support+4,target,12);return RF_OK;
    }
    return RF_OK;
}
#endif
