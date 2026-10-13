#ifndef RF_SCENE_STAND_GROUND_COMBAT_INCLUDED
#define RF_SCENE_STAND_GROUND_COMBAT_INCLUDED
/*406b79 /406a28: runtime Stand Ground2 rejects pursuit/translation, retaining
 * facing and attacks.464acf maps RFL raw3 to2 (raw2 is Direct3). Read only
 * the existing immutable override; no class-default inference or new owner.
 * Include after reload identity and ordinary opposed-target qualification. */
static uint32_t scene_stand_ground_combat(const scene_stream *stream,uint32_t slot)
{
    const campaign_npc_body *o;const rf_entity_seed_class *c;
    const campaign_model_owner *model;const rf_entity_pose *pose;
    rf_level_entity_ai ai;uint32_t cls;float aim[3],health;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count||!campaign_seeds.items||
       !campaign_seeds.records.items||slot>=campaign_seeds.records.count||!campaign_seeds.classes)return 0;
    o=campaign_npc_bodies+slot;
    /* An alert with scripted0 uses the player's implicit target, including
     * the legacy hearing/damage path whose combat_target may still be0. */
    if(!o->combat_alert||o->combat_scripted>2||!campaign_enemy_opposed_live(o)||
       scene_npc_reload_identity(slot,o)||!scene_npc_reload_standalone(o)||
       o->registration.handle==campaign_player_object.handle||o->view.weapons[0]<0||
       !o->persistence_registered||o->movement_slot>=16||
       (campaign_modes[o->movement_slot].index!=1&&campaign_modes[o->movement_slot].index!=3)||
       o->view.action_520!=o->ai_mode.action_280||scene_noise_active(o)||
       o->ai_mode.action_280==1||o->ai_mode.action_280==7||o->ai_mode.action_280==13||o->ai_mode.action_280==14||
       o->flight.entered||scene_guard_retreat_active(o)||o->guard_retreat_fault||o->medic.pending||
       o->alarm.present||o->alarm_fault||o->script_animation.freeze||
       o->scripted_physics||campaign_npc_ai_suppressed(o)||scene_npc_seat_attached(o->registration.handle))return 0;
    cls=campaign_seeds.items[slot].class_index;if(cls>=campaign_seeds.class_count)return 0;
    c=campaign_seeds.classes+cls;
    if((c->physics.flags&0x20011u)!=0x20011u||(c->physics.flags&(0x200u|0x2000u|0x400000u))||
       (c->physics.flags2&0x100u)||c->physics.use_kind==1||c->model_kind!=2||
       !isfinite(c->physics.mass)||c->physics.mass<=0||!isfinite(o->body.state.mass)||o->body.state.mass<=0||
       c->physics.movement_index>=16||
       (campaign_modes[c->physics.movement_index].index!=1&&campaign_modes[c->physics.movement_index].index!=3)||
       campaign_seeds.items[slot].spawn.seat_host_uid!=-1||
       !campaign_model_owners||slot>=campaign_model_owner_count||!campaign_poses.items||slot>=campaign_poses.count)return 0;
    model=campaign_model_owners+slot;pose=campaign_poses.items+slot;
    if(!model->registration.loaded||model->owned||model->pose!=pose||
       model->registration.active!=&pose->playback.completion.active||!pose->matrices||!pose->generations||
       !pose->bone_count||pose->skeleton!=o->selection.mapping.skeleton||
       !model->registration.next||!model->registration.previous||
       model->registration.next->previous!=&model->registration||model->registration.previous->next!=&model->registration)return 0;
    if(rf_level_entity_ai_read(campaign_seeds.records.items+slot,&ai)||ai.attack_style!=3)return 0;
    if(!o->combat_scripted||o->combat_target==campaign_player_object.handle)
        return campaign_player_object.handle&&campaign_player_object.handle!=UINT32_MAX&&
            campaign_player_object.view==&campaign_player_view&&
            rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object&&
            rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view&&
            isfinite(campaign_player_damage.state.effects.health)&&campaign_player_damage.state.effects.health>0;
    if(!o->combat_target||o->combat_target==UINT32_MAX||o->combat_target==o->registration.handle)return 0;
    if(scene_campaign_vehicle_target(stream,o->combat_target,aim,&health))return 1;
    for(uint32_t j=0;j<campaign_npc_body_count;j++)
        if(campaign_npc_bodies[j].registration.handle==o->combat_target)
            return campaign_enemy_opposed_live(campaign_npc_bodies+j);
    return 0;
}
/* Call only after genuine combat qualification. follow2 alone also describes
 * action6 noise; event/default routes and every combat/weapon bank survive. */
static void scene_stand_ground_stop_route(campaign_npc_body *o)
{
    if(o->script_move.active&&o->script_move.follow==2&&o->script_move.origin==SCENE_MOVE_ORIGIN_NONE)
        campaign_pursuit_stop(o);
}
#endif
