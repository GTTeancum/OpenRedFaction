/* Frame-local motion survives the controller position commit. This is owned
 * scene scheduling data, not persistent save state or a velocity extrapolation. */
typedef struct scene_mover_interval {
    float start[3],end[3],matrix[9],end_matrix[9];
    uint32_t handle,changed; /* bit0 translation, bit1 orientation */
} scene_mover_interval;
static scene_mover_interval *campaign_mover_intervals;
static rf_group_attached_pose *campaign_fragment_mover_poses;
static float campaign_mover_interval_seconds;
static uint32_t campaign_mover_translation_count,campaign_mover_changed_count;
static int scene_mover_intervals_capture(const rf_geometry_collision_movers *movers,
    scene_mover_interval *items,uint32_t capacity,uint32_t finish)
{
    uint32_t i,j;
    if(!movers || movers->count>capacity || (movers->count && (!items || !movers->poses || !movers->views)))return RF_RANGE;
    for(i=0;i<movers->count;i++) {
        const rf_group_attached_pose *pose=movers->poses+i;
        if(finish && items[i].handle!=movers->views[i].object_id)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(pose->position[j]))return RF_FORMAT;
        for(j=0;j<9;j++)if(!isfinite(pose->input_matrix[j]))return RF_FORMAT;
    }
    for(i=0;i<movers->count;i++) {
        const rf_group_attached_pose *pose=movers->poses+i;scene_mover_interval *item=items+i;
        if(!finish) {
            memcpy(item->start,pose->position,12);memcpy(item->matrix,pose->input_matrix,36);
            item->handle=movers->views[i].object_id;
        }
        memcpy(item->end,pose->position,12);memcpy(item->end_matrix,pose->input_matrix,36);
        item->changed=(memcmp(item->start,item->end,12)!=0) |
            ((memcmp(item->matrix,item->end_matrix,36)!=0)<<1);
    }
    return RF_OK;
}
