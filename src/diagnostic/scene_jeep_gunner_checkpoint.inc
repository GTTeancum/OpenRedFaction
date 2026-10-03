#ifndef RF_SCENE_JEEP_GUNNER_CHECKPOINT_INCLUDED
#define RF_SCENE_JEEP_GUNNER_CHECKPOINT_INCLUDED
/* After driller_checkpoint_live: all seat/vehicle/world player stages exist.
 * RFNS2 keeps the16-byte header and24-byte rows. Jeep control word1 denotes
 * an active NPC driver with a simultaneous player gunner; turret control bits
 * keep RFNS1 meaning. RFNS1 never acquires this new exception implicitly.
 * RFNS3 admits one inactive former driver row with control1: a living player
 * remains gunner after the original NPC's death. Internal player_gunner2
 * distinguishes this from RFNS2's live-driver1; no NPC is reattached.
 * RFNS decoding retains player_gunner in scene_npc_seat_saved; all live publication is
 * after complete world admission. No board events or vehicle-use callbacks.
 */
struct scene_jeep_gunner_checkpoint_stage {
    scene_vehicle_combat_candidate combat;
    scene_vehicle_combat_publish_data host;
    scene_vehicle_possession session;
    scene_vehicle_player_control player;
    rf_look_pose exit_look;
    rf_physics_body_state body;
    rf_group_attached_pose group;
    int32_t seat;
    uint32_t actor,actor_uid,host_uid,admitted,unpiloted;
    float npc_position[3];
};
/* restores,host UID,NPC UID,host handle,NPC handle,player handle,role,
 * real driver,seat0,seat1,player parent,route active,index,event,ammo,
 * host xyz,velocity xyz,player xyz,NPC xyz,aim pitch/yaw,
 * exit body angles3,exit eye angles3,session active,live NPC pair,error */
uint32_t rf_scene_jeep_gunner_restore[38];
static int scene_jeep_gunner_checkpoint_capture_uid(const scene_stream *s,uint32_t *uid)
{
    const scene_driller_runtime *r=s?s->driller_runtime:NULL;uint32_t i,matches=0,unpiloted;
    if(!uid)return RF_RANGE;*uid=0;
    if(!r||!scene_jeep_npc_gunner_active(&r->entry))return RF_OK;
    if(scene_jeep_npc_gunner_valid(&r->entry)||
       !r->entry.host.alive||r->jeep_seat.role!=1||!r->player.saved||
       campaign_player_damage.state.effects.health<=0)return RF_NOT_FOUND;
    unpiloted=r->entry.host.driver==UINT32_MAX;
    if(unpiloted?r->entry.occupant!=-1:!scene_jeep_npc_gunner_driver(&r->entry))return RF_NOT_FOUND;
    for(i=0;i<scene_npc_seat_count;i++) {
        const scene_npc_seat_binding *b=scene_npc_seats+i;campaign_npc_body *o;
        if(b->kind!=SCENE_NPC_SEAT_JEEP||b->host!=r->entry.host.handle||
           (unpiloted?b->active:(!b->active||b->actor!=r->entry.host.driver)))continue;
        o=scene_npc_seat_actor(b);
        if(!o||!isfinite(o->damage.effects.health)||b->host_uid!=(uint32_t)campaign_authored_vehicle_uid)return RF_NOT_FOUND;
        if(unpiloted){
            if(o->damage.effects.health>0||o->view.linked_handle!=-1||b->occupant!=-1||
               o->ai_mode.action_280==13)return RF_NOT_FOUND;
        }else if(o->damage.effects.health<=0||o->ai_mode.action_280!=13||
           (o->view.flags_810&1u)||(o->object_flags&(2u|0x4000u)))return RF_NOT_FOUND;
        *uid=(uint32_t)campaign_seeds.records.items[b->slot].record.uid;
        if(!*uid||++matches!=1)return RF_FORMAT;
    }
    return matches==1?RF_OK:RF_NOT_FOUND;
}
static uint32_t scene_jeep_gunner_checkpoint_capture_allowed(const scene_stream *s)
{uint32_t uid=0;return !scene_jeep_gunner_checkpoint_capture_uid(s,&uid)&&uid!=0;}
/* Called per decoded RFNS row. The parent retains original UID/tag/kind match
 * and duplicate-actor checks; this only gives the versioned control word its
 * narrow meaning. No word may request both seat roles or use arbitrary bits. */
