/* Include before rf_scene_npc_fall. Cached support velocity is a separate
 * additive velocity in rf_physics_fall_propose, not an acceleration. On loss
 * retain that last world-space point velocity, but release its source handle
 * so later refreshes cannot overwrite it with the old host's new velocity. */
#ifndef RF_SCENE_NPC_SUPPORT_LIFECYCLE_INCLUDED
#define RF_SCENE_NPC_SUPPORT_LIFECYCLE_INCLUDED

uint32_t rf_scene_npc_support_lifecycle[8]; /* losses,static contacts,moving contacts,actor,retained XYZ,error */

int rf_scene_npc_support_loss(rf_physics_support_contact *support,
    const float inherited[3],uint32_t *body_flags)
{
    uint32_t i;
    if(!support||!inherited||!body_flags)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(inherited[i]))return RF_FORMAT;
    support->handle=0;support->material=-1;*body_flags&=~0x400000u;
    return RF_OK;
}

/* Called only after collision admits a new ground contact. On static ground
 * remove the old additive carry contribution; moving ground replaces it with
 * this contact's velocity, never adds it a second time to body.velocity. */
int rf_scene_npc_support_contact_velocity(float inherited[3],
    const float contact[3],uint32_t moving)
{
    float next[3]={0};uint32_t i;
    if(!inherited||moving>1||(moving&&!contact))return RF_RANGE;
    if(moving)for(i=0;i<3;i++) {
        if(!isfinite(contact[i]))return RF_FORMAT;
        next[i]=contact[i];
    }
    memcpy(inherited,next,sizeof(next));return RF_OK;
}

static int campaign_npc_support_release(campaign_npc_body *owner)
{
    int status;
    if(!owner)return RF_RANGE;
    status=rf_scene_npc_support_loss(&owner->support,owner->support_velocity,&owner->body.state.flags);
    if(status){rf_scene_npc_support_lifecycle[7]=(uint32_t)status;return status;}
    memset(&owner->piece_support,0,sizeof(owner->piece_support));owner->piece_reacquire=0;
    ++rf_scene_npc_support_lifecycle[0];rf_scene_npc_support_lifecycle[3]=owner->registration.handle;
    memcpy(rf_scene_npc_support_lifecycle+4,owner->support_velocity,12);return RF_OK;
}

static int campaign_npc_support_contact(campaign_npc_body *owner,
    const float velocity[3],uint32_t moving)
{
    int status;
    if(!owner)return RF_RANGE;
    status=rf_scene_npc_support_contact_velocity(owner->support_velocity,velocity,moving);
    if(status){rf_scene_npc_support_lifecycle[7]=(uint32_t)status;return status;}
    ++rf_scene_npc_support_lifecycle[moving?2:1];return RF_OK;
}
#endif
