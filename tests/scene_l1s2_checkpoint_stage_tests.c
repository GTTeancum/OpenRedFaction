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
    scene_authored_checkpoint_stage *restore=NULL;scene_authored_collection_stage *collection_stage=NULL;
    rf_geomod_terrain_view candidate,before,after;
    const rf_geomod_publication_origin *origins=NULL;rf_preview_surface_lightmap *bindings=NULL;
    const float center[3]={121.208061f,-2.12633848f,-17.f};
    const float basis[9]={1,0,0,0,1,0,0,0,1};
    unsigned char *save=NULL,*resave=NULL;uint32_t bytes=0,resaved=0,i,pair_mode,second_mode,commit_mode;char path[1024];
    pair_mode=argc==4 && (!strcmp(argv[3],"paired") || !strcmp(argv[3],"paired-second") ||
        !strcmp(argv[3],"paired-commit"));
    second_mode=pair_mode && !strcmp(argv[3],"paired-second");
    commit_mode=pair_mode && !strcmp(argv[3],"paired-commit");
    if(argc!=3 && !pair_mode)return 2;
    s=calloc(1,sizeof(*s));save=malloc(SCENE_CHECKPOINT_MAX);
    resave=malloc(SCENE_CHECKPOINT_MAX);if(!s || !save || !resave)return 1;
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
    if(commit_mode) {
        CHECK(scene_terrain_render_exclude(s,s->terrain_publication->replaced_ids,
            s->terrain_publication->replaced_count));
        s->collision=&s->terrain_collision.world;
    }
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
    if(!scene_world_l1s2_authored_terrain(s))return 1;
    memcpy(s->terrain_history_minimum,world.minimum,12);
    memcpy(s->terrain_history_maximum,world.maximum,12);
    CHECK(scene_checkpoint_identity(s,&level));
    if(pair_mode) {
        scene_authored_sources_stage *source_stage=NULL;
        rf_authored_sources_layout source_layout;
        rf_geomod_terrain_view source_core;
        CHECK(scene_terrain_authored_template_edit(s,center,basis,1.f/shape.radius,NULL,0));
        if(commit_mode) {
            const float next_center[3]={124.314514f,-2.12633848f,-17.f};
            const float next_basis[9]={-0.287775129f,0.000001598f,-0.957698166f,
                -0.004944316f,0.999986291f,0.001487379f,
                0.957684517f,0.005163210f,-0.287771463f};
            rf_geomod_shallow_limit shallow={{0,-1,0},0.4f};
            scene_l1s2_detail_owner *detail=s->terrain_publication->detail;
            CHECK(scene_terrain_authored_template_edit(s,next_center,next_basis,1.f,&shallow,1));
            if(s->terrain_publication_serial!=2 || !detail || !detail->published ||
               s->collision!=&detail->overlay.world ||
               s->terrain_geometry.faces!=geometry.faces-s->terrain_publication->replaced_count-detail->source.face_count ||
               detail->banks[detail->active].mesh.face_count!=74)return 1;
            for(i=0;i<detail->source.face_count;i++) {
                uint32_t j;
                for(j=0;j<s->terrain_geometry.faces;j++)
                    if(s->terrain_geometry.face_offsets[j]==geometry.face_offsets[detail->replaced[i]])return 1;
            }
            printf("PASS L1S2 paired second commit room8=%u room121=%u static_faces=%u\n",
                s->terrain_collision.world.rooms[8].tree.face_count,
                detail->overlay.world.rooms[121].tree.face_count,s->terrain_geometry.faces);
        }
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
            printf("PASS L1S2 second-contact two-room commit gate rollback status%d serial%u\n",
                second_status,previous_serial);
            {
                scene_authored_edit_context factory={0};rf_geomod_terrain *detail_core=NULL;
                rf_geomod_publication_cut cutter;rf_collision_composition_view composed,room8;
                rf_collision_tree_hit hit={0};uint32_t history_bytes,found=0,original_detail_faces;uint64_t stage_peak;
                unsigned char *history_blob;
                const float ray_start[3]={126.4099f,-1.5967f,-17.7522f};
                const float ray_delta[3]={0.4371f,-0.2428f,0};
                scene_l1s2_detail_owner *detail=s->terrain_publication->detail;
                scene_l1s2_detail_bank *bank;
                if(!detail || detail->source.face_count!=30)return 1;
                CHECK(rf_geomod_terrain_history_size(s->terrain,&history_bytes));
                history_blob=malloc(history_bytes);if(!history_blob)return 1;
                CHECK(rf_geomod_terrain_history_encode(s->terrain,history_blob,history_bytes));
                factory.scene=s;factory.authored=s->terrain_authored;
                CHECK(scene_authored_edit_create(&factory,SCENE_TERRAIN_CORE_BUDGET,&detail_core));
                CHECK(rf_geomod_terrain_history_decode(detail_core,history_blob,history_bytes));
                CHECK(rf_geomod_terrain_cut_template_limits(detail_core,&shape,next_center,next_basis,
                    1.f,s->terrain_material,&shallow,1));
                CHECK(rf_geomod_terrain_cutter_get(detail_core,1,&cutter.mesh,cutter.kernel,&cutter.star));
                CHECK(scene_l1s2_detail_prepare(detail,&s->terrain_publication->work,s->geometry,
                    &cutter,1,previous_serial+1));
                bank=detail->banks+detail->pending;
                CHECK(rf_collision_composition_pending(detail->composition,&composed));
                CHECK(rf_collision_composition_get(s->terrain_publication->composition,&room8));
                stage_peak=(uint64_t)scene_publication_owned_bytes(s->terrain_publication)+
                    room8.resident_bytes+composed.peak_bytes+s->terrain_collision.resident_bytes;
                if(stage_peak>SCENE_PUBLICATION_BUDGET)return 1;
                CHECK(rf_collision_thin_tree(composed.tree->nodes,composed.tree->node_count,
                    composed.tree->faces,composed.tree->face_count,4,ray_start,ray_delta,1.f,
                    composed.tree->stack,composed.tree->node_count,&hit,&found));
                if(bank->mesh.face_count!=74 || bank->mesh.vertex_count!=297 ||
                   (found && composed.face_ids[composed.tree->source_indices[hit.face_index]]==4972))return 1;
                printf("PASS L1S2 room121 scene candidate faces%u vertices%u stage_peak%u\n",
                    bank->mesh.face_count,bank->mesh.vertex_count,(uint32_t)stage_peak);
                scene_l1s2_detail_abort(detail);
                if(detail->has_pending || s->terrain_publication->has_pending ||
                   s->terrain_publication_serial!=previous_serial)return 1;
                CHECK(rf_collision_composition_get(detail->composition,&composed));
                original_detail_faces=composed.tree->face_count;
                found=0;
                CHECK(rf_collision_thin_tree(composed.tree->nodes,composed.tree->node_count,
                    composed.tree->faces,composed.tree->face_count,4,ray_start,ray_delta,1.f,
                    composed.tree->stack,composed.tree->node_count,&hit,&found));
                if(!found || composed.face_ids[composed.tree->source_indices[hit.face_index]]!=4972)return 1;
                CHECK(scene_l1s2_detail_prepare(detail,&s->terrain_publication->work,s->geometry,
                    &cutter,1,previous_serial+1));
                CHECK(scene_l1s2_detail_publish(detail));
                if(detail->has_pending || detail->banks[detail->active].mesh.face_count!=74 ||
                   detail->overlay.world.views[121].tree!=&detail->overlay.world.rooms[121].tree ||
                   detail->overlay.world.rooms[121].tree.face_count<=original_detail_faces ||
                   detail->overlay.world.rooms[8].tree.face_count!=s->terrain_collision.world.rooms[8].tree.face_count)
                    return 1;
                {
                    rf_geometry_world_hit world_hit={0};uint32_t world_found=0;
                    CHECK(rf_geometry_collision_world_ray(&detail->overlay.world,4,
                        ray_start,ray_delta,1.f,&world_hit,&world_found));
                    if(world_found && world_hit.room==121 && world_hit.face==4972)return 1;
                }
                {
                    rf_level camera={0};rf_preview_mesh projected={0};
                    const scene_l1s2_detail_bank *draw_bank=detail->banks+detail->active;
                    const rf_geomod_face *face=draw_bank->mesh.faces;
                    const float *normal=draw_bank->bound[0].plane;
                    float forward[3],right[3],up[3],length,seed[3];uint32_t corner,axis,mapped=0;
                    projected.vertices=calloc(2048,sizeof(*projected.vertices));
                    if(!projected.vertices)return 1;
                    for(corner=0;corner<face->count;corner++)for(axis=0;axis<3;axis++)
                        camera.player_position[axis]+=draw_bank->mesh.vertices[face->first+corner].position[axis]/face->count;
                    for(axis=0;axis<3;axis++){
                        camera.player_position[axis]+=normal[axis]*5.f;
                        forward[axis]=-normal[axis];
                    }
                    seed[0]=0;seed[1]=fabsf(forward[1])<.9f?1.f:0.f;
                    seed[2]=seed[1]?0.f:1.f;
                    right[0]=seed[1]*forward[2]-seed[2]*forward[1];
                    right[1]=seed[2]*forward[0]-seed[0]*forward[2];
                    right[2]=seed[0]*forward[1]-seed[1]*forward[0];
                    length=sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
                    if(length<.01f)return 1;
                    for(axis=0;axis<3;axis++)right[axis]/=length;
                    up[0]=forward[1]*right[2]-forward[2]*right[1];
                    up[1]=forward[2]*right[0]-forward[0]*right[2];
                    up[2]=forward[0]*right[1]-forward[1]*right[0];
                    memcpy(camera.player_orientation[0],right,12);
                    memcpy(camera.player_orientation[1],up,12);
                    memcpy(camera.player_orientation[2],forward,12);
                    CHECK(rf_preview_geomod_world_lit(&projected,2048*sizeof(*projected.vertices),
                        &draw_bank->mesh,draw_bank->bound,materials.count,&camera,
                        draw_bank->colors,&geometry));
                    if(!projected.count || projected.bytes!=projected.count*sizeof(*projected.vertices))return 1;
                    for(i=0;i<projected.count;i++)if(projected.vertices[i].lightmap<RF_PREVIEW_VERTEX_LIT)mapped++;
                    printf("PASS L1S2 room121 authored draw projection vertices%u mapped%u\n",
                        projected.count,mapped);
                    free(projected.vertices);
                }
                printf("PASS L1S2 paired collision overlay room8=%u room121=%u\n",
                    detail->overlay.world.rooms[8].tree.face_count,
                    detail->overlay.world.rooms[121].tree.face_count);
                CHECK(scene_l1s2_detail_prepare_reset(detail));
                CHECK(scene_l1s2_detail_publish(detail));
                if(detail->published || detail->overlay.world.rooms[121].tree.face_count!=
                   world.rooms[121].tree.face_count)return 1;
                rf_geomod_terrain_close(&detail_core);free(history_blob);
            }
        }
        CHECK(scene_terrain_publication_view(s,&candidate));
        if(candidate.cuts!=(commit_mode?3u:1u) || candidate.mesh.generation!=(commit_mode?2u:1u) ||
           candidate.mesh.face_count<s->terrain_authored->windows.face_count ||
           s->terrain_publication->replaced_count!=91 ||
           rf_scene_authored_collection[0]!=2 || s->terrain_publication->has_pending)return 1;
        CHECK(scene_authored_sources_write(s,save,SCENE_CHECKPOINT_MAX,&bytes));
        CHECK(scene_authored_sources_match(s,save,bytes,&source_layout));
        if(source_layout.count!=2 || source_layout.sources[0].piece_bytes ||
           source_layout.sources[1].piece_bytes)return 1;
        CHECK(scene_authored_sources_stage_prepare(s,save,bytes,s->terrain_publication_serial,
            6u*1024u*1024u,&source_stage));
        CHECK(rf_geomod_terrain_get(source_stage->cores[0],&source_core));
        if(source_core.cuts!=(commit_mode?2u:1u) || source_stage->pieces[0])return 1;
        CHECK(rf_geomod_terrain_get(source_stage->cores[1],&source_core));
        if(source_core.cuts!=(commit_mode?1u:0u) || source_stage->pieces[1])return 1;
        scene_authored_sources_stage_discard(&source_stage);
        rf_scene_combat_trace=1;
        CHECK(scene_authored_collection_checkpoint_write(s,save,SCENE_CHECKPOINT_MAX,&bytes));
        CHECK(scene_authored_collection_stage_prepare(s,save,bytes,&collection_stage));
        CHECK(scene_authored_collection_stage_commit(collection_stage,NULL));
        rf_scene_combat_trace=0;
        scene_authored_collection_stage_discard(&collection_stage);
        CHECK(scene_authored_collection_checkpoint_write(s,resave,SCENE_CHECKPOINT_MAX,&resaved));
        if(resaved!=bytes || memcmp(save,resave,bytes))return 1;
        if(commit_mode && (!s->terrain_publication->detail->published ||
           s->collision!=&s->terrain_publication->detail->overlay.world ||
           s->terrain_publication->detail->banks[s->terrain_publication->detail->active].mesh.face_count!=74))return 1;
        printf("PASS L1S2 paired scene edit and RFDS3 roundtrip bytes%u faces%u vertices%u replaced%u\n",
            bytes,
            candidate.mesh.face_count,candidate.mesh.vertex_count,
            s->terrain_publication->replaced_count);
        if(commit_mode) {
            const uint32_t indices[2]={0,1};
            scene_authored_edit_context reset[2]={{0}};
            for(i=0;i<2;i++){reset[i].scene=s;reset[i].reset=1;}
            CHECK(scene_terrain_authored_edit_group(s,indices,reset,2));
            if(s->terrain_publication->detail->published ||
               s->collision!=&s->terrain_collision.world ||
               s->terrain_geometry.faces!=geometry.faces-s->terrain_publication->replaced_count ||
               s->terrain_publication->detail->overlay.world.rooms[121].tree.face_count!=
                   world.rooms[121].tree.face_count)return 1;
            CHECK(scene_terrain_publication_view(s,&candidate));
            if(candidate.cuts || candidate.mesh.face_count || s->terrain_publication_serial!=3)return 1;
            printf("PASS L1S2 paired reset restores room121 and original static detail serial%u\n",
                s->terrain_publication_serial);
            CHECK(scene_terrain_authored_template_edit(s,center,basis,1.f/shape.radius,NULL,0));
            CHECK(scene_terrain_publication_view(s,&candidate));
            if(candidate.cuts!=1 || s->terrain_publication->detail->published)return 1;
        }
        goto complete;
    }
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
    free(s->terrain_face_offsets);
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
    campaign_surface_palette=NULL;actor_follow_world=NULL;free(save);free(resave);free(s);
    return 0;
}
