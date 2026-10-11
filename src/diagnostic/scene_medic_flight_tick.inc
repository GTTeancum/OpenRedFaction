#ifndef RF_SCENE_MEDIC_FLIGHT_TICK_INCLUDED
#define RF_SCENE_MEDIC_FLIGHT_TICK_INCLUDED
/* Include after actual ground, sweep, stance and landing owners. The source
 * route/controller emits a command; the existing kinematic NPC scheduler is
 * the explicitly retained physical policy. No fabricated velocity or route. */
static int scene_medic_flight_locomotion(scene_stream *s,campaign_npc_body *o,uint32_t index)
{
    rf_entity_pose *pose;rf_motion_priority priority={0};rf_motion_stance_decision stance;
    rf_motion_movement move={0};int status,handled=0;
    uint32_t guard=scene_guard_retreat_active(o),handle=o->registration.handle;
    uint64_t epoch=scene_continuous_charge_runtime.epoch,lifetime=scene_medic_flight_lifetime;
    rf_scene_npc_stance_services services={NULL,campaign_scripted_speed_ground,s};
    status=campaign_actor_pose(index,&pose);if(status)return status;
    if(!pose)return RF_NOT_FOUND;
    if(o->movement_slot>=16)return RF_FORMAT;
    priority.forced_state=o->ai_override.word_834;priority.flags=o->view.flags_810;priority.physics_flags=o->body.state.flags;
    priority.mode=(int32_t)campaign_modes[o->movement_slot].index;
    priority.class_type=o->view.class_type;priority.action=o->ai_mode.action_280;
    priority.entity_handle=(int32_t)o->registration.handle;priority.linked_occupant_handle=-1;
    memcpy(priority.velocity,o->body.state.velocity,12);
    /* Linked owners are excluded before this slice can own executable state.
     * Existing scripted motion retains its playback override independently. */
    status=rf_motion_select_priority(&pose->controller,o->selection.mapping.states,&priority,&handled);
    if(status||handled)return status;
    status=rf_motion_select_stance(&pose->controller,o->selection.mapping.states,8,0,o->view.flags_810,&stance);
    if(status)return status;
    if(stance.effect==RF_MOTION_STANCE_STAND){
        status=rf_scene_npc_normal(s->collision,o->registration.handle,&services);if(status)return status;
        if(guard){
            if(!scene_guard_retreat_same_owner(o,index,handle,epoch,lifetime))return RF_NOT_FOUND;
            if(!scene_guard_retreat_active(o)||o->damage.effects.health<=0||(o->view.flags_810&1u))return RF_OK;
            if(o->ai_mode.action_280!=7||o->view.action_520!=7)return RF_FORMAT;
            status=campaign_actor_pose(index,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
        }
    }
    memcpy(move.vector,o->command_714,12);move.mode=(int32_t)campaign_modes[o->movement_slot].index;
    move.direction=o->movement.mode;
    move.idle_state=o->ai_mode.action_280==7?13:0;
    move.move_state=o->ai_mode.action_280==7?6:2;
    move.alternate_state=o->ai_mode.action_280==7&&o->selection.mapping.states[6]>=0?6:4;
    return rf_motion_select_movement(&pose->controller,o->selection.mapping.states,&move);
}
static void scene_medic_flight_command_publish(campaign_npc_body *o,const float world[3],const float basis[9])
{
    double norm=(double)world[0]*world[0]+(double)world[1]*world[1]+(double)world[2]*world[2];
    if(!(norm>1e-5))return;
    for(uint32_t i=0;i<3;i++){
        uint32_t k=i*3;
        float value=(float)(((double)world[2]*basis[k+2]+(double)world[1]*basis[k+1])+
            (double)world[0]*basis[k]);
        o->command_714[i]=fmaxf(-1.f,fminf(1.f,value));
    }
}
static int scene_medic_flight_tick(scene_stream *s,campaign_npc_body *o,uint32_t index,
    float elapsed,uint32_t frame,scene_npc_landing_receipt *landing)
{
    scene_medic_flight_threat_view threat;scene_medic_flight_nav_result nav={0};
    rf_entity_pose *pose;rf_entity_playback_model *model;rf_physics_body_state proposal;
    rf_collision_body_sphere scratch[8];rf_geometry_body_hit hit={0};
    float basis[9],world[3]={0},turn;uint32_t found=0,blocked=0,supported=0;int active=0,due=0,status;
    int32_t now=scene_medic_now(frame);
    rf_scene_npc_stance_services services={NULL,campaign_scripted_speed_ground,s};
    if(scene_medic_flight_restoring||!scene_medic_flight_live(o)||!o->flight.entered||
       o->ai_mode.action_280!=7)return RF_NOT_FOUND;
    if(o->damage.effects.health<=0||(o->view.flags_810&1u)||(o->object_flags&(2u|0x4000u)))return RF_OK;
    if(scene_medic_flight_overlap(o))return RF_NOT_FOUND;
    if(index>=campaign_model_owner_count)return RF_RANGE;
    memcpy(basis,campaign_model_owners[index].basis,36);
    status=scene_medic_flight_threat(o,frame,&threat);
    if(status!=RF_OK&&status!=RF_NOT_FOUND)return status;
    if(!status){status=scene_medic_flight_visibility_admit(s,o,&threat,&supported);if(status)return status;}
    else supported=1; /* Missing target defaults only after the selected-pain gate. */
    if(!supported){
        /* Approved port capability-loss policy: default without inventing a
         * LOS result, route or RNG draw. Retained flight metadata survives. */
        memset(o->command_714,0,12);memset(o->look.command_708,0,12);
        status=scene_medic_flight_default(o,now);if(status)return status;
        goto physical;
    }
    /*403660 before dispatch, distinct from AI500; saved714 survives until here. */
    memset(o->command_714,0,12);memset(o->look.command_708,0,12);++rf_scene_medic_flight[5];
    status=campaign_actor_pose(index,&pose);if(status)return status;
    if(!pose||pose->skeleton>=campaign_playback_resources.model_count)return RF_NOT_FOUND;
    model=campaign_playback_resources.models+pose->skeleton;
    if(o->pain.selected_action!=-1){
        status=rf_motion_action_active(&pose->playback,model->resources,model->count,
            o->selection.mapping.actions,o->pain.selected_action,&active);if(status)return status;
        if(active){++rf_scene_medic_flight[6];goto physical;}
        o->pain.selected_action=-1;
    }
    status=scene_medic_flight_threat(o,frame,&threat);
    if(status==RF_NOT_FOUND){status=scene_medic_flight_default(o,now);if(status)return status;goto physical;}
    if(status)return status;
    if(o->movement_slot>=16)return RF_FORMAT;
    if((campaign_seeds.classes[campaign_seeds.items[index].class_index].physics.flags&1u)&&
       campaign_modes[o->movement_slot].index==1){
        status=rf_scene_npc_normal(s->collision,o->registration.handle,&services);if(status)return status;
    }
    status=scene_medic_flight_nav_step(s,o,&threat,now,elapsed,&nav);if(status)return status;
    if(nav.no_route){
        status=rf_timer_set(&o->flight.stall_deadline_4d0,now,1000);if(status)return status;
        status=scene_medic_flight_no_route(o,now);if(status)return status;
    }
    if(nav.source_result==-1){
        status=rf_timer_expired(o->flight.sight_deadline_294,now,&due);if(status)return status;
        if(due){
            uint32_t visibility[2];
            status=rf_timer_set(&o->flight.sight_deadline_294,now,200+(int32_t)(scene_medic_flight_random()%200u));if(status)return status;
            status=scene_medic_flight_observe(s,o,&threat,visibility);if(status)return status;
            o->flight.visible_published_29c=visibility[0];o->flight.visible_eye_29d=visibility[1];
            ++rf_scene_medic_flight[8];
        }
        if(o->flight.visible_published_29c||o->flight.visible_eye_29d){
            status=scene_medic_flight_nav_request(s,o,&threat,now,&found);if(status)return status;
        }
        if(!found){
            status=rf_timer_expired(o->flight.no_route_deadline_4d4,now,&due);if(status)return status;
            if(due){
                /*405075 caller truncates extended504e40 directly, unlike
                 * the binary32-stored60..120-second onset duration. */
                int32_t delay=(int32_t)(10000.0+(double)scene_medic_flight_random()*(5000.0/32768.0));
                status=rf_timer_set(&o->flight.no_route_deadline_4d4,now,delay);if(status)return status;
            }
        }
    }else if(nav.source_result!=-2&&nav.source_result!=0)return RF_FORMAT;
    if(nav.command_valid)scene_medic_flight_command_publish(o,nav.command,basis);
    status=rf_timer_expired(o->flight.deadline_250,now,&due);if(status)return status;
    if(due){status=scene_medic_flight_default(o,now);if(status)return status;}
physical:
    /* Ground/fall remains a real independent body owner, including pain pause.
     * Saving a genuinely falling unsupported ordinary body stays guarded. */
    status=campaign_script_ground(s,o,index,elapsed,frame,landing);if(status)return status;
    if(o->damage.effects.health<=0||(o->view.flags_810&1u))return RF_OK;
    if(nav.turn_valid){
        status=rf_scene_npc_steer(o->registration.handle,nav.turn_point,elapsed,rf_scene_npc_playback[0],&turn);if(status)return status;
    }
    status=rf_scene_npc_prepare_angular(o->registration.handle,elapsed);if(status)return status;
    proposal=o->body.state;
    if(campaign_modes[o->movement_slot].index==1){
        if(!isfinite(o->movement.speed)||o->movement.speed<0)return RF_FORMAT;
        /* Consume the same clamped local command as the motion/glare owner.
         * Existing NPC translation is kinematic and preserves true velocity. */
        for(uint32_t i=0;i<3;i++)world[i]=(float)(((double)o->command_714[2]*basis[6+i]+
            (double)o->command_714[1]*basis[3+i])+(double)o->command_714[0]*basis[i]);
        proposal.next_position[0]=proposal.position[0]+world[0]*o->movement.speed*elapsed;
        proposal.next_position[1]=proposal.position[1];
        proposal.next_position[2]=proposal.position[2]+world[2]*o->movement.speed*elapsed;
        status=rf_physics_body_prepare_sweep(&proposal);if(status)return status;
        status=rf_scene_npc_body_sweep(s->collision,o->registration.handle,&proposal,0x460,scratch,8,&hit,&blocked);if(status)return status;
        if(blocked&&hit.contact.normal[1]>=.5f){
            float dx=proposal.next_position[0]-proposal.position[0],dz=proposal.next_position[2]-proposal.position[2];
            float rise=-(dx*hit.contact.normal[0]+dz*hit.contact.normal[2])/hit.contact.normal[1];
            if(rise>0){proposal.next_position[1]=proposal.position[1]+rise+.001f;
                status=rf_physics_body_prepare_sweep(&proposal);if(status)return status;
                status=rf_scene_npc_body_sweep(s->collision,o->registration.handle,&proposal,0x460,scratch,8,&hit,&blocked);if(status)return status;}
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
                    status=rf_physics_body_prepare_sweep(&proposal);if(status)return status;
                    status=rf_scene_npc_body_sweep(s->collision,o->registration.handle,&proposal,0x460,scratch,8,&hit,&blocked);if(status)return status;
                }
            }
        }
    }
    if(blocked)memcpy(proposal.next_position,proposal.position,12);
    memcpy(o->body.state.next_position,proposal.next_position,12);o->body.state.flags|=0x40000000u;
    status=rf_scene_npc_commit_ordinary(o->registration.handle,elapsed);if(status)return status;
    status=rf_scene_npc_publish_position(o->registration.handle);if(status)return status;
    return scene_medic_flight_locomotion(s,o,index);
}
#endif
