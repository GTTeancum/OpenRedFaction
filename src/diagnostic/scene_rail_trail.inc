#ifndef RF_SCENE_RAIL_TRAIL_INC
#define RF_SCENE_RAIL_TRAIL_INC
/* Rail Driver shot visuals. weapons.tbl rail_gun has no V3D and names
 * $Bitmap Filename "railrifle1TrailAlpha.tga" (parsed at 4c33b7), so the
 * original draws the slug as a bitmap object. Its exact draw path is not yet
 * reconstructed. Port approximation (labelled, not RE): on each shot a beam
 * of that bitmap runs from the gun to the first world surface on the shot
 * line, 0.25 units wide, fading out over 0.4 s; that surface also takes the
 * ordinary rail impact sound and spark (scene_weapon_impact_world). Rail
 * damage and piercing are unchanged (scene_precision_gameplay.inc). */
#define SCENE_RAIL_TRAILS 4u
#define SCENE_RAIL_TRAIL_SECONDS .4f
static struct {float start[3],end[3],age;uint32_t active;} scene_rail_trails[SCENE_RAIL_TRAILS];
static rf_image scene_rail_trail_image;static uint32_t scene_rail_trail_ready;
/* shots, beams drawn, image loaded, world contacts */
uint32_t rf_scene_rail_trail[4];
static int scene_rail_trail_open(rf_vpp *maps,uint32_t map_count)
{
    uint32_t i;
    memset(scene_rail_trails,0,sizeof(scene_rail_trails));memset(rf_scene_rail_trail,0,sizeof(rf_scene_rail_trail));
    if(scene_rail_trail_ready){rf_scene_rail_trail[2]=1;return RF_OK;}
    for(i=0;i<map_count;i++){rf_vpp_entry entry;
        if(rf_vpp_find(maps+i,"railrifle1TrailAlpha.tga",&entry)!=RF_OK)continue;
        if(!rf_image_open(&scene_rail_trail_image,maps+i,&entry,256*1024))scene_rail_trail_ready=rf_scene_rail_trail[2]=1;
        break;}
    return RF_OK; /* Optional: a missing bitmap only drops the beam. */
}
static void scene_rail_trail_close(void)
{
    if(scene_rail_trail_ready)rf_image_close(&scene_rail_trail_image);
    scene_rail_trail_ready=0;memset(scene_rail_trails,0,sizeof(scene_rail_trails));
}
/* Fire: world contact along the full shot path gets the impact; the beam ends there. */
static void scene_rail_trail_fire(scene_stream *stream,int32_t weapon,const float eye[3],const float path[3])
{
    float matrix[3][3],end[3];rf_collision_solid_hit hit;uint32_t matched=0,i,k,oldest=0;
    ++rf_scene_rail_trail[0];
    for(k=0;k<3;k++)end[k]=eye[k]+path[k];
    if(stream->collision && !rf_geometry_collision_ray(stream->collision,&campaign_movers,eye,end,0x26,&hit,&matched) && matched) {
        memcpy(end,hit.hit.point,12);++rf_scene_rail_trail[3];
        scene_weapon_impact_world(stream,weapon,eye,path,hit.hit.fraction);
    }
    if(rf_entity_navigation_basis(path,matrix))return;
    for(i=0;i<SCENE_RAIL_TRAILS;i++){if(!scene_rail_trails[i].active){oldest=i;break;}
        if(scene_rail_trails[i].age>scene_rail_trails[oldest].age)oldest=i;}
    for(k=0;k<3;k++)scene_rail_trails[oldest].start[k]=eye[k]+matrix[2][k]*.4f+matrix[0][k]*.18f-matrix[1][k]*.2f;
    memcpy(scene_rail_trails[oldest].end,end,12);scene_rail_trails[oldest].age=0;scene_rail_trails[oldest].active=1;
}
static void scene_rail_trail_tick(void)
{
    uint32_t i;
    for(i=0;i<SCENE_RAIL_TRAILS;i++)if(scene_rail_trails[i].active && (scene_rail_trails[i].age+=scene_step_seconds)>=SCENE_RAIL_TRAIL_SECONDS)
        scene_rail_trails[i].active=0;
}
static int scene_rail_trail_draw(scene_stream *stream,rf_scene_particle_sink sink,void *context)
{
    uint32_t i,k;int status;
    if(!scene_rail_trail_ready || !sink)return RF_OK;
    for(i=0;i<SCENE_RAIL_TRAILS;i++){
        rf_particle_screen_polygon polygon={0};rf_particle_draw_vertex vertices[12];
        rf_particle_vertex_environment e={0};rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
        float fade;uint32_t alpha;
        if(!scene_rail_trails[i].active)continue;
        fade=1-scene_rail_trails[i].age/SCENE_RAIL_TRAIL_SECONDS;alpha=(uint32_t)(fade*255);if(alpha>255)alpha=255;
        status=rf_volume_beam_project(&stream->particle_camera,scene_rail_trails[i].end,scene_rail_trails[i].start,.25f,&polygon);
        if(status)return status;if(!polygon.count)continue;
        status=rf_particle_render_decode(RF_PARTICLE_NORMAL_MODE,&re,&states);if(status)return status;
        e.rgba=0x00ffffffu|(alpha<<24);e.vertex_color=states.vertex_color;e.vertex_alpha=states.vertex_alpha;
        e.depth_scale=e.reciprocal_scale=stream->particle_camera.view.scale[2];e.uv_scale[0]=e.uv_scale[1]=1;
        for(k=0;k<polygon.count;k++){status=rf_particle_vertex_encode(&e,polygon.vertices+k,vertices+k);if(status)return status;}
        status=sink(context,vertices,polygon.count,&scene_rail_trail_image,RF_PARTICLE_NORMAL_MODE);if(status)return status;
        ++rf_scene_rail_trail[1];
    }
    return RF_OK;
}
#endif