static int scene_jeep_gunner_checkpoint_control(uint32_t version,uint32_t kind,
    uint32_t active,uint32_t word,uint32_t *owns_flag,uint32_t *gunner)
{
    if(!owns_flag||!gunner)return RF_RANGE;*owns_flag=*gunner=0;
    if((version!=1&&version!=2&&version!=3)||active>1||word>1)return RF_FORMAT;
    if(kind==SCENE_NPC_SEAT_TURRET){if(!active&&word)return RF_FORMAT;*owns_flag=word;return RF_OK;}
    if(kind!=SCENE_NPC_SEAT_JEEP)return RF_FORMAT;
    if(!word)return RF_OK;
    if(version==2&&active){*gunner=1;return RF_OK;}
    if(version==3&&!active){*gunner=2;return RF_OK;}
    return RF_FORMAT;
}
static int scene_jeep_gunner_checkpoint_wire_admit(const scene_npc_seat_checkpoint_stage *s,uint32_t version)
{
    uint32_t count=0;if(!s||(version!=1&&version!=2&&version!=3))return RF_FORMAT;
    for(uint32_t i=0;i<s->count;i++) {
        const scene_npc_seat_saved *v=s->rows+i;
        if(v->player_gunner){
            if(v->binding.kind!=SCENE_NPC_SEAT_JEEP||v->owns_host_flag||
               (version==2?(v->player_gunner!=1||!v->active):
                version==3?(v->player_gunner!=2||v->active):1))return RF_FORMAT;
            ++count;
        }
    }
    return count==(version>=2?1u:0u)?RF_OK:RF_FORMAT;
}
/* Replaces only the Jeep occupied-host prohibition inside RFNS hosts(). */
static int scene_jeep_gunner_checkpoint_host(const scene_npc_seat_saved *v,const scene_vehicle_checkpoint_record *vehicle)
{
    if(!v||v->binding.kind!=SCENE_NPC_SEAT_JEEP||(!v->active&&v->player_gunner!=2)||!vehicle||vehicle->profile!=3||
       !vehicle->value.jeep.vehicle.alive)return RF_FORMAT;
    if(!v->player_gunner)return vehicle->value.jeep.vehicle.player_occupied?RF_FORMAT:RF_OK;
    if((v->player_gunner==1?!v->active:v->player_gunner==2?v->active:1)||vehicle->value.jeep.vehicle.player_occupied!=1||vehicle->value.jeep.role!=1||
       v->binding.host_uid!=(uint32_t)campaign_authored_vehicle_uid||
       v->binding.host!=campaign_authored_vehicle_handle)return RF_FORMAT;
    return RF_OK;
}
/* Parent invokes after saved player seat placement has passed, only for the
 * player/NPC pair. Both candidate poses are fixed to different authored tags;
 * this never relaxes world or third-party overlap admission. */
static uint32_t scene_jeep_gunner_checkpoint_pair(const scene_npc_seat_checkpoint_stage *s,
    const scene_vehicle_checkpoint_record *vehicle,uint32_t npc_uid)
{
    const scene_npc_seat_saved *v=scene_npc_seat_checkpoint_find(s,npc_uid);
    return v&&v->player_gunner==1&&v->active&&v->pose_ready&&!scene_jeep_gunner_checkpoint_host(v,vehicle);
}
/* Only the saved unpiloted player's exact former-driver corpse may reuse the
 * existing quarter-unit authored-seat contact exception. Ordinary detached
 * corpses and live passengers remain subject to normal player overlap rules. */
static uint32_t scene_jeep_gunner_checkpoint_dead_pair(const scene_npc_seat_checkpoint_stage *seats,
    const scene_npc_checkpoint_restore_stage *npcs,const scene_vehicle_checkpoint_record *vehicle,uint32_t index)
{
    const scene_npc_seat_saved *v;
    if(!npcs||index>=npcs->count)return 0;
    v=scene_npc_seat_checkpoint_find(seats,npcs->entries[index].saved.uid);
    if(!v||v->active||v->player_gunner!=2||!v->pose_ready||
       scene_jeep_gunner_checkpoint_host(v,vehicle))return 0;
    return scene_npc_jeep_seat_save_dead_contact(seats,npcs,vehicle,v->binding.host_uid,index);
}
static uint32_t scene_jeep_gunner_checkpoint_bytes(const scene_jeep_gunner_checkpoint_stage *s)
{return s?(uint32_t)sizeof(*s):0;}
static void scene_jeep_gunner_checkpoint_close(scene_jeep_gunner_checkpoint_stage **s)
{if(s&&*s){free(*s);*s=NULL;}}
/* All fallible vehicle/body/look work precedes ALL owner publication. First
 * pass admits fresh on-foot runtime plus its real authored NPC driver; an
 * already-seated live player remains rejected, including in-session gunner.
 * world_player_prepare and RFNS admit must have completed before this call. */
