#ifndef RF_SCENE_GUARD_RETREAT_TICK_INCLUDED
#define RF_SCENE_GUARD_RETREAT_TICK_INCLUDED
/* Included after ground/sweep/stance and the shared locomotion projection.
 * Pain pauses navigation, not the elapsed250 clock. A return does only a
 * neutral physical step; ordinary service resumes at its next iteration. */
static int scene_guard_retreat_tick(scene_stream *s,campaign_npc_body *o,uint32_t index,
    float elapsed,uint32_t frame,scene_npc_landing_receipt *landing)
{
    scene_medic_flight_threat_view threat;scene_medic_flight_nav_result nav={0};
    rf_entity_pose *pose;rf_entity_playback_model *model;rf_physics_body_state proposal;
    rf_collision_body_sphere scratch[8];rf_geometry_body_hit hit={0};
    float basis[9],world[3]={0},turn;uint32_t blocked=0,handle=o->registration.handle,owned=1;
    uint64_t epoch=scene_continuous_charge_runtime.epoch,lifetime=scene_medic_flight_lifetime;
    int active=0,due=0,status;int32_t now=scene_medic_now(frame);
    rf_scene_npc_stance_services services={NULL,campaign_scripted_speed_ground,s};
    if(o->guard_retreat_fault)return RF_FORMAT;
    if(!scene_guard_retreat_active(o)||!o->flight.entered)return RF_NOT_FOUND;
    /* Fixed physical adapters cannot reenter save; the ground-impact damage
     * owner CAN synchronously kill/cancel this actor. Do not hold the onset
     * latch over that service and thereby obstruct its real death owner.
     * Every mandatory error latches the reached state, with no rollback. */
#define GUARD_RECHECK() do { \
    if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return RF_NOT_FOUND; \
    if(o->guard_retreat_fault)return RF_FORMAT; \
    owned=scene_guard_retreat_active(o); \
    if(owned&&(o->ai_mode.action_280!=7||o->view.action_520!=7)){o->guard_retreat_fault=1;return RF_FORMAT;} \
    if(!owned)memset(&nav,0,sizeof(nav)); \
} while(0)
#define GUARD_STEP(call) do { \
    status=(call); \
    if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return status?status:RF_NOT_FOUND; \
    if(status){o->guard_retreat_fault=1;return status;} \
    GUARD_RECHECK(); \
} while(0)
    GUARD_RECHECK();
    if(index>=campaign_model_owner_count||!campaign_model_owners||o->movement_slot>=16){o->guard_retreat_fault=1;return RF_RANGE;}
    memcpy(basis,campaign_model_owners[index].basis,36);
    if(o->damage.effects.health<=0||(o->view.flags_810&1u)||(o->object_flags&(2u|0x4000u)))
        return scene_guard_retreat_cancel(o,now,SCENE_GUARD_CANCEL_RESTORE_ORDINARY);
    status=scene_guard_retreat_player_onfoot(s);
    if(status==RF_NOT_FOUND){GUARD_STEP(scene_guard_retreat_return(o,now));goto physical;}
    if(status){o->guard_retreat_fault=1;return status;}
    if(scene_guard_retreat_overlap(o,0)){
        /* All successful command replacements cancel BEFORE their bank is
         * installed. An unexplained overlapping owner is not ours to erase. */
        o->guard_retreat_fault=1;return RF_FORMAT;
    }
    if((campaign_modes[o->movement_slot].index!=1&&campaign_modes[o->movement_slot].index!=3)||
       campaign_npc_ai_suppressed(o)){
        GUARD_STEP(scene_guard_retreat_return(o,now));goto physical;
    }
    status=scene_guard_retreat_binding(index,o->view.weapons[0],o->view.weapons[1],&o->inventory,&o->selection,0);
    if(status==RF_NOT_FOUND){GUARD_STEP(scene_guard_retreat_return(o,now));goto physical;}
    if(status){o->guard_retreat_fault=1;return status;}
    status=scene_medic_flight_threat(o,frame,&threat);
    if(status==RF_NOT_FOUND||campaign_player_damage.state.effects.health<=0){
        GUARD_STEP(scene_guard_retreat_return(o,now));goto physical;
    }
    if(status){o->guard_retreat_fault=1;return status;}
    GUARD_STEP(rf_timer_expired(o->flight.deadline_250,now,&due));
    if(due){GUARD_STEP(scene_guard_retreat_return(o,now));goto physical;}
    memset(o->command_714,0,12);memset(o->look.command_708,0,12);
    GUARD_STEP(campaign_actor_pose(index,&pose));
    if(!pose||pose->skeleton>=campaign_playback_resources.model_count){o->guard_retreat_fault=1;return RF_NOT_FOUND;}
    model=campaign_playback_resources.models+pose->skeleton;
    if(o->pain.selected_action!=-1){
        GUARD_STEP(rf_motion_action_active(&pose->playback,model->resources,model->count,
            o->selection.mapping.actions,o->pain.selected_action,&active));
        if(active)goto physical;
        o->pain.selected_action=-1;
    }
    if((campaign_seeds.classes[campaign_seeds.items[index].class_index].physics.flags&1u)&&
       campaign_modes[o->movement_slot].index==1){
        GUARD_STEP(rf_scene_npc_normal(s->collision,handle,&services));
        if(!owned)goto physical;
    }
    GUARD_STEP(scene_medic_flight_nav_step(s,o,&threat,now,elapsed,&nav));
    if(!owned)goto physical;
    if(nav.no_route||nav.source_result==-1||!o->flight.nav.count){
        GUARD_STEP(scene_guard_retreat_return(o,now));goto physical;
    }
    if(nav.source_result){o->guard_retreat_fault=1;return RF_FORMAT;}
    if(nav.command_valid)scene_medic_flight_command_publish(o,nav.command,basis);
