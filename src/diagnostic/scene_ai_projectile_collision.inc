#ifndef RF_SCENE_AI_PROJECTILE_COLLISION_INC
#define RF_SCENE_AI_PROJECTILE_COLLISION_INC
/* Source-aware composition for NPC rocket and grenade flights. Include after
 * scene_rocket_body_hit and shared detached-source sweep declarations, before
 * both NPC projectile adapters. Query remains alive throughout each step. */
#define SCENE_AI_PROJECTILE_PLAYER 0x10000000u
typedef struct scene_ai_projectile_query {scene_stream *stream;uint32_t source;} scene_ai_projectile_query;
static int scene_ai_projectile_sweep(void *context,const float *start,const float *delta,float radius,uint32_t flags,
    rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{
    scene_ai_projectile_query *q=context;scene_stream *s=q->stream;rf_geometry_world_sweep_hit hit;
    rf_geomod_registry_hit piece;uint32_t i,found;int status;
    status=rf_geometry_collision_world_sweep_liquid(s->collision,flags,start,delta,radius,1,&hit,matched,liquid);if(status)return status;
    if(*matched){out->hit=hit.hit;out->room=hit.room;out->face=hit.face;out->object=UINT32_MAX;}
    for(i=0;i<campaign_movers.count;i++){
        const rf_collision_solid_view *solid=campaign_movers.views+i;rf_collision_sweep_room_hit local;rf_collision_ray_hit world;
        status=rf_collision_solid_preferred(solid,NULL,flags&~0x1004u,start,delta,radius,*matched?out->hit.fraction:1,&local,&found);if(status)return status;
        if(!found || (*matched && local.tree.hit.fraction>=out->hit.fraction))continue;
        status=rf_collision_contact_world(&local.tree.hit,solid->output_origin,solid->output_matrix,&world);if(status)return status;
        out->hit=world;out->room=UINT32_MAX;out->face=local.tree.face_index;out->object=SCENE_MOVER_ROCKET_OWNER|i;*matched=1;*liquid=0;
    }
    status=scene_detached_sources_sweep(s,flags,start,delta,radius,*matched?out->hit.fraction:1,&piece,&found);if(status)return status;
    if(found && (!*matched || piece.piece.hit.fraction<out->hit.fraction)){
        rf_collision_room_location room;status=rf_geometry_collision_world_locate(s->collision,piece.piece.hit.point,&room);if(status)return status;
        out->hit=piece.piece.hit;out->room=room.room;out->face=piece.piece.face;out->object=SCENE_DETACHED_ROCKET_OWNER|(piece.batch<<16)|piece.piece.piece;*matched=1;*liquid=0;
    }
    for(i=0;i<=campaign_npc_body_count;i++){
        const rf_physics_body *body;uint32_t tag,room;rf_collision_ray_hit body_hit;
        if(i==campaign_npc_body_count){
            if(!campaign_player_object.view || campaign_player_object.handle==q->source || campaign_player_damage.state.effects.health<=0)continue;
            body=&scene_actor_body;tag=SCENE_AI_PROJECTILE_PLAYER;room=rf_scene_player_swim[0];
        }else{
            const campaign_npc_body *npc=campaign_npc_bodies+i;
            if(!npc->registration.view || npc->registration.handle==q->source || !npc->body.allocated_bytes ||
               npc->damage.effects.health<=0 || (npc->object_flags&(2u|0x4000u)) || (npc->view.flags_810&1u))continue;
            body=&npc->body;tag=SCENE_ACTOR_ROCKET_OWNER|i;room=npc->room.room?npc->room.room-1:UINT32_MAX;
        }
        if(!scene_rocket_body_hit(body,start,delta,radius,*matched?out->hit.fraction:1,&body_hit) || (*matched && body_hit.fraction>=out->hit.fraction))continue;
        out->hit=body_hit;out->object=tag;out->room=room;out->face=UINT32_MAX;*matched=1;*liquid=0;
    }
    return scene_driller_projectile_compose(q->source,start,delta,radius,out,liquid,matched);
}
static int scene_ai_projectile_solid_sweep(void *context,const float *start,const float *delta,float radius,
    rf_weapon_flight_contact *contact,uint32_t *matched)
{
    uint32_t liquid=0;
    return scene_ai_projectile_sweep(context,start,delta,radius,4u|(radius<.05f?0x100u:0),contact,&liquid,matched);
}
#endif
