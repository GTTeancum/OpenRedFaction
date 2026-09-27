/* Installed L1S2 authored-cut checkpoint staging, without the ordinary world envelope. */
#include "../src/diagnostic/scene.c"
#define CHECK(call) do { int check_status=(call); if(check_status) { \
    fprintf(stderr,"L1S2 stage line%d status%d %s\n",__LINE__,check_status,#call);return 1; \
} } while(0)
static const char *map_names[6]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp","ui.vpp"};
int main(int argc,char **argv)
{
    rf_vpp archive={0},maps[6]={{0}};rf_level level;rf_geometry geometry={0};
    rf_geometry_collision_world world={0};rf_materials materials={0};
    rf_scene_world_geometry render={0};scene_stream *s=NULL;
    rf_geomod_template shape;scene_terrain_lighting_stage *lighting=NULL;
    scene_authored_checkpoint_stage *restore=NULL;rf_geomod_terrain_view candidate,before,after;
    const rf_geomod_publication_origin *origins=NULL;rf_preview_surface_lightmap *bindings=NULL;
    const float center[3]={121.208061f,-2.12633848f,-17.f};
    const float basis[9]={1,0,0,0,1,0,0,0,1};
    unsigned char *save=NULL;uint32_t bytes=0,i,pair_mode,second_mode;char path[1024];
    pair_mode=argc==4 && (!strcmp(argv[3],"paired") || !strcmp(argv[3],"paired-second"));
    second_mode=pair_mode && !strcmp(argv[3],"paired-second");
    if(argc!=3 && !pair_mode)return 2;
    s=calloc(1,sizeof(*s));save=malloc(SCENE_CHECKPOINT_MAX);if(!s || !save)return 1;
    snprintf(path,sizeof(path),"%s/levels1.vpp",argv[1]);CHECK(rf_vpp_open(&archive,path));
    CHECK(rf_level_open(&level,&archive,"L1S2.rfl"));
    CHECK(rf_geometry_open(&geometry,&level,8u*1024u*1024u));
    CHECK(rf_geometry_collision_world_open(&geometry,16u*1024u*1024u,&world));
    CHECK(rf_lightmap_rgb_open(&s->light_rgb,&level,16u*1024u*1024u));
    for(i=0;i<6;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],map_names[i]);CHECK(rf_vpp_open(maps+i,path));}
    render.slots=calloc(geometry.textures,sizeof(*render.slots));if(!render.slots)return 1;
    for(i=0;i<geometry.textures;i++)render.slots[i]=i;
    render.world=&geometry;render.material_count=geometry.textures;
    actor_follow_world=&render;s->geometry=&geometry;s->collision=&world;s->materials=&materials;
    materials.count=geometry.textures+1;s->terrain_material=geometry.textures;
    s->terrain_texture_width=s->terrain_texture_height=256;
    {
        rf_vpp tables={0};rf_vpp_entry entry;void *text;
        snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);CHECK(rf_vpp_open(&tables,path));
        CHECK(rf_vpp_find(&tables,"materials.tbl",&entry));text=malloc(entry.size);if(!text)return 1;
        campaign_surface_palette=malloc(sizeof(*campaign_surface_palette));if(!campaign_surface_palette)return 1;
        CHECK(rf_vpp_read(&tables,&entry,0,text,entry.size));
        CHECK(rf_surface_materials_read(text,entry.size,campaign_surface_palette));
        free(text);rf_vpp_close(&tables);
    }
    if(pair_mode) {
        const uint32_t uids[2]={8123,8219};
        rf_scene_vehicle_enabled=1;
        CHECK(scene_terrain_sources_open(s,&level,maps,6,uids,2,6u*1024u*1024u));
    } else CHECK(scene_terrain_authored_open_source(s,&level,maps,6,8123));
    CHECK(scene_terrain_publication_open(s));
    CHECK(rf_geomod_template_load(argv[2],&shape));s->terrain_template=&shape;
    s->terrain_noise=calloc(1,sizeof(*s->terrain_noise));
    s->terrain_atlas_pixels=calloc(512u*512u*2u,1);
    s->terrain_tile=calloc(64u*64u*2u,1);
    s->terrain_bindings=calloc(SCENE_TERRAIN_FACES,sizeof(*s->terrain_bindings));
    s->terrain_colors=calloc(SCENE_TERRAIN_DRAW_VERTICES,sizeof(*s->terrain_colors));
    s->terrain_light_cache=calloc(1,sizeof(*s->terrain_light_cache));
    s->terrain_tiles=calloc(SCENE_TERRAIN_FACES,sizeof(*s->terrain_tiles));
    s->terrain_draw=calloc(1,sizeof(*s->terrain_draw));
    s->light_overlay_work=calloc(1100,sizeof(uint32_t)+sizeof(rf_vfx_light_source));
    if(!s->terrain_noise || !s->terrain_atlas_pixels || !s->terrain_tile || !s->terrain_bindings ||
       !s->terrain_colors || !s->terrain_light_cache || !s->terrain_tiles || !s->terrain_draw ||
       !s->light_overlay_work)return 1;
    s->terrain_atlas_registered=1;s->terrain_atlas_index=s->light_rgb.count;
    strcpy(campaign_current_level,"L1S2.rfl");
    memcpy(s->terrain_history_minimum,world.minimum,12);
    memcpy(s->terrain_history_maximum,world.maximum,12);
    if(pair_mode) {
        CHECK(scene_terrain_authored_template_edit(s,center,basis,1.f/shape.radius,NULL,0));
        if(second_mode) {
            const float next_center[3]={124.314514f,-2.12633848f,-17.f};
            const float next_basis[9]={-0.287775129f,0.000001598f,-0.957698166f,
                -0.004944316f,0.999986291f,0.001487379f,
                0.957684517f,0.005163210f,-0.287771463f};
            rf_geomod_shallow_limit shallow={{0,-1,0},0.4f};
            uint32_t touched[4],n=0,previous_serial=s->terrain_publication_serial;
            int second_status;
            CHECK(scene_terrain_authored_affected(s,next_center,next_basis,1.f,&shallow,1,touched,&n));
            if(n!=2 || touched[0]!=0 || touched[1]!=1)return 1;
            rf_scene_combat_trace=1;
            second_status=scene_terrain_authored_template_edit(s,next_center,next_basis,1.f,&shallow,1);
            rf_scene_combat_trace=0;
            if(second_status!=RF_NOT_FOUND || s->terrain_publication_serial!=previous_serial ||
               s->terrain_publication->has_pending)return 1;
            printf("PASS L1S2 second-contact admission rollback status%d serial%u\n",
                second_status,previous_serial);
        }
        CHECK(scene_terrain_publication_view(s,&candidate));
        if(candidate.cuts!=1 || candidate.mesh.generation!=1 ||
           candidate.mesh.face_count<s->terrain_authored->windows.face_count ||
           s->terrain_publication->replaced_count!=91 ||
           rf_scene_authored_collection[0]!=2 || s->terrain_publication->has_pending)return 1;
        printf("PASS L1S2 paired scene edit faces%u vertices%u replaced%u\n",
            candidate.mesh.face_count,candidate.mesh.vertex_count,
            s->terrain_publication->replaced_count);
        goto complete;
    }
    CHECK(scene_checkpoint_identity(s,&level));
    CHECK(rf_geomod_terrain_cut_template(s->terrain,&shape,center,basis,1.f,s->terrain_material));
    CHECK(scene_terrain_publication_prepare(s));
    CHECK(scene_terrain_publication_candidate(s,&candidate,&origins,&bindings));
    CHECK(scene_terrain_lighting_stage_prepare(s,&candidate,bindings,0,&lighting));
    CHECK(scene_terrain_lighting_stage_draw(lighting));
    CHECK(scene_terrain_publication_finish(s,lighting->staged->terrain_bindings,
        candidate.mesh.face_count,s->light_rgb.count+1));
    scene_terrain_lighting_stage_commit(lighting);
    scene_terrain_lighting_stage_discard(&lighting);
    s->terrain_publication_serial=candidate.mesh.generation;
    CHECK(rf_geomod_terrain_get(s->terrain,&before));
    CHECK(scene_authored_checkpoint_write(s,save,SCENE_CHECKPOINT_MAX,&bytes));
    CHECK(scene_authored_checkpoint_stage_prepare(s,save,bytes,NULL,&restore));
    if(!restore || restore->pieces || restore->cuts!=1)return 1;
    CHECK(scene_authored_checkpoint_stage_commit(restore));
    scene_authored_checkpoint_stage_discard(&restore);
    CHECK(rf_geomod_terrain_get(s->terrain,&after));
    if(before.cuts!=after.cuts || before.mesh.face_count!=after.mesh.face_count ||
       s->terrain_publication->has_pending)return 1;
    printf("PASS L1S2 authored checkpoint stage bytes%u faces%u cuts%u\n",bytes,after.mesh.face_count,after.cuts);
complete:
    rf_geometry_collision_overlay_close(&s->terrain_collision);
    scene_terrain_publication_close(&s->terrain_publication);
    if(pair_mode)scene_terrain_sources_close(s);
    else {rf_geomod_terrain_close(&s->terrain);scene_terrain_authored_close(&s->terrain_authored);}
    free(s->terrain_noise);free(s->terrain_atlas_pixels);free(s->terrain_tile);
    free(s->terrain_bindings);free(s->terrain_colors);free(s->terrain_light_cache);
    free(s->terrain_tiles);free(s->terrain_draw);free(s->light_overlay_work);
    rf_lightmap_rgb_close(&s->light_rgb);rf_geometry_collision_world_close(&world);
    rf_geometry_close(&geometry);for(i=0;i<6;i++)rf_vpp_close(maps+i);
    rf_vpp_close(&archive);free(render.slots);free(campaign_surface_palette);
    campaign_surface_palette=NULL;actor_follow_world=NULL;free(save);free(s);
    return 0;
}