static int scene_jeep_gunner_checkpoint_prepare(scene_stream *stream,
    const scene_npc_seat_checkpoint_stage *seats,const scene_vehicle_checkpoint_record *vehicle,
    const scene_world_player_stage *player,uint32_t budget,scene_jeep_gunner_checkpoint_stage **out)
{
    scene_jeep_gunner_checkpoint_stage *s=NULL;const scene_npc_seat_saved *seat=NULL;
    scene_driller_runtime *r;const scene_npc_checkpoint_restore_entry *npc=NULL;
    scene_vehicle_player_control saved;uint32_t i;int status;
    if(!stream||!player||!out||*out)return RF_RANGE;
    if(!seats)return RF_OK;
    for(i=0;i<seats->count;i++)if(seats->rows[i].player_gunner){if(seat)return RF_FORMAT;seat=seats->rows+i;}
    if(!seat)return RF_OK;
    status=scene_jeep_gunner_checkpoint_host(seat,vehicle);if(status)return status;
    r=stream->driller_runtime;
    if(!seats->admitted||!seat->pose_ready||!r||!player->npcs||
       player->player.player.health<=0||r->entry.session.active||r->player.saved||
       r->entry.gunner_occupant!=-1||r->entry.view.occupants!=r->entry.occupants||
       r->entry.view.occupant_count!=2||r->entry.host.handle!=seat->binding.host)return RF_NOT_FOUND;
    for(i=0;i<player->npcs->count;i++)if(player->npcs->entries[i].saved.uid==seat->actor_uid){npc=player->npcs->entries+i;break;}
    if(!npc||npc->saved.retired||npc->handle!=seat->binding.actor)return RF_FORMAT;
    if(seat->player_gunner==2){
        if(seat->active||!npc->saved.dead_pose||npc->saved.health>0||npc->saved.ai_mode==13)return RF_FORMAT;
    }else {
        if(!seat->active||npc->saved.dead_pose||npc->saved.health<=0||npc->saved.ai_mode!=13)return RF_FORMAT;
        for(i=0;i<3;i++)if(fabsf(npc->published[i]-seat->pose[9+i])>.001f)return RF_FORMAT;
    }
    if(sizeof(*s)>budget)return RF_RANGE;
    s=calloc(1,sizeof(*s));if(!s)return RF_IO;
    status=scene_jeep_checkpoint_stage_rigid(&r->physics.state,&vehicle->value.jeep,&s->combat);if(status)goto fail;
    s->combat.vehicle.player_occupied=0;
    status=scene_vehicle_combat_parked_prepare(stream,&s->combat,&s->host);if(status)goto fail;
    status=scene_jeep_seat_index(stream->driller,1,&s->seat);if(status)goto fail;
    memcpy(s->exit_look.state.body_angles,player->player.body_angles,12);
    memcpy(s->exit_look.state.eye_angles,player->player.eye_angles,12);
    status=rf_look_update_pose(&s->exit_look.state,1.f,scene_step_seconds,&s->exit_look);if(status)goto fail;
    saved=r->entry.player;saved.host=UINT32_MAX;saved.control=saved.handle;
    saved.weapon=(int32_t)player->player.player.weapon;saved.control_flags=r->entry.player_view->flags_810;
    memcpy(saved.pose.position,player->player.position,12);memcpy(saved.pose.basis,s->exit_look.body_orientation,36);
    if(!scene_vehicle_pose_valid(&saved.pose)){status=RF_FORMAT;goto fail;}
    s->session.active=1;s->session.host=r->entry.host.handle;s->session.driver=saved.handle;s->session.saved=saved;
    s->player=saved;s->player.host=s->player.control=r->entry.host.handle;memcpy(s->player.pose.basis,player->basis,36);
    s->body=player->body;s->group=rf_scene_actor_pose;
    memset(s->body.velocity,0,12);memset(s->body.vector_138,0,12);
    status=rf_physics_body_prepare_contact(&s->body);if(status)goto fail;
    status=rf_group_pose_set_position(&s->group,player->player.position);if(status)goto fail;
    memset(s->group.velocity,0,12);memcpy(s->group.input_matrix,player->basis,36);
    memcpy(s->group.output_matrix,player->basis,36);memcpy(s->group.pending_matrix,player->basis,36);
    memcpy(s->body.bounds.minimum,s->group.minimum,12);memcpy(s->body.bounds.maximum,s->group.maximum,12);
    s->actor=seat->binding.actor;s->actor_uid=seat->actor_uid;s->host_uid=seat->binding.host_uid;
    memcpy(s->npc_position,npc->published,12);s->unpiloted=seat->player_gunner==2;
    s->admitted=1;*out=s;return RF_OK;
fail:free(s);return status;
}
/* Assignment-only host half immediately AFTER RFNS detach, BEFORE RFNS assign
 * and route mode publication. Reuses the extracted existing combat commit. */
