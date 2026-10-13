#ifndef RF_SCENE_TACTICAL_QUALIFICATION_INCLUDED
#define RF_SCENE_TACTICAL_QUALIFICATION_INCLUDED
/* Immutable cohort. Direct3 and Evasive1 share this ordinary behavior.
 * Stand Ground2 remains separate. This is independent of blind-duration
 * metadata and does not require Waiting when a real default patrol exists. */
static int scene_tactical_source(uint32_t slot)
{
    const rf_entity_seed_class *c;rf_level_entity_ai ai;
    char held[2][RF_LEVEL_NAME_CAPACITY];uint32_t cls;int status,style;
    static const int32_t overrides[4]={-1,1,3,2};
    if(!campaign_seeds.items||!campaign_seeds.records.items||!campaign_seeds.classes||
       slot>=campaign_seeds.records.count)return RF_RANGE;
    cls=campaign_seeds.items[slot].class_index;
    if(!scene_noise_classes.valid||scene_noise_classes.count!=campaign_seeds.class_count||
       cls>=scene_noise_classes.count||cls>=640u)return RF_FORMAT;
    c=campaign_seeds.classes+cls;
    if((c->physics.flags&0x20019u)!=0x20019u||(c->physics.flags&(0x200u|0x400u|0x800u|0x1000u|0x2000u|0x400000u))||
       (c->physics.flags2&0x100u)||c->physics.use_kind==1||c->model_kind!=2||
       !isfinite(c->physics.mass)||c->physics.mass<=0||c->physics.movement_index>=16||
       (campaign_modes[c->physics.movement_index].index!=1&&campaign_modes[c->physics.movement_index].index!=3)||
       campaign_seeds.items[slot].spawn.seat_host_uid!=-1)return RF_NOT_FOUND;
    status=rf_level_entity_ai_read(campaign_seeds.records.items+slot,&ai);if(status)return status;
    if((ai.mode!=1&&ai.mode!=2)||ai.attack_style>=4)return RF_NOT_FOUND;
    style=overrides[ai.attack_style];if(style<0)style=scene_noise_classes.style[cls];
    if(style!=1&&style!=3)return RF_NOT_FOUND;
    status=rf_level_entity_held_clutter_read(campaign_seeds.records.items+slot,held);if(status)return status;
    for(uint32_t j=0;j<2;j++)if(held[j][0]&&!scene_npc_loadout_equal(held[j],"none"))return RF_NOT_FOUND;
    return RF_OK;
}
/* Persistent identity qualification. Hidden/disarmed/frozen owners may still
 * retain recovery. Current source retirement is handled by the lifecycle
 * caller; a temporary service refusal must never erase this receipt. */
static int scene_tactical_owner(uint32_t slot)
{
    const campaign_npc_body *o;const campaign_model_owner *m;const rf_entity_pose *p;int status;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    status=scene_tactical_source(slot);if(status)return status;
    o=campaign_npc_bodies+slot;
    if(scene_npc_reload_identity(slot,o)||!scene_npc_reload_standalone(o)||
       o->registration.handle==campaign_player_object.handle||!o->persistence_registered||
       !o->body.allocated_bytes||!isfinite(o->body.state.mass)||o->body.state.mass<=0||
       !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||(o->view.flags_810&1u)||
       scene_npc_seat_attached(o->registration.handle)||
       !campaign_model_owners||slot>=campaign_model_owner_count||!campaign_poses.items||slot>=campaign_poses.count)return RF_NOT_FOUND;
    m=campaign_model_owners+slot;p=campaign_poses.items+slot;
    if(!m->registration.loaded||m->owned||m->pose!=p||m->registration.active!=&p->playback.completion.active||
       !p->matrices||!p->generations||!p->bone_count||p->skeleton!=o->selection.mapping.skeleton||
       !m->registration.next||!m->registration.previous||m->registration.next->previous!=&m->registration||
       m->registration.previous->next!=&m->registration)return RF_NOT_FOUND;
    return RF_OK;
}
/* Temporary eligibility only. Full ranged weapon binding and actual in-range
 * target/LOS qualification remain with the existing ordinary combat traversal.
 * Constructor controller_handle is a mover backlink, not exclusive control. */
static uint32_t scene_tactical_service_allowed(const campaign_npc_body *o)
{
    if(!o||!o->combat_alert||o->combat_scripted>2||o->script_shoot.active||
       o->view.weapons[0]<0||o->view.weapons[0]==campaign_riot_id||
       scene_blind_weapon(o->view.weapons[0])!=RF_OK||
       o->movement_slot>=16||(campaign_modes[o->movement_slot].index!=1&&campaign_modes[o->movement_slot].index!=3)||
       (o->object_flags&(2u|0x4000u))||campaign_npc_ai_suppressed(o)||
       o->scripted_physics||o->script_animation.freeze||
       o->view.action_520!=o->ai_mode.action_280||o->ai_mode.action_280==1||
       o->ai_mode.action_280==7||o->ai_mode.action_280==13||o->ai_mode.action_280==14||
       o->flight.entered||scene_guard_retreat_active(o)||o->guard_retreat_fault||
       o->alarm.present||o->alarm_fault||o->medic.pending||scene_noise_active(o)||
       campaign_script_movement_owns(o)||campaign_cutscene_runtime.active||campaign_endgame.phase||
       scene_blind_paid(o->registration.handle))return 0;
    return 1;
}
#endif
