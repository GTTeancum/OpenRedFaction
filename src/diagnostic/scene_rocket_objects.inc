/* Practical projectile/actor composition over retained physics spheres.
 * The original508e40 sphere query supplies time; model hit-location scaling
 * and within-tick relative actor motion remain separate reconstruction work. */
#define SCENE_ACTOR_ROCKET_OWNER 0x40000000u
#define SCENE_MOVER_ROCKET_OWNER 0x20000000u
uint32_t rf_scene_rocket_contacts[8]; /* queries,actors,movers,direct,amount bits,UID,status,reserved */
static uint32_t scene_rocket_body_hit(const rf_physics_body *body,const float start[3],
    const float delta[3],float radius,float limit,rf_collision_ray_hit *out)
{
    float ray[6],length=(float)sqrt((double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2]);
    rf_collision_ray_hit best={0};uint32_t i,k,found=0;
    if(length<=0 || !body->spheres.items)return 0;
    memcpy(ray,start,12);for(k=0;k<3;k++)ray[3+k]=delta[k]/length;
    for(i=0;i<body->spheres.count;i++) {
        const rf_physics_sphere *sphere=body->spheres.items+i;float center[3],point[3],fraction,normal[3],norm=0;
        for(k=0;k<3;k++)center[k]=(float)((double)body->state.position[k]+
            sphere->center[0]*(double)body->state.orientation[k]+sphere->center[1]*(double)body->state.orientation[3+k]+
            sphere->center[2]*(double)body->state.orientation[6+k]);
        if(!rf_collision_ray_sphere(ray,length,center,sphere->radius+radius,point,&fraction) || fraction>limit)continue;
        if(found && fraction>=best.fraction)continue;
        for(k=0;k<3;k++){normal[k]=point[k]-center[k];norm+=normal[k]*normal[k];}
        norm=(float)sqrt(norm);best.fraction=fraction;
        for(k=0;k<3;k++) {
            best.normal[k]=norm>0?normal[k]/norm:-ray[3+k];
            best.point[k]=center[k]+best.normal[k]*sphere->radius;
        }
        found=1;
    }
    if(found)*out=best;return found;
}
static int scene_rocket_objects_sweep(scene_stream *s,const float start[3],const float delta[3],
    float radius,uint32_t flags,rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{
    uint32_t i,found;int status;
    for(i=0;i<campaign_movers.count;i++) {
        const rf_collision_solid_view *solid=campaign_movers.views+i;
        rf_collision_sweep_room_hit hit;rf_collision_ray_hit world;
        status=rf_collision_solid_preferred(solid,NULL,flags&~0x1000u,start,delta,radius,
            *matched?out->hit.fraction:1,&hit,&found);if(status)return status;
        if(!found || (*matched && hit.tree.hit.fraction>=out->hit.fraction))continue;
        status=rf_collision_contact_world(&hit.tree.hit,solid->output_origin,solid->output_matrix,&world);if(status)return status;
        out->hit=world;out->object=SCENE_MOVER_ROCKET_OWNER|i;out->room=UINT32_MAX;out->face=hit.tree.face_index;
        *liquid=0;*matched=1;
    }
    for(i=0;i<campaign_npc_body_count;i++) {
        const campaign_npc_body *npc=campaign_npc_bodies+i;rf_collision_ray_hit hit;
        if(!npc->registration.view || !npc->body.allocated_bytes || npc->damage.effects.health<=0 ||
            (npc->object_flags&(2|0x4000)) || (npc->view.flags_810&1))continue;
        if(!scene_rocket_body_hit(&npc->body,start,delta,radius,*matched?out->hit.fraction:1,&hit))continue;
        if(*matched && hit.fraction>=out->hit.fraction)continue;
        out->hit=hit;out->object=SCENE_ACTOR_ROCKET_OWNER|i;
        out->room=npc->room.room?npc->room.room-1:UINT32_MAX;out->face=UINT32_MAX;
        *liquid=0;*matched=1;
    }
    (void)s;return RF_OK;
}