static void scene_jeep_gunner_checkpoint_host_assign(scene_stream *stream,const scene_jeep_gunner_checkpoint_stage *s)
{
    if(!s||!s->admitted)return;
    scene_vehicle_combat_publish_staged(stream,&s->combat,&s->host);
}
/* Assignment-only player half after ordinary player state is assigned, in
 * place of generic occupied-vehicle publish. RFNS already owns real driver0.
 * No live possess() call, route restart, fresh action13, or callbacks. */
static void scene_jeep_gunner_checkpoint_player_assign(scene_stream *stream,const scene_jeep_gunner_checkpoint_stage *s)
{
    scene_driller_runtime *r;uint32_t *p=rf_scene_jeep_gunner_restore;
    if(!s||!s->admitted)return;r=stream->driller_runtime;
    r->entry.session=s->session;r->entry.player=s->player;r->entry.gunner_occupant=(int32_t)s->player.handle;
    if(s->unpiloted){r->entry.host.driver=UINT32_MAX;r->entry.occupant=-1;}
    r->entry.player_view->linked_handle=(int32_t)r->entry.host.handle;r->entry.held=1;
    r->player.saved=1;r->player.saved_look=s->exit_look;
    r->jeep_seat.role=1;r->jeep_seat.held=1;stream->driller->seat=s->seat;
    memcpy(r->entry.eye_offset,scene_driller_player_eye_offset(),12);
    scene_actor_body.state=s->body;rf_scene_actor_pose=s->group;actor_look=s->exit_look;
    memcpy(actor_look.body_orientation,s->player.pose.basis,36);memcpy(actor_look.eye_orientation,s->player.pose.basis,36);
    memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));campaign_support_handle=0;
    memset(&campaign_piece_support,0,sizeof(campaign_piece_support));stream->player_checkpoint_look=0;
    stream->apc_aim_active=1;r->damage.eject_pending=0;r->last_frame=UINT32_MAX;
    rf_scene_vehicle_state[3]=1;
    ++p[0];p[1]=s->host_uid;p[2]=s->actor_uid;p[3]=r->entry.host.handle;p[4]=s->actor;p[5]=s->player.handle;
    p[6]=1;p[7]=r->entry.host.driver;p[8]=(uint32_t)r->entry.occupant;p[9]=(uint32_t)r->entry.gunner_occupant;
    p[10]=(uint32_t)r->entry.player_view->linked_handle;p[11]=campaign_vehicle_route.active;
    p[12]=campaign_vehicle_route.index;p[13]=campaign_vehicle_route.event;p[14]=(uint32_t)stream->apc_primary.reserve;
    memcpy(p+15,r->physics.state.position,12);memcpy(p+18,r->physics.state.velocity,12);
    memcpy(p+21,s->player.pose.position,12);memcpy(p+24,s->npc_position,12);
    memcpy(p+27,stream->apc_aim.angles,8);memcpy(p+29,s->exit_look.state.body_angles,12);memcpy(p+32,s->exit_look.state.eye_angles,12);
    p[35]=r->entry.session.active;p[36]=scene_jeep_npc_gunner_driver(&r->entry);
    p[37]=(uint32_t)scene_jeep_npc_gunner_valid(&r->entry);
}
#endif
