/* Opt-in process-local cover experiment. A real rocket-extracted fragment is held across
 * the eye-to-eye line, then retired. NPC targeting, spread, fire and damage run
 * normally. Holding/repositioning the fragment is test stimulus only. */
uint32_t rf_scene_dev_npc_cover[6][8];
static uint32_t npc_cover_tag,npc_cover_piece,npc_cover_ready;
static float npc_cover_position[3];
/* Xbox-only support fixture: real extracted piece, held still for a bounded
 * interval while the normal NPC gravity and ground paths run. */
uint32_t rf_scene_dev_npc_rubble_support[16];
extern uint32_t rf_scene_npc_idle_ground[6];
static uint32_t npc_support_tag,npc_support_piece,npc_support_ready;
static float npc_support_position[3];
static int scene_npc_rubble_support_stimulus(scene_stream *s,uint32_t frame)
{
    rf_geomod_piece_registry *registry;rf_geomod_piece_batch *batch;
    rf_geomod_owned_piece part;rf_physics_body *body;campaign_npc_body *npc;
    rf_physics_ground_probe probe;rf_collision_actor_contact contact={0};
    uint32_t source,b,p,k,found;int status;
    if(rf_scene_dev_npc_enabled!=10)return RF_OK;
    if(!rf_scene_dev_room_enabled || campaign_npc_body_count!=1)return RF_RANGE;
    if(!frame){npc_support_ready=0;memset(rf_scene_dev_npc_rubble_support,0,sizeof(rf_scene_dev_npc_rubble_support));}
    if(frame<300 || frame>420)return RF_OK;
    npc=campaign_npc_bodies;
    if(!npc_support_ready) {
        rf_scene_dev_npc_rubble_support[0]=scene_detached_sources_batch_count(s);
        for(source=0;source<(s->terrain_source_count?s->terrain_source_count:1);source++) {
            registry=scene_detached_source_registry(s,source);
            for(b=0;b<rf_geomod_piece_registry_count(registry);b++) {
                status=rf_geomod_piece_registry_get(registry,b,&batch);if(status)return status;
                for(p=0;p<rf_geomod_piece_batch_count(batch);p++) {
                    status=rf_geomod_piece_batch_get(batch,p,&part,&body);if(status)return status;
                    if(!rf_geomod_piece_batch_alive(batch,p) || body->state.bounds.radius<=.5f)continue;
                    npc_support_tag=scene_detached_batch_tag(source,b);npc_support_piece=p;
                    npc_support_ready=1;goto selected;
                }
            }
        }
        return RF_NOT_FOUND;
    }
selected:
    registry=scene_detached_source_registry(s,scene_detached_tag_source(npc_support_tag));
    status=rf_geomod_piece_registry_get(registry,scene_detached_tag_batch(npc_support_tag),&batch);if(status)return status;
    status=rf_geomod_piece_batch_get(batch,npc_support_piece,&part,&body);if(status)return status;
    if(!rf_geomod_piece_batch_alive(batch,npc_support_piece))return RF_NOT_FOUND;
    if(frame==300) {
        const rf_physics_sphere *ground=npc->body.spheres.items;uint32_t i,face=UINT32_MAX;
        float face_center[3]={0},highest=-1.e30f;
        if(!ground || !npc->body.spheres.count)return RF_RANGE;
        for(i=1;i<npc->body.spheres.count;i++)if(npc->body.spheres.items[i].center[1]<ground->center[1])ground=npc->body.spheres.items+i;
        for(i=0;i<part.mesh.face_count;i++) {
            const rf_collision_face *surface=part.collision+i;uint32_t vertex,axis;float center[3]={0};
            if(surface->plane[1]<.7f || !surface->count)continue;
            for(vertex=0;vertex<surface->count;vertex++)for(axis=0;axis<3;axis++)center[axis]+=surface->vertices[vertex][axis]/surface->count;
            if(center[1]>highest){highest=center[1];face=i;memcpy(face_center,center,sizeof(center));}
        }
        if(face==UINT32_MAX)return RF_NOT_FOUND;
        npc_support_position[0]=npc->body.state.position[0]+ground->center[0]-face_center[0];
        npc_support_position[1]=.4f-face_center[1];
        npc_support_position[2]=npc->body.state.position[2]+ground->center[2]-face_center[2];
        rf_scene_dev_npc_rubble_support[0]=1;rf_scene_dev_npc_rubble_support[1]=npc_support_tag;
        rf_scene_dev_npc_rubble_support[2]=npc_support_piece;
        memcpy(rf_scene_dev_npc_rubble_support+3,&body->state.bounds.radius,4);
    }
    for(k=0;k<3;k++) {
        float delta=npc_support_position[k]-body->state.position[k];
        body->state.position[k]=body->state.next_position[k]=npc_support_position[k];
        body->state.bounds.minimum[k]+=delta;body->state.bounds.maximum[k]+=delta;
    }
    memset(body->state.velocity,0,12);body->state.flags&=~0x80000000u;
    memset(body->state.orientation,0,sizeof(body->state.orientation));
    memset(body->state.next_orientation,0,sizeof(body->state.next_orientation));
    for(k=0;k<3;k++)body->state.orientation[k*3+k]=body->state.next_orientation[k*3+k]=1;
    if(frame==300) {
        const rf_physics_sphere *ground=npc->body.spheres.items;float rise;uint32_t i;
        for(i=1;i<npc->body.spheres.count;i++)if(ground[i].center[1]<ground->center[1])ground=npc->body.spheres.items+i;
        rise=.4f+ground->radius-ground->center[1]+.025f-npc->body.state.position[1];
        npc->body.state.position[1]+=rise;npc->body.state.next_position[1]+=rise;
        npc->body.state.bounds.minimum[1]+=rise;npc->body.state.bounds.maximum[1]+=rise;
        npc->published[1]+=rise;npc->eye_position[1]+=rise;
        memset(npc->body.state.velocity,0,12);npc->support.handle=0;
        status=rf_scene_npc_fall(npc->registration.handle);if(status)return status;
        status=rf_scene_npc_publish_position(npc->registration.handle);if(status)return status;
        status=rf_scene_npc_ground_query(s->collision,npc->registration.handle,1.f/60,&probe,&contact,&found);
        if(status)return status;
        rf_scene_dev_npc_rubble_support[4]=found;
        rf_scene_dev_npc_rubble_support[5]=contact.handle;
        memcpy(rf_scene_dev_npc_rubble_support+6,contact.normal+1,4);
        memcpy(rf_scene_dev_npc_rubble_support+7,&contact.time,4);
        memcpy(rf_scene_dev_npc_rubble_support+12,probe.start+1,4);
        memcpy(rf_scene_dev_npc_rubble_support+13,probe.end+1,4);
        memcpy(rf_scene_dev_npc_rubble_support+14,contact.point+1,4);
        {float top=.4f;memcpy(rf_scene_dev_npc_rubble_support+15,&top,4);}
    }
    rf_scene_dev_npc_rubble_support[8]=rf_scene_npc_idle_ground[3];
    rf_scene_dev_npc_rubble_support[9]=campaign_modes[npc->movement_slot].index;
    memcpy(rf_scene_dev_npc_rubble_support+10,npc->body.state.position+1,4);
    rf_scene_dev_npc_rubble_support[11]=rf_scene_npc_idle_ground[4];
    return RF_OK;
}
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
