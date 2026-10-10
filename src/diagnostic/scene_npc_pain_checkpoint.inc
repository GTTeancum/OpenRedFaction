#ifndef RF_SCENE_NPC_PAIN_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_PAIN_CHECKPOINT_INCLUDED
/* RFNC15 owns ordinary non-burning pain continuation. Include before NPC
 * capture/restore, after the live NPC, motion and shared pain RNG owners.
 * Original428740 selects actions22/23, owns timer830 and installs timer744
 * from the real clip duration. The retained attack loop consumes timer744.
 * No hit, weapon reset, action start or sound is replayed by this adapter. */
static int scene_npc_pain_checkpoint_action(int32_t action,
    const rf_entity_motion_selection *selection,int32_t *motion)
{
    const rf_entity_playback_model *model;uint32_t skeleton;
    if(!selection||!motion||(action!=-1&&action!=22&&action!=23))return RF_FORMAT;
    *motion=-1;if(action<0)return RF_OK;
    skeleton=selection->mapping.skeleton;*motion=selection->mapping.actions[action];
    if(*motion<0||!campaign_playback_resources.models||
       skeleton>=campaign_playback_resources.model_count)return RF_FORMAT;
    model=campaign_playback_resources.models+skeleton;
    if(!model->resources||(uint32_t)*motion>=model->count||model->resources[*motion].looping)return RF_FORMAT;
    return RF_OK;
}
static int scene_npc_pain_checkpoint_capture(const campaign_npc_body *owner,
    const rf_entity_pose *pose,int32_t now,rf_npc_checkpoint_pain *out)
{
    rf_npc_checkpoint_pain next={0};int32_t deadlines[3],motion=-1;uint32_t i,needed=0;int status;
    if(!owner||!out||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    /* RFAP4 is already the sole burn/pain owner. Retired/terminal actors retain
     * their separate death admission; this does not waive pending death pain. */
    if(!owner->registration.view||owner->damage.effects.health<=0||owner->damage.effects.burn){*out=next;return RF_OK;}
    /* The next pain call reuses828 until ordinary target-aim retires it, even
     * after the old clip has left playback. Keep that authored action choice. */
    needed=owner->pain.selected_action!=-1;
    deadlines[0]=owner->pain.ai_timer;deadlines[1]=owner->pain.animation_lock;deadlines[2]=owner->pain.cooldown;
    for(i=0;i<3;i++){
        if(deadlines[i]<0){next.remaining[i]=-1;needed=1;continue;}
        status=rf_timer_remaining(deadlines[i],now,next.remaining+i);if(status)return status;
        if(next.remaining[i]<0)next.remaining[i]=0;
        needed|=next.remaining[i]>0;
    }
    if(!needed){memset(out,0,sizeof(*out));return RF_OK;}
    if(!pose||pose->skeleton!=owner->selection.mapping.skeleton)return RF_FORMAT;
    status=scene_npc_pain_checkpoint_action(owner->pain.selected_action,&owner->selection,&motion);if(status)return status;
    next.present=1;next.action=owner->pain.selected_action;next.random=combat_pain_random.value;
    *out=next;return RF_OK;
}
/* This predicate only admits the one mapped non-looping pain action. Other
 * clips retain their existing script/combat/death ownership checks. */
static int scene_npc_pain_checkpoint_clip(const rf_npc_checkpoint_pain *pain,
    const rf_entity_motion_selection *selection,int32_t motion)
{
    return pain->present&&(pain->action==22||pain->action==23)&&
        motion>=0&&selection->mapping.actions[pain->action]==motion;
}
static int scene_npc_pain_checkpoint_prepare(const rf_npc_checkpoint_record *saved,
    const rf_entity_motion_selection *selection,int32_t now,int32_t deadlines[3])
{
    const rf_npc_checkpoint_pain *pain;int32_t motion;uint32_t i;int status;
    if(!saved||now<0||now>RF_TIMER_PERIOD||!deadlines)return RF_RANGE;pain=&saved->pain;
    /* Legacy/absent state is expired, not disabled: deadline-1 would suppress
     * RF_PAIN_COOLDOWN forever. In-session loads must clear a later reaction. */
    for(i=0;i<3;i++)deadlines[i]=now;
    if(!pain->present)return RF_OK;
    if(saved->retired||saved->dead_pose||saved->health<=0)return RF_FORMAT;
    status=scene_npc_pain_checkpoint_action(pain->action,selection,&motion);if(status)return status;
    for(i=0;i<3;i++){
        if(pain->remaining[i]< -1||pain->remaining[i]>RF_TIMER_PERIOD/2)return RF_FORMAT;
        if(pain->remaining[i]<0)deadlines[i]=-1;
        else {status=rf_timer_set(deadlines+i,now,pain->remaining[i]);if(status)return status;}
    }
    return RF_OK;
}
/* assignments, last UID, presence, three remaining deadlines, action, RNG.
 * Read-only evidence for the parent-owned scheduled Xbox check. */
uint32_t rf_scene_npc_pain_restored[8];
static void scene_npc_pain_checkpoint_assign(campaign_npc_body *owner,
    const rf_npc_checkpoint_record *saved,const int32_t deadlines[3])
{
    owner->pain.ai_timer=deadlines[0];owner->pain.animation_lock=deadlines[1];owner->pain.cooldown=deadlines[2];
    owner->pain.selected_action=saved->pain.present?saved->pain.action:-1;
    if(!saved->pain.present)return;
    combat_pain_random.value=saved->pain.random;
    ++rf_scene_npc_pain_restored[0];rf_scene_npc_pain_restored[1]=saved->uid;
    rf_scene_npc_pain_restored[2]=saved->pain.present;
    for(uint32_t i=0;i<3;i++)rf_scene_npc_pain_restored[3+i]=(uint32_t)saved->pain.remaining[i];
    rf_scene_npc_pain_restored[6]=(uint32_t)saved->pain.action;rf_scene_npc_pain_restored[7]=saved->pain.random;
}
#endif
