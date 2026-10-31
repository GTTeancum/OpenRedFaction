#ifndef RF_SCENE_RAIL_TRAIL_INC
#define RF_SCENE_RAIL_TRAIL_INC
/* Rail Driver shot visuals. weapons.tbl rail_gun has no V3D and names
 * $Bitmap Filename "railrifle1TrailAlpha.tga" (parsed at 4c33b7), so the
 * original draws the slug as a bitmap object. Its exact draw path is not yet
 * reconstructed. Port approximation (labelled, not RE): on each shot a beam
 * of that bitmap runs from the drawn barrel to the first world surface on the shot
 * line, 0.25 units wide, fading out over 0.4 s; that surface also takes the
 * ordinary rail impact sound and spark (scene_weapon_impact_world). Rail
 * damage and piercing are unchanged (scene_precision_gameplay.inc). */
#define SCENE_RAIL_TRAILS 4u
#define SCENE_RAIL_TRAIL_SECONDS .4f
static struct {float start[3],end[3],age;uint32_t active;} scene_rail_trails[SCENE_RAIL_TRAILS];
static rf_image scene_rail_trail_image;static uint32_t scene_rail_trail_ready;
/* tracer01.tga (maps2.vpp, 64x64, black background): the player's tracer streak
 * drawn camera-facing (scene_player_rounds.inc), since Tracer01.VFX's planes lie
 * along the flight line and are edge-on from the shooter's eye. */
static rf_image scene_tracer_beam_image;static uint32_t scene_tracer_beam_ready;
/* shots, beams drawn, image loaded, world contacts */
uint32_t rf_scene_rail_trail[4];
static int scene_rail_trail_open(rf_vpp *maps,uint32_t map_count)
{
    uint32_t i;
    memset(scene_rail_trails,0,sizeof(scene_rail_trails));memset(rf_scene_rail_trail,0,sizeof(rf_scene_rail_trail));
    if(!scene_tracer_beam_ready)for(i=0;i<map_count;i++){rf_vpp_entry entry;
        if(rf_vpp_find(maps+i,"tracer01.tga",&entry)!=RF_OK)continue;
        if(!rf_image_open(&scene_tracer_beam_image,maps+i,&entry,64*1024))scene_tracer_beam_ready=1;
        break;}
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
    if(scene_tracer_beam_ready)rf_image_close(&scene_tracer_beam_image);
    scene_rail_trail_ready=scene_tracer_beam_ready=0;memset(scene_rail_trails,0,sizeof(scene_rail_trails));
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
    scene_player_muzzle_world(eye,scene_rail_trails[oldest].start); /* drawn barrel (scene_player_muzzle.inc) */
    memcpy(scene_rail_trails[oldest].end,end,12);scene_rail_trails[oldest].age=0;scene_rail_trails[oldest].active=1;
}
static void scene_rail_trail_tick(void)
{
    uint32_t i;
    for(i=0;i<SCENE_RAIL_TRAILS;i++)if(scene_rail_trails[i].active && (scene_rail_trails[i].age+=scene_step_seconds)>=SCENE_RAIL_TRAIL_SECONDS)
        scene_rail_trails[i].active=0;
}
/* beam calls, projected polygons submitted, last polygon vertex count, last status */
uint32_t rf_scene_beam_draws[4];
/* One camera-facing textured streak from start to end. Port geometry: the
 * volume-light beam (555b80) and stretched particles (558e30) both collapse
 * when the axis runs along the view, as a shot leaving the player's gun does. */
static int scene_beam_draw(scene_stream *stream,rf_scene_particle_sink sink,void *context,const rf_image *image,
    const float start[3],const float end[3],float width,uint32_t rgba,uint32_t mode)
{
    rf_particle_screen_polygon polygon={0};rf_particle_draw_vertex vertices[12];
    rf_particle_vertex_environment e={0};rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
    uint32_t k;int status;
    ++rf_scene_beam_draws[0];
    {   /* Corners offset by half the width along cross(axis, view ray): the quad
         * faces the camera whatever the axis. u runs across, v along the streak. */
        rf_particle_billboard_vertex q[4];const float *eye=stream->particle_camera.view.origin;
        float axis[3],ray[3],side[3],length=0;uint32_t j;
        for(j=0;j<3;j++){axis[j]=end[j]-start[j];ray[j]=(start[j]+end[j])*.5f-eye[j];}
        side[0]=axis[1]*ray[2]-axis[2]*ray[1];side[1]=axis[2]*ray[0]-axis[0]*ray[2];side[2]=axis[0]*ray[1]-axis[1]*ray[0];
        for(j=0;j<3;j++)length+=side[j]*side[j];length=sqrtf(length);
        if(!(length>1e-6f) || !isfinite(length))return RF_OK;
        for(j=0;j<3;j++)side[j]*=width*.5f/length;
        for(j=0;j<3;j++){q[0].position[j]=start[j]-side[j];q[1].position[j]=start[j]+side[j];q[2].position[j]=end[j]+side[j];q[3].position[j]=end[j]-side[j];}
        q[0].uv[0]=0;q[0].uv[1]=1;q[1].uv[0]=1;q[1].uv[1]=1;q[2].uv[0]=1;q[2].uv[1]=0;q[3].uv[0]=0;q[3].uv[1]=0;
        status=rf_particle_world_quad(&stream->particle_camera,q,&polygon);
    }
    rf_scene_beam_draws[2]=polygon.count;rf_scene_beam_draws[3]=(uint32_t)status;
    if(status || !polygon.count)return status;
    ++rf_scene_beam_draws[1];
    status=rf_particle_render_decode(mode,&re,&states);if(status)return status;
    e.rgba=rgba;e.vertex_color=states.vertex_color;e.vertex_alpha=states.vertex_alpha;
    e.depth_scale=e.reciprocal_scale=stream->particle_camera.view.scale[2];e.uv_scale[0]=e.uv_scale[1]=1;
    for(k=0;k<polygon.count;k++){status=rf_particle_vertex_encode(&e,polygon.vertices+k,vertices+k);if(status)return status;}
    return sink(context,vertices,polygon.count,image,mode);
}
static int scene_rail_trail_draw(scene_stream *stream,rf_scene_particle_sink sink,void *context)
{
    uint32_t i;int status;
    if(!scene_rail_trail_ready || !sink)return RF_OK;
    for(i=0;i<SCENE_RAIL_TRAILS;i++){
        float fade;uint32_t alpha;
        if(!scene_rail_trails[i].active)continue;
        fade=1-scene_rail_trails[i].age/SCENE_RAIL_TRAIL_SECONDS;alpha=(uint32_t)(fade*255);if(alpha>255)alpha=255;
        status=scene_beam_draw(stream,sink,context,&scene_rail_trail_image,scene_rail_trails[i].start,scene_rail_trails[i].end,
            .25f,0x00ffffffu|(alpha<<24),RF_PARTICLE_NORMAL_MODE);if(status)return status;
        ++rf_scene_rail_trail[1];
    }
    return RF_OK;
}
#endif
