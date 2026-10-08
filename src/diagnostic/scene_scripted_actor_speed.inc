#ifndef RF_SCENE_SCRIPTED_ACTOR_SPEED_INCLUDED
#define RF_SCENE_SCRIPTED_ACTOR_SPEED_INCLUDED
/* Historical Make_Fly26 ->4b9540 ->428030(actor,0). This is the recovered
 * slow/standing transition, not a flying-body factory or vehicle controller.
 * Include after ordinary NPC ground/support helpers, before the script loop. */
uint32_t rf_scene_scripted_actor_speed[8]; /* calls,applied,missing,actorUID,handle,descriptor,speed bits,status */
static int campaign_scripted_speed_ground(void *context,uint32_t handle)
{
    scene_stream *s=context;campaign_npc_body *o;rf_entity_pose *pose;uint32_t cls,found=0,moving;int status;
    rf_physics_ground_probe probe;rf_collision_actor_contact contact={0};scene_piece_support piece={0};
    if(!s||!s->collision)return RF_RANGE;
    status=campaign_npc_motion_owner(handle,&o,&cls,&pose);if(status)return status;
    status=scene_npc_ground_query_piece(s->collision,handle,scene_step_seconds,&probe,&contact,&found,&piece);if(status)return status;
    moving=found&&contact.handle&&contact.handle!=UINT32_MAX&&campaign_object_velocity(contact.handle)!=NULL;
    if(found&&contact.time<1&&contact.normal[1]>=.5f){
        status=rf_physics_support_accept(&o->body.state,&probe,contact.time,moving,contact.velocity[1],
            moving?contact.handle:0,(int32_t)contact.material,&o->support,o->published);if(status)return status;
        status=rf_collision_contact_write(&o->body.state,&o->collision_contact,&contact);if(status)return status;
        status=campaign_npc_support_contact(o,contact.velocity,moving);if(status)return status;o->piece_support=piece;
    }else{
        memset(&o->piece_support,0,sizeof(o->piece_support));
        status=rf_scene_npc_fall(handle);if(status)return status;
    }
    return rf_scene_npc_publish_position(handle);
}
static int campaign_set_actor_slow(void *context,uint32_t handle)
{
    scene_stream *s=context;campaign_npc_body *o;rf_entity_pose *pose;uint32_t cls;int status;
    rf_scene_npc_stance_services services={NULL,campaign_scripted_speed_ground,s};
    ++rf_scene_scripted_actor_speed[0];rf_scene_scripted_actor_speed[4]=handle;
    if(!s||!s->collision){status=RF_RANGE;goto fail;}
    status=campaign_npc_motion_owner(handle,&o,&cls,&pose);if(status)goto fail;
    /* The installed merc_com target is an ordinary living, unlinked NPC.
     * Unknown attachment/vehicle/player transitions retain their own owner. */
    if(!o->registration.view||o->damage.effects.health<=0||(o->view.flags_810&1u)||
       (o->object_flags&2u)||o->view.linked_handle!=-1||scene_npc_seat_attached(handle)){
        status=RF_NOT_FOUND;goto fail;
    }
    rf_scene_scripted_actor_speed[3]=(uint32_t)campaign_seeds.records.items[o-campaign_npc_bodies].record.uid;
    status=rf_scene_npc_slow(s->collision,handle,0,&services);if(status)goto fail;
    ++rf_scene_scripted_actor_speed[1];rf_scene_scripted_actor_speed[5]=o->movement_slot;
    memcpy(rf_scene_scripted_actor_speed+6,&o->movement.speed,4);rf_scene_scripted_actor_speed[7]=0;return RF_OK;
fail:
    if(status==RF_NOT_FOUND)++rf_scene_scripted_actor_speed[2];rf_scene_scripted_actor_speed[7]=(uint32_t)status;return status;
}
#endif
