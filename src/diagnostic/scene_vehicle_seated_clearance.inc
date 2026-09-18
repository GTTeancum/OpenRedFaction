#ifndef RF_SCENE_VEHICLE_SEATED_CLEARANCE_INC
#define RF_SCENE_VEHICLE_SEATED_CLEARANCE_INC
/* Practical seated-head clearance, not a bypass of world/mover collision.
 * Include inside entry adapter after scene_driller_player_shape. The actual
 * top player sphere supplies radius and center offset relative to authored
 * standing eye; no invented tiny radius. Seated legs/standing capsule are
 * intentionally excluded from transit inside the host. Caller MUST retain
 * full standing destination/floor/host exclusion checks when exiting.
 * Rotation follows the existing endpoint-center chord approximation.
 */
static int scene_vehicle_seated_clearance(const scene_vehicle_collision *collision,const rf_physics_body *player,
    const float eye_offset[3],const float start[3],const float start_basis[9],
    const float end[3],const float end_basis[9],uint32_t *clear)
{
    rf_collision_body_sphere head={{0,0,0},0};rf_collision_body_query query={0};rf_geometry_body_hit hit={0};
    uint32_t i,j,index=0,found=0;float offset[3];int status;
    if(!collision||!player||!eye_offset||!start||!start_basis||!end||!end_basis||!clear||
       !player->spheres.items||!player->spheres.count||player->spheres.count>64)return RF_RANGE;
    for(i=0;i<player->spheres.count;i++){
        const rf_physics_sphere *sphere=player->spheres.items+i;
        if(!isfinite(sphere->radius)||sphere->radius<=0)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(sphere->center[j]))return RF_FORMAT;
        /* Highest center identifies head rather than a large torso sphere
         * whose radius happens to overlap higher. Stable first wins ties. */
        if(sphere->center[1]>player->spheres.items[index].center[1])index=i;
    }
    head.radius=player->spheres.items[index].radius;
    for(i=0;i<3;i++){
        if(!isfinite(eye_offset[i])||!isfinite(start[i])||!isfinite(end[i]))return RF_FORMAT;
        offset[i]=player->spheres.items[index].center[i]-eye_offset[i];
    }
    for(i=0;i<9;i++)if(!isfinite(start_basis[i])||!isfinite(end_basis[i]))return RF_FORMAT;
    query.spheres=&head;query.count=1;query.radius=head.radius;query.flags=collision->flags;query.limit=1;
    query.matrix[0][0]=query.matrix[1][1]=query.matrix[2][2]=1;
    for(i=0;i<3;i++){
        query.start[i]=start[i];query.end[i]=end[i];
        for(j=0;j<3;j++){
            /* World eye plus the real model head-to-eye vector. */
            query.start[i]+=start_basis[j*3+i]*eye_offset[j]+start_basis[j*3+i]*offset[j];
            query.end[i]+=end_basis[j*3+i]*eye_offset[j]+end_basis[j*3+i]*offset[j];
        }
        if(!isfinite(query.start[i])||!isfinite(query.end[i]))return RF_FORMAT;
    }
    status=scene_vehicle_collision_query(collision,&query,&hit,&found);if(status)return status;
    if(found>1)return RF_FORMAT;
    *clear=!found;return RF_OK;
}
static int scene_vehicle_seated_path(scene_driller_entry_exit *entry,const scene_vehicle_pose *from,
    const scene_vehicle_pose *to,uint32_t *clear)
{
    if(!entry||!from||!to)return RF_RANGE;
    return scene_vehicle_seated_clearance(&entry->collision,entry->player_body,entry->eye_offset,
        from->position,from->basis,to->position,to->basis,clear);
}
#endif
