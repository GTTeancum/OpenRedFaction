#ifndef RF_SCENE_CROUCH_QUERY_GEOMETRY_INCLUDED
#define RF_SCENE_CROUCH_QUERY_GEOMETRY_INCLUDED
/*429b99..429bdd: base object-info (+294) crouching eye offset +a8 is rotated with4faa90,
 * rounded to binary32, then40a030 adds actor position. The target endpoint
 * is the already resolved48a8d0 eye/position. This helper does no visibility
 * query. Do not substitute alternate render-class (+29c) offsets without
 * proving constructor pointer aliasing. The caller must use the actual0x27/0x461 world-query path. */
typedef struct scene_crouch_query_geometry {float start[3],end[3];} scene_crouch_query_geometry;
static int scene_crouch_query_prepare(const float position[3],const float orientation[9],
    const float crouching_offset[3],const float target_eye[3],scene_crouch_query_geometry *out)
{
    scene_crouch_query_geometry next;
    if(!position||!orientation||!crouching_offset||!target_eye||!out)return RF_RANGE;
    for(uint32_t i=0;i<9;i++)if(!isfinite(orientation[i]))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(!isfinite(position[i])||!isfinite(crouching_offset[i])||!isfinite(target_eye[i]))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++){
        float rotated=(float)(((double)crouching_offset[2]*orientation[i+6]+
            (double)crouching_offset[1]*orientation[i+3])+(double)crouching_offset[0]*orientation[i]);
        next.start[i]=position[i]+rotated;next.end[i]=target_eye[i];
        if(!isfinite(rotated)||!isfinite(next.start[i]))return RF_RANGE;
    }
    *out=next;return RF_OK;
}
#endif
