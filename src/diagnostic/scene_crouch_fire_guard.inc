#ifndef RF_SCENE_CROUCH_FIRE_GUARD_INCLUDED
#define RF_SCENE_CROUCH_FIRE_GUARD_INCLUDED
static int scene_crouch_fire_guard_valid(const scene_crouch_fire_guard *g)
{
    const campaign_npc_body *o;
    if(!g)return RF_RANGE;if(!g->active)return RF_OK;
    /* Static owning headers and epoch precede every retained dereference. */
    if(g->epoch!=scene_robot_death_epoch||g->owners!=campaign_npc_bodies||
       g->slot>=campaign_npc_body_count||memcmp(&g->poses,&campaign_poses,sizeof(g->poses))||
       !campaign_npc_bodies||!campaign_poses.items||g->slot>=campaign_poses.count)return RF_NOT_FOUND;
    o=campaign_npc_bodies+g->slot;
    if(o->registration.handle!=g->handle||scene_tactical_owner(g->slot)||
       o->view.action_520!=g->action||o->view.weapons[0]!=g->weapon||
       (o->view.flags_810&0x400u)!=g->physical||o->view.flags_810!=g->actor_flags||
       o->unholster.stance_7bc!=g->intent||o->ai_mode.state_2b4!=g->behavior||
       o->stance_clock_7b4!=g->stamp||o->view.flags_7d0!=g->flags_7d0||
       o->object_flags!=g->object_flags||o->movement_slot!=g->movement_slot||
       memcmp(o->published,g->position,sizeof(g->position))||memcmp(o->eye_position,g->eye,sizeof(g->eye))||
       memcmp(&o->crouch,&g->receipt,sizeof(g->receipt))||
       memcmp(&o->selection,&g->selection,sizeof(g->selection))||
       memcmp(campaign_poses.items+g->slot,&g->pose,sizeof(g->pose)))return RF_NOT_FOUND;
    return RF_OK;
}
static int scene_crouch_owner_guard_capture(uint32_t slot,scene_crouch_fire_guard *g)
{
    const campaign_npc_body *o;int status;
    if(!g||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    g->active=0;o=campaign_npc_bodies+slot;
    status=scene_crouch_live_shape(slot,o);if(status)return status;
    status=scene_tactical_owner(slot);if(status)return status;
    g->epoch=scene_robot_death_epoch;g->active=1;g->slot=slot;g->handle=o->registration.handle;
    g->physical=o->view.flags_810&0x400u;g->action=o->view.action_520;g->weapon=o->view.weapons[0];
    g->intent=o->unholster.stance_7bc;g->behavior=o->ai_mode.state_2b4;g->stamp=o->stance_clock_7b4;
    g->flags_7d0=o->view.flags_7d0;g->object_flags=o->object_flags;g->movement_slot=o->movement_slot;
    g->actor_flags=o->view.flags_810;memcpy(g->position,o->published,sizeof(g->position));
    memcpy(g->eye,o->eye_position,sizeof(g->eye));
    g->owners=campaign_npc_bodies;g->poses=campaign_poses;g->receipt=o->crouch;
    g->selection=o->selection;g->pose=campaign_poses.items[slot];
    return RF_OK;
}
static int scene_crouch_fire_guard_capture(uint32_t slot,scene_crouch_fire_guard *g)
{
    if(!g||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    g->active=0;
    if(!(campaign_npc_bodies[slot].crouch.flags&SCENE_CROUCH_PRESENT))return RF_OK;
    return scene_crouch_owner_guard_capture(slot,g);
}
#endif
