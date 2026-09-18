/* Opt-in process-local cover experiment. A real rocket-extracted fragment is held across
 * the eye-to-eye line, then retired. NPC targeting, spread, fire and damage run
 * normally. Holding/repositioning the fragment is test stimulus only. */
uint32_t rf_scene_dev_npc_cover[6][8];
static uint32_t npc_cover_tag,npc_cover_piece,npc_cover_ready;
static float npc_cover_position[3];
static int scene_npc_rubble_stimulus(scene_stream *s,uint32_t frame,const float eye[3])
{
    rf_geomod_piece_registry *registry;rf_geomod_piece_batch *batch;
    rf_geomod_owned_piece part;rf_physics_body *body;uint32_t source,b,p,k;int status;
    if(rf_scene_dev_npc_enabled!=2)return RF_OK;
    if(!rf_scene_dev_room_enabled || rf_scene_player_checkpoint_enabled || campaign_npc_body_count!=1)return RF_RANGE;
    if(!frame){npc_cover_ready=0;memset(rf_scene_dev_npc_cover,0,sizeof(rf_scene_dev_npc_cover));}
    if(frame<600)return RF_OK;
    if(frame==600) {
        campaign_npc_body *npc=campaign_npc_bodies;
        /* Stage the authored actor after real extraction; keep it outside the
         * rocket blast during preparation. No save-scope exception is made. */
        npc->body.state.position[2]-=5.5f;npc->body.state.next_position[2]-=5.5f;
        npc->body.state.bounds.minimum[2]-=5.5f;npc->body.state.bounds.maximum[2]-=5.5f;
        npc->published[2]-=5.5f;npc->eye_position[2]-=5.5f;
    }
    if(!npc_cover_ready) {
        for(source=0;source<(s->terrain_source_count?s->terrain_source_count:1);source++) {
            registry=scene_detached_source_registry(s,source);
            for(b=0;b<rf_geomod_piece_registry_count(registry);b++) {
                status=rf_geomod_piece_registry_get(registry,b,&batch);if(status)return status;
                for(p=0;p<rf_geomod_piece_batch_count(batch);p++) {
                    status=rf_geomod_piece_batch_get(batch,p,&part,&body);if(status)return status;
                    if(!rf_geomod_piece_batch_alive(batch,p) || body->state.bounds.radius<=1)continue;
                    npc_cover_tag=scene_detached_batch_tag(source,b);npc_cover_piece=p;npc_cover_ready=1;
                    for(k=0;k<3;k++)npc_cover_position[k]=(eye[k]+campaign_npc_bodies[0].eye_position[k])*.5f;
                    goto selected;
                }
            }
        }
        return RF_NOT_FOUND;
    }
selected:
    if(frame>810)return RF_OK;
    registry=scene_detached_source_registry(s,scene_detached_tag_source(npc_cover_tag));
    status=rf_geomod_piece_registry_get(registry,scene_detached_tag_batch(npc_cover_tag),&batch);if(status)return status;
    status=rf_geomod_piece_batch_get(batch,npc_cover_piece,&part,&body);if(status)return status;
    if(frame==810)return scene_detached_sources_damage(s,npc_cover_tag,npc_cover_piece,400);
    for(k=0;k<3;k++) {
        float delta=npc_cover_position[k]-body->state.position[k];
        body->state.position[k]=body->state.next_position[k]=npc_cover_position[k];
        body->state.bounds.minimum[k]+=delta;body->state.bounds.maximum[k]+=delta;
    }
    memset(body->state.velocity,0,12);body->state.flags&=~0x80000000u;
    return RF_OK;
}
static int scene_npc_rubble_record(scene_stream *s,uint32_t frame)
{
    rf_geomod_piece_batch *batch;uint32_t *row,k;int status;
    if(rf_scene_dev_npc_enabled!=2 || frame<=600 || frame>960 || frame%60)return RF_OK;
    status=rf_geomod_piece_registry_get(scene_detached_source_registry(s,scene_detached_tag_source(npc_cover_tag)),
        scene_detached_tag_batch(npc_cover_tag),&batch);if(status)return status;
    row=rf_scene_dev_npc_cover[(frame-600)/60-1];row[0]=frame;
    row[1]=rf_scene_enemy_combat[2];row[2]=rf_scene_enemy_combat[3];row[3]=rf_scene_enemy_spread[4];
    memcpy(row+4,&campaign_player_damage.state.effects.health,4);
    row[5]=rf_geomod_piece_batch_alive(batch,npc_cover_piece);row[6]=npc_cover_tag;row[7]=npc_cover_piece;
    printf("DEV_NPC_COVER");for(k=0;k<8;k++)printf(" %u",row[k]);puts("");return RF_OK;
}