physical:
    GUARD_RECHECK();
    /* No cancelled flee command reaches translation/steering. Do not clear a
     * newly committed incoming owner's command/look banks to achieve this. */
    GUARD_STEP(campaign_script_ground(s,o,index,elapsed,frame,landing));
    if(o->damage.effects.health<=0||(o->view.flags_810&1u))return RF_OK;
    if(owned&&nav.turn_valid)GUARD_STEP(rf_scene_npc_steer(handle,nav.turn_point,elapsed,rf_scene_npc_playback[0],&turn));
    if(owned)GUARD_STEP(rf_scene_npc_prepare_angular(handle,elapsed));
    proposal=o->body.state;
    if(o->movement_slot>=16){o->guard_retreat_fault=1;return RF_RANGE;}
    if(campaign_modes[o->movement_slot].index==1){
        if(!isfinite(o->movement.speed)||o->movement.speed<0){o->guard_retreat_fault=1;return RF_FORMAT;}
        if(owned)for(uint32_t i=0;i<3;i++)world[i]=(float)(((double)o->command_714[2]*basis[6+i]+
            (double)o->command_714[1]*basis[3+i])+(double)o->command_714[0]*basis[i]);
        proposal.next_position[0]=proposal.position[0]+world[0]*o->movement.speed*elapsed;
        proposal.next_position[1]=proposal.position[1];
        proposal.next_position[2]=proposal.position[2]+world[2]*o->movement.speed*elapsed;
        GUARD_STEP(rf_physics_body_prepare_sweep(&proposal));
        GUARD_STEP(scene_npc_live_body_sweep(s->collision,o,&proposal,0x460,scratch,8,&hit,&blocked));
        if(blocked&&hit.contact.normal[1]>=.5f){
            float dx=proposal.next_position[0]-proposal.position[0],dz=proposal.next_position[2]-proposal.position[2];
            float rise=-(dx*hit.contact.normal[0]+dz*hit.contact.normal[2])/hit.contact.normal[1];
            if(rise>0){
                proposal.next_position[1]=proposal.position[1]+rise+.001f;
                GUARD_STEP(rf_physics_body_prepare_sweep(&proposal));
                GUARD_STEP(scene_npc_live_body_sweep(s->collision,o,&proposal,0x460,scratch,8,&hit,&blocked));
            }
        }
        if(blocked&&fabsf(hit.contact.normal[1])<.5f){
            float nx=hit.contact.normal[0],nz=hit.contact.normal[2],length=nx*nx+nz*nz;
            float dx=proposal.next_position[0]-proposal.position[0],dz=proposal.next_position[2]-proposal.position[2];
            float inward=dx*nx+dz*nz;
            if(length>.0001f&&inward<0){
                dx-=nx*inward/length;dz-=nz*inward/length;
                if(dx*dx+dz*dz>.00000001f){
                    proposal.next_position[0]=proposal.position[0]+dx;proposal.next_position[1]=proposal.position[1];
                    proposal.next_position[2]=proposal.position[2]+dz;
                    GUARD_STEP(rf_physics_body_prepare_sweep(&proposal));
                    GUARD_STEP(scene_npc_live_body_sweep(s->collision,o,&proposal,0x460,scratch,8,&hit,&blocked));
                }
            }
        }
    }
    /* Keep the existing kinematic stop/slide response, now including the
     * actual grounded player in each live translation query. */
    if(blocked||!owned)memcpy(proposal.next_position,proposal.position,12);
    memcpy(o->body.state.next_position,proposal.next_position,12);o->body.state.flags|=0x40000000u;
    GUARD_STEP(rf_scene_npc_commit_ordinary(handle,elapsed));
    GUARD_STEP(rf_scene_npc_publish_position(handle));
    if(owned)GUARD_STEP(scene_medic_flight_locomotion(s,o,index));
    return RF_OK;
#undef GUARD_STEP
#undef GUARD_RECHECK
}
#endif
