#ifndef RF_SCENE_BURNING_VISUAL_INC
#define RF_SCENE_BURNING_VISUAL_INC
/* Include after scene_burning.inc and scene_flame_visual.inc. Call once after
 * scene_burning_tick; its wet/death/expiry retirement is authoritative. Existing
 * particle simulation/drawing owns age, alpha and texture animation. Three body
 * sphere anchors per burning actor, at most12 new sprites per frame, are explicit
 * port presentation policy. No new textures, light or full-screen overlay.
 * Private negative owner tags bypass entity-owner lookup and identify our
 * records after shared-pool reuse; never touch list links or alive flags. */
#define SCENE_BURNING_VISUAL_ANCHORS 3u
#define SCENE_BURNING_VISUAL_TAG 0xffb00000u
typedef struct scene_burning_visual_link {uint32_t particle,target;} scene_burning_visual_link;
static scene_burning_visual_link scene_burning_visual_links[SCENE_BURNING_CAPACITY*SCENE_BURNING_VISUAL_ANCHORS];
/* spawned, followed, retired, pool misses, current attached */
uint32_t rf_scene_burning_visual[5];
static int scene_burning_visual_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t pass,j,k,n,texture=UINT32_MAX,created=0;int status;
    if(!stream)return RF_RANGE;
    if(!frame)memset(rf_scene_burning_visual,0,sizeof(rf_scene_burning_visual));
    rf_scene_burning_visual[4]=0;
    if(!stream->particles.state || !scene_flame_visual_ready || !stream->impact)return RF_OK;
    /* Rotating start gives every live burn a chance at the
     * global spawn budget without allocating or scanning particle records. */
    for(pass=0;pass<SCENE_BURNING_CAPACITY;pass++){
        uint32_t slot=(frame+pass)%SCENE_BURNING_CAPACITY;
        scene_burning_record *burn=scene_burning_records+slot;
        campaign_npc_body *owner=burn->left?scene_burning_owner(burn->target,NULL):NULL;
        uint32_t room=UINT32_MAX;
        if(owner && (owner->damage.effects.burn!=slot+1 || owner->damage.effects.health<=0 ||
           !owner->body.allocated_bytes || !owner->body.spheres.count || !owner->body.spheres.items ||
           (owner->object_flags&(2u|0x4000u)) || (owner->view.flags_810&1u)))owner=NULL;
        if(owner){rf_collision_room_location location;
            status=rf_geometry_collision_world_locate(stream->collision,owner->body.state.position,&location);if(status)return status;
            room=location.room;if(room==UINT32_MAX)owner=NULL;
        }
        for(j=0;j<SCENE_BURNING_VISUAL_ANCHORS;j++){
            uint32_t link_index=slot*SCENE_BURNING_VISUAL_ANCHORS+j,tag=SCENE_BURNING_VISUAL_TAG+link_index;
            scene_burning_visual_link *link=scene_burning_visual_links+link_index;
            rf_particle *particle=link->particle<RF_PARTICLE_CAPACITY?stream->particles.state->records+link->particle:NULL;
            uint32_t owned=particle && (particle->flags&1u) && particle->pool==0 && particle->owner==tag;
            float center[3],position[3],toward[3],radius;double length=0;
            const rf_physics_sphere *sphere;
            if(owned && (!owner || link->target!=burn->target)){
                status=rf_particle_pool_recycle(&stream->particles.state->particles,link->particle);if(status)return status;
                ++rf_scene_burning_visual[2];owned=0;
            }
            if(!owner)continue;
            sphere=owner->body.spheres.items+(j*owner->body.spheres.count/SCENE_BURNING_VISUAL_ANCHORS);
            for(k=0;k<3;k++){
                center[k]=owner->body.state.position[k];
                for(n=0;n<3;n++)center[k]+=sphere->center[n]*owner->body.state.orientation[n*3+k];
                toward[k]=stream->particle_camera.view.origin[k]-center[k];length+=(double)toward[k]*toward[k];
            }
            if(!isfinite(sphere->radius) || sphere->radius<=0 || !isfinite(length))return RF_RANGE;
            length=sqrt(length);
            for(k=0;k<3;k++)position[k]=center[k]+(length>1e-6?toward[k]*(float)(sphere->radius/length):0);
            /* Retain authored growth/lifetime; body-scaled birth radius avoids
             * hiding a tiny emitter inside the actor's visible surface. */
            radius=sphere->radius;if(radius<.25f)radius=.25f;if(radius>.55f)radius=.55f;
            if(owned){
                memcpy(particle->position,position,12);particle->room=room+1;++rf_scene_burning_visual[1];
            }else{
                rf_particle_spawn spawn={0};uint32_t index;
                if(created>=12)continue;
                if(!scene_flame_visual_ready || !stream->impact)return RF_NOT_FOUND;
                if(texture==UINT32_MAX){
                    for(k=0;k<9;k++)if((campaign_rocket_impact.resolved&(1u<<k)) &&
                        scene_flame_visual_same_bitmap(scene_flame_particle.bitmap,campaign_rocket_impact.emitters[k].bitmap)){
                        texture=stream->impact->materials.slot_texture[k];break;}
                    if(texture>=stream->impact->materials.count || !stream->impact->materials.animations[texture].count)return RF_NOT_FOUND;
                }
                memcpy(spawn.position,position,12);spawn.radius=radius;spawn.life=scene_flame_particle.max_life;
                spawn.growth=scene_flame_particle.growth;spawn.acceleration=scene_flame_particle.acceleration;
                spawn.gravity_scale=scene_flame_particle.gravity_scale;
                spawn.bitmap=stream->particles.materials.texture_count+texture;
                spawn.frame_count=stream->impact->materials.animations[texture].count;
                memcpy(&spawn.color,scene_flame_particle.color,4);memcpy(&spawn.color_destination,scene_flame_particle.color_destination,4);
                spawn.flags=scene_flame_particle.flags.particle;spawn.secondary=scene_flame_particle.flags.secondary;
                spawn.age_to_finish_vbm=scene_flame_particle.age_to_finish_vbm;
                status=rf_particle_pool_create(&stream->particles.state->particles,0,&spawn,tag,room+1,0,&stream->particles.state->random,&index);
                if(status==RF_NOT_FOUND){++rf_scene_burning_visual[3];continue;}if(status)return status;
                link->particle=index;link->target=burn->target;++created;++rf_scene_burning_visual[0];
            }
            ++rf_scene_burning_visual[4];
        }
    }
    return RF_OK;
}
#endif
