#ifndef RF_SCENE_ROBOT_DEATH_CHECKPOINT_NPC_INCLUDED
#define RF_SCENE_ROBOT_DEATH_CHECKPOINT_NPC_INCLUDED
/* RFNC29 dead_pose2 is a still-registered, clipless finalizer source, never
 * a corpse animation or a retired actor. RFAP13 must discharge its ownership
 * obligation before composed publication. No death/loot/audio is replayed. */
static int scene_robot_death_checkpoint_shape(const scene_robot_death_receipt *r)
{
    const scene_robot_death_profile *p;
    if(!r||r->state!=1||!r->sequence||r->sequence==UINT64_MAX||!r->uid||r->uid>(uint32_t)INT32_MAX||
       r->profile>=SCENE_ROBOT_DEATH_PROFILE_COUNT||!isfinite(r->health)||r->health>0)return RF_FORMAT;
    p=scene_robot_death_profiles+r->profile;
    if(!p->ready||p->class_index!=r->class_index||p->class_index>=campaign_seeds.class_count||
       !campaign_seeds.classes||campaign_seeds.classes[p->class_index].corpse.model[0]||
       !isfinite(r->damage)||r->damage!=p->damage||!isfinite(r->radius)||r->radius!=p->radius||
       r->clip!=p->clip||r->foley_group!=p->foley_group)return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(!isfinite(r->position[i])||fabsf(r->position[i])>100000.f)return RF_FORMAT;
    return RF_OK;
}
static int scene_robot_death_checkpoint_live(uint32_t slot)
{
    const campaign_npc_body *o;const scene_robot_death_receipt *r;const rf_campaign_object_record *key;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count||slot>=campaign_seeds.records.count)return RF_FORMAT;
    o=campaign_npc_bodies+slot;r=&o->robot_death;
    if(scene_robot_death_checkpoint_shape(r)||scene_robot_death_profile_at(slot)!=(int32_t)r->profile||
       campaign_seeds.items[slot].class_index!=r->class_index||
       (uint32_t)campaign_seeds.records.items[slot].record.uid!=r->uid||
       !scene_robot_death_identity(slot,r->handle)||!o->body.allocated_bytes||
       !o->persistence_registered||o->persistence_slot!=r->persistence||
       r->persistence>=rf_scene_defeated_actors.count||r->persistence>=RF_CAMPAIGN_ACTOR_SLOTS||
       !isfinite(o->damage.effects.health)||o->damage.effects.health>0||!(o->view.flags_810&1u)||
       o->death.action_824!=-1||o->death.deadline_4b8!=-1||o->death_item_fault||
       !campaign_model_owners||slot>=campaign_model_owner_count||campaign_model_owners[slot].owned||
       !campaign_poses.items||slot>=campaign_poses.count||o->script_once.count>16||
       o->view.linked_handle!=-1||o->view.occupant_count||o->attachment_75c!=-1||
       o->damage.effects.burn||o->script_animation.active||o->combat_reload_pending||
       o->firing.sound_81c!=-1||o->firing.sound_820!=-1||o->firing.effect_13d4!=-1)return RF_FORMAT;
    key=rf_scene_defeated_actors.items+r->persistence;
    if(key->retired||key->uid!=r->uid||key->level>=rf_scene_defeated_actors.level_count||
       key->level>=RF_CAMPAIGN_PICKUP_LEVELS||
       !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[key->level],campaign_current_level))return RF_FORMAT;
    for(uint32_t i=0;i<64;i++)if(o->firing.active[i])return RF_NOT_FOUND;
    return RF_OK;
}
static int scene_robot_death_checkpoint_npc_row(uint32_t slot,rf_npc_checkpoint_record *row)
{
    const campaign_npc_body *o=campaign_npc_bodies+slot;uint32_t p=o->persistence_slot;int status;
    status=scene_robot_death_checkpoint_live(slot);if(status)return status;
    status=scene_death_item_owner_admit(o,rf_scene_defeated_actors.item_drops+p);if(status)return status;
    if(!campaign_npc_ai_suppression_consistent(o))return RF_FORMAT;
    memset(row,0,sizeof(*row));row->uid=o->robot_death.uid;row->class_id=o->robot_death.class_index;
    row->dead_pose=2;row->death_action=-1;row->death_flags_810=o->view.flags_810;
    row->flags=o->object_flags&0x4004u;row->affiliation=o->damage.effects.affiliation;
    row->health=o->damage.effects.health;row->armor=o->damage.effects.armor;
    row->inventory=o->inventory;row->primary=o->view.weapons[0];row->secondary=o->view.weapons[1];
    row->ai_mode=o->ai_mode.action_280;row->ai_suppressed=campaign_npc_ai_suppressed(o);
    row->shield_disabled=!!(o->damage.effects.flags_814&0x20u);
    row->drop=rf_scene_defeated_actors.drops[p];row->item_drop=rf_scene_defeated_actors.item_drops[p];
    memcpy(row->position,o->published,12);row->yaw=o->look.body_angles[1];
    status=scene_actor_route_capture(o,0,&row->route);if(status)return status;
    return scene_medic_state_capture(o,&row->medic);
}
#endif
