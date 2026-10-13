#ifndef RF_SCENE_CROUCH_RUNTIME_ATTENTION_INCLUDED
#define RF_SCENE_CROUCH_RUNTIME_ATTENTION_INCLUDED
/* Bounded adaptation of406146: independently service the attention clock,
 * with fresh body/eye world+mover rays for the scheduled actor. There is no
 * cached original200..399ms visibility bank and no substitution of Blind age.
 * Actor/clutter/corpse occluder-list parity remains outside this bounded lane. */
static int scene_crouch_runtime_attention(scene_stream *stream,uint32_t slot,uint32_t frame)
{
    campaign_npc_body *o;scene_crouch_fire_guard guard;scene_tactical_receipt tactical;
    scene_crouch_finish_policy before,after;uint32_t restart=0,body_seen=0,eye_seen=0,blocked,player=0;
    float eye[3],body[3],forward[3],direction[3],dot=0;double length2=0,reciprocal;
    int32_t now=scene_patrol_now(frame);int status,checked,expired;
    if(!stream||!stream->collision||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;
    if(!(o->crouch.flags&SCENE_CROUCH_PRESENT)||!(o->tactical.flags&SCENE_TACTICAL_PRESENT)||
       !scene_crouch_motion_available(o)||!scene_tactical_service_allowed(o)||o->view.action_520!=3)return RF_OK;
    status=rf_timer_expired(o->crouch.attention_due,now,&expired);if(status||!expired)return status;
    status=scene_crouch_owner_guard_capture(slot,&guard);if(status)return status;
    tactical=o->tactical;
    before.stance=(uint32_t)o->unholster.stance_7bc;before.switch_due=o->crouch.switch_due;
    before.attention_due=o->crouch.attention_due;before.tactical_due=o->tactical.phase_due;before.random=o->crouch.random;
    player=scene_blind_target_handle(o)==campaign_player_object.handle&&
        scene_blind_target_live(campaign_player_object.handle);
    if(player&&o->ai_mode.state_2b4!=11){
        status=scene_medic_player_eye(frame,eye);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
        memcpy(body,scene_actor_body.state.position,sizeof(body));memcpy(forward,actor_look.eye_orientation+6,sizeof(forward));
        for(uint32_t i=0;i<3;i++){
            direction[i]=(float)((double)o->published[i]-eye[i]);
            if(!isfinite(direction[i])||!isfinite(forward[i]))return RF_FORMAT;
            length2+=(double)direction[i]*direction[i];
        }
        if(!isfinite(length2))return RF_FORMAT;
        if(length2>0){
            reciprocal=1.0/sqrt(length2);
            for(uint32_t i=0;i<3;i++)direction[i]=(float)(direction[i]*reciprocal);
            dot=(float)((double)forward[2]*direction[2]+(double)forward[1]*direction[1]+(double)forward[0]*direction[0]);
        }
        /* No ray is needed when the facing predicate already rules out a
         * restart. The attention deadline/RNG still follow the same branch. */
        if(dot>.95f)for(uint32_t i=0;i<2;i++){
            status=rf_geometry_collision_ray(stream->collision,&campaign_movers,o->eye_position,i?eye:body,0x27,NULL,&blocked);
            checked=scene_crouch_fire_guard_valid(&guard);if(checked)return checked;
            if(status)return status;
            if(memcmp(&o->tactical,&tactical,sizeof(tactical))||
               scene_blind_target_handle(o)!=campaign_player_object.handle||
               !scene_blind_target_live(campaign_player_object.handle)||!scene_crouch_motion_available(o)||
               !scene_tactical_service_allowed(o))return RF_OK;
            if(!blocked){if(i)eye_seen=1;else body_seen=1;break;}
        }
    }
    status=scene_crouch_attention_prepare(&before,now,rf_scene_campaign_countdown.difficulty,
        o->ai_mode.state_2b4,player,body_seen,eye_seen,dot,&after,&restart);if(status)return status;
    o->crouch.attention_due=after.attention_due;o->crouch.random=after.random;
    if(restart)o->tactical.phase_due=after.tactical_due;
    return RF_OK;
}
#endif
