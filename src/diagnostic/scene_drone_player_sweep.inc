/* Translation-only port adapter for the authored Drone fly11 body. Uses actual
 * oriented sphere centers, not the grounded lateral max-radius approximation.
 * Each caller freezes the other body's committed pose; world hits win ties.
 * Angular sweep and dead-body response remain separate, unimplemented paths. */
uint32_t rf_scene_drone_player_contact[10]; /* NPC queries,hits,player queries,hits; last admission side,slot,spheres,material,mass bits,radius bits */
uint32_t rf_scene_drone_separation[20]; /* samples,side,gap bits,source xyz,target xyz,radii,delta xyz,limit,nearest fraction,i,j,ray hits,reserved */
static uint32_t scene_contact_drone(uint32_t slot)
{
    uint32_t cls;
    if(!campaign_npc_bodies || !campaign_seeds.items || !campaign_seeds.classes ||
       !campaign_seeds.records.items || slot>=campaign_npc_body_count ||
       slot>=campaign_seeds.records.count)return 0;
    cls=campaign_seeds.items[slot].class_index;
    return cls<campaign_seeds.class_count && campaign_npc_bodies[slot].movement_slot<16 &&
        campaign_modes[campaign_npc_bodies[slot].movement_slot].index==11 &&
        campaign_seeds.classes[cls].physics.movement_index==11 &&
        !strcmp(campaign_seeds.records.items[slot].record.class_name,"Drone");
}
static int scene_drone_sphere_sweep(const rf_physics_body_state *moving,
    const rf_physics_spheres *source,const rf_physics_body_state *fixed,
    const rf_physics_spheres *target,float limit,rf_geometry_body_hit *hit,uint32_t *found)
{
    uint32_t i,j,k,n;float delta[3],length;double magnitude=0;
    if(!moving||!fixed||!source||!target||!hit||!found ||
       !source->items||!target->items||!source->count||source->count>8||
       !target->count||target->count>8||!isfinite(limit)||limit<0||limit>1)return RF_RANGE;
    *found=0;
    for(k=0;k<3;k++) {
        delta[k]=moving->next_position[k]-moving->position[k];
        if(!isfinite(delta[k])||!isfinite(moving->position[k])||!isfinite(fixed->position[k]))return RF_FORMAT;
        magnitude+=(double)delta[k]*delta[k];
    }
    length=(float)sqrt(magnitude);if(!isfinite(length))return RF_FORMAT;
    if(!length||!limit)return RF_OK;
    for(i=0;i<source->count;i++)for(j=0;j<target->count;j++) {
        float ray[6],center[3],point[3],t,radius;double norm=0,dot=0;
        if(!isfinite(source->items[i].radius)||source->items[i].radius<=0||
           !isfinite(target->items[j].radius)||target->items[j].radius<=0)return RF_FORMAT;
        radius=source->items[i].radius+target->items[j].radius;if(!isfinite(radius))return RF_FORMAT;
        for(k=0;k<3;k++) {
            double a=moving->position[k],b=fixed->position[k];
            for(n=0;n<3;n++) {
                a+=(double)source->items[i].center[n]*moving->orientation[n*3+k];
                b+=(double)target->items[j].center[n]*fixed->orientation[n*3+k];
            }
            ray[k]=(float)a;center[k]=(float)b;ray[k+3]=delta[k]/length;
            if(!isfinite(ray[k])||!isfinite(center[k]))return RF_FORMAT;
        }
        /* Diagnostic receipt of the closest permitted translation segment.
         * Sample only finite validated geometry; never alter the collision result. */
        {
            double projection=0,distance2=0;float nearest,gap,previous;
            for(k=0;k<3;k++)projection+=((double)center[k]-ray[k])*delta[k];
            nearest=(float)fmax(0.0,fmin((double)limit,projection/magnitude));
            for(k=0;k<3;k++) {double d=(double)ray[k]+delta[k]*nearest-center[k];distance2+=d*d;}
            gap=(float)(sqrt(distance2)-radius);
            memcpy(&previous,rf_scene_drone_separation+2,4);
            if(!rf_scene_drone_separation[0] || gap<previous) {
                rf_scene_drone_separation[1]=rf_scene_drone_player_contact[4];
                memcpy(rf_scene_drone_separation+2,&gap,4);
                memcpy(rf_scene_drone_separation+3,ray,12);
                memcpy(rf_scene_drone_separation+6,center,12);
                memcpy(rf_scene_drone_separation+9,&source->items[i].radius,4);
                memcpy(rf_scene_drone_separation+10,&target->items[j].radius,4);
                memcpy(rf_scene_drone_separation+11,delta,12);
                memcpy(rf_scene_drone_separation+14,&limit,4);
                memcpy(rf_scene_drone_separation+15,&nearest,4);
                rf_scene_drone_separation[16]=i;rf_scene_drone_separation[17]=j;
            }
            ++rf_scene_drone_separation[0];
        }
        if(!rf_collision_ray_sphere(ray,length,center,radius,point,&t))continue;
        ++rf_scene_drone_separation[18];
        if(!isfinite(t))return RF_FORMAT;if(t<0||t>=limit)continue;
        for(k=0;k<3;k++){double d=(double)point[k]-center[k];norm+=d*d;dot+=d*delta[k];}
        if(!t&&dot>=0)continue;
        if(!(norm>0))continue;
        norm=sqrt(norm);memset(hit,0,sizeof(*hit));
        for(k=0;k<3;k++) {
            hit->contact.normal[k]=(float)(((double)point[k]-center[k])/norm);
            hit->contact.point[k]=point[k]-hit->contact.normal[k]*source->items[i].radius;
        }
        hit->contact.fraction=t;hit->contact.texture=hit->contact.face_token=UINT32_MAX;
        hit->solid=hit->room=hit->face=UINT32_MAX;hit->sphere=i;hit->hits=1;
        limit=t;*found=1;
    }
    return RF_OK;
}
