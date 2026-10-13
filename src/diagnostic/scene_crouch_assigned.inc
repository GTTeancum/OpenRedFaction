#ifndef RF_SCENE_CROUCH_ASSIGNED_INCLUDED
#define RF_SCENE_CROUCH_ASSIGNED_INCLUDED
/* Pure post-assignment projection for the proposed independent late-close
 * lane. The caller must FIRST authenticate retained stage/scene lifetimes
 * and the immutable candidate snapshot. This is not a lifetime receipt and
 * is not sufficient alone to authorize final storage close. It permits only
 * the specific NPC fields published by restore_assign, while keeping old
 * physical sphere bytes and crouch policy until restore_finish. */
static int scene_crouch_assigned(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_checkpoint_restore_entry *e)
{
    const campaign_npc_body *o,*old;const rf_entity_pose *p;
    rf_motion_controller controller;rf_physics_support_contact support;
    int status;
    if(!stage||!e||e->crouch_shape_present>1)return RF_RANGE;
    if(!e->crouch_shape_present)return RF_OK;
    status=scene_crouch_prepared(stage,e);if(status)return status;
    if(!campaign_poses.items||e->slot>=campaign_poses.count||
       !campaign_model_owners||e->slot>=campaign_model_owner_count)return RF_FORMAT;
    o=campaign_npc_bodies+e->slot;old=&e->owner_before;p=campaign_poses.items+e->slot;
    if(o->body.allocated_bytes!=old->body.allocated_bytes||
       memcmp(&o->crouch,&old->crouch,sizeof(o->crouch))||
       o->unholster.stance_7bc!=old->unholster.stance_7bc||o->stance_clock_7b4!=old->stance_clock_7b4||
       ((o->view.flags_810^old->view.flags_810)&0x400u)||
       ((o->damage.effects.flags_810^old->damage.effects.flags_810)&0x400u)||
       memcmp(&o->body.state,e->crouch_retire_pending?&old->body.state:&e->body,sizeof(e->body))||
       memcmp(&o->room,e->crouch_retire_pending?&old->room:&e->room,sizeof(e->room))||
       memcmp(o->published,e->published,sizeof(e->published))||
       memcmp(o->previous,e->published,sizeof(e->published))||
       memcmp(o->eye_position,e->crouch_retire_pending?old->eye_position:e->eye,sizeof(e->eye))||
       memcmp(&o->selection,e->crouch_retire_pending?&old->selection:&e->selection,sizeof(e->selection))||
       o->object_flags!=(e->flags|(e->crouch_retire_pending?(2u|0x4000u):0u))||o->view.flags_7c!=o->object_flags||
       o->controller_handle!=e->controller_handle)return RF_FORMAT;
    support=e->crouch_retire_pending?old->support:e->support;
    if(e->saved.airborne.present){support.handle=0;support.material=-1;}
    if(memcmp(&o->support,&support,sizeof(support)))return RF_FORMAT;
    if(e->movement_resolved&&(memcmp(&o->movement,&e->movement,sizeof(e->movement))||
       o->movement_slot!=e->movement_slot||o->movement_orientation!=campaign_identity[0]))return RF_FORMAT;
    if(p->skeleton!=e->pose_before.skeleton||p->bone_count!=e->pose_before.bone_count||
       p->matrices!=e->pose_before.matrices||p->generations!=e->pose_before.generations||
       p->overrides!=e->pose_before.overrides||
       memcmp(&p->playback,&e->playback,sizeof(e->playback)))return RF_FORMAT;
    if(e->saved.animation_present)controller=e->saved.controller;
    else{memset(&controller,0,sizeof(controller));controller.override_state=-1;}
    if(memcmp(&p->controller,&controller,sizeof(controller)))return RF_FORMAT;
    if(e->bone_count){
        if(!p->matrices||!p->generations||!e->matrices||!e->generations||e->bone_count!=p->bone_count||
           memcmp(p->matrices,e->matrices,(size_t)e->bone_count*48)||
           memcmp(p->generations,e->generations,(size_t)e->bone_count*2))return RF_FORMAT;
    }else if(p->generations){
        for(uint32_t i=0;i<p->bone_count;i++)
            if(p->generations[i]!=(uint16_t)(p->playback.generation-1u))return RF_FORMAT;
    }
    return RF_OK;
}
#endif
