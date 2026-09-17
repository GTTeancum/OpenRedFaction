/* Installed CPU pipeline, no window/GPU/emulator. Arguments: Installed_Game RFCT. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"authored digest line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int reject_atomic(scene_stream *s,const rf_geomod_terrain_view *v,const rf_geomod_publication_origin *o,
    const rf_collision_composition_view *cv,scene_stream *lighting,uint32_t serial)
{
    rf_authored_owner_expected output,old;uint16_t tail[768],saved[768];uint32_t peak=0xabcdef01;
    memset(&output,0xa5,sizeof(output));memcpy(&old,&output,sizeof(old));memset(tail,0x97,sizeof(tail));memcpy(saved,tail,sizeof(saved));
    CHECK(scene_authored_digest_capture(s,v,o,cv,lighting,serial,tail,768,&output,&peak)!=RF_OK);
    CHECK(!memcmp(&output,&old,sizeof(old)) && !memcmp(tail,saved,sizeof(tail)) && peak==0xabcdef01);return 0;
}
int main(int argc,char **argv)
{
    static const char *names[6]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp","ui.vpp"};
    rf_vpp archive={0},maps[6]={{0}};rf_level level;rf_geometry geometry={0};rf_geometry_collision_world world={0};
    rf_materials materials={0};rf_scene_world_geometry render={0};scene_stream s={0};rf_geomod_template shape;
    rf_geomod_terrain_view candidate;const rf_geomod_publication_origin *origins;rf_preview_surface_lightmap *bindings;
    rf_collision_composition_view cv;scene_terrain_lighting_stage *stage=NULL,*clone=NULL;
    rf_authored_owner_expected expected,again,before_unused;uint16_t tail[768],other[768];
    uint32_t i,j,step,peak,peak2,atlas_hash,unused=0;char path[1024];
    const float centers[2][3]={{-4.75f,-.9f,2.5f},{-4.75f,.2f,2.5f}},basis[9]={1,0,0,0,1,0,0,0,1};
    CHECK(argc==3);snprintf(path,sizeof(path),"%s/levelsm.vpp",argv[1]);CHECK(!rf_vpp_open(&archive,path));
    CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geometry_collision_world_open(&geometry,16*1024*1024,&world));CHECK(!rf_lightmap_rgb_open(&s.light_rgb,&level,16*1024*1024));
    for(i=0;i<6;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],names[i]);CHECK(!rf_vpp_open(maps+i,path));}
    render.slots=calloc(geometry.textures,sizeof(*render.slots));CHECK(render.slots);
    for(i=0;i<geometry.textures;i++)render.slots[i]=i;render.world=&geometry;render.material_count=geometry.textures;
    actor_follow_world=&render;s.geometry=&geometry;s.collision=&world;s.materials=&materials;materials.count=geometry.textures+1;
    s.terrain_material=geometry.textures;s.terrain_texture_width=s.terrain_texture_height=256;
    /* Production authored ownership now requires the physical material table. */
    {
        rf_vpp tables={0};rf_vpp_entry entry;void *text;
        snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);CHECK(!rf_vpp_open(&tables,path));
        CHECK(!rf_vpp_find(&tables,"materials.tbl",&entry));text=malloc(entry.size);CHECK(text);
        campaign_surface_palette=malloc(sizeof(*campaign_surface_palette));CHECK(campaign_surface_palette);
        CHECK(!rf_vpp_read(&tables,&entry,0,text,entry.size));
        CHECK(!rf_surface_materials_read(text,entry.size,campaign_surface_palette));free(text);rf_vpp_close(&tables);
    }
    CHECK(!scene_terrain_authored_open(&s,&level,maps,6));CHECK(s.terrain_authored->identity_manifest.substrate);
    CHECK(!scene_terrain_publication_open(&s));CHECK(!rf_geomod_template_load(argv[2],&shape));
#define ALLOC(field,n) do{s.field=calloc((n),sizeof(*s.field));CHECK(s.field);}while(0)
    ALLOC(terrain_noise,1);ALLOC(terrain_atlas_pixels,512*512*2);ALLOC(terrain_tile,64*64*2);
    ALLOC(terrain_bindings,SCENE_TERRAIN_FACES);ALLOC(terrain_colors,SCENE_TERRAIN_DRAW_VERTICES);
    ALLOC(terrain_light_cache,1);ALLOC(terrain_tiles,SCENE_TERRAIN_FACES);ALLOC(terrain_draw,1);
#undef ALLOC
    s.light_overlay_work=calloc(1100,sizeof(uint32_t)+sizeof(rf_vfx_light_source));CHECK(s.light_overlay_work);
    s.terrain_atlas_registered=1;s.terrain_atlas_index=s.light_rgb.count;
    strcpy(campaign_current_level,"ctf06.rfl");s.terrain_template=&shape;
    memcpy(s.terrain_history_minimum,world.minimum,12);memcpy(s.terrain_history_maximum,world.maximum,12);
    CHECK(!scene_checkpoint_identity(&s,&level));

    for(step=1;step<=2;step++) {
        CHECK(!rf_geomod_piece_registry_begin(s.detached_pieces,0));
        CHECK(!rf_geomod_terrain_cut_template(s.terrain,&shape,centers[step-1],basis,1.05000007f,s.terrain_material));
        rf_geomod_piece_registry_commit(s.detached_pieces);
        CHECK(!scene_terrain_publication_prepare(&s));CHECK(!scene_terrain_publication_candidate(&s,&candidate,&origins,&bindings));
        CHECK(candidate.cuts==step && candidate.tree->face_count==786+candidate.mesh.face_count);
        s.terrain_publication_serial=candidate.mesh.generation;
        CHECK(!rf_collision_composition_pending(s.terrain_publication->composition,&cv));
        CHECK(!scene_terrain_lighting_stage_prepare(&s,&candidate,bindings,0,&stage));
        CHECK(!scene_authored_digest_capture(&s,&candidate,origins,&cv,stage->staged,candidate.mesh.generation,tail,768,&expected,&peak));
        CHECK(peak<512*1024 && expected.uid==94 && expected.source_count==6 && expected.neighbor_count==3);
        CHECK(!scene_terrain_lighting_stage_clone(&s,&candidate,&clone));
        for(i=0;i<9;i++)memcpy(clone->buffers[i].copy,stage->buffers[i].copy,stage->buffers[i].bytes);
        CHECK(!scene_authored_digest_capture(&s,&candidate,origins,&cv,clone->staged,candidate.mesh.generation,other,768,&again,&peak2));
        CHECK(!memcmp(&expected,&again,sizeof(expected)) && peak==peak2 && !memcmp(tail,other,candidate.mesh.face_count*2));
        /* Dynamic GPU-atlas appearance is excluded; private base journal is not. */
        atlas_hash=npc_hash_bytes(2166136261u,clone->staged->terrain_atlas_pixels,512*512*2);
        memset(clone->staged->terrain_atlas_pixels,0x33,512*512*2);
        CHECK(atlas_hash!=npc_hash_bytes(2166136261u,clone->staged->terrain_atlas_pixels,512*512*2));
        CHECK(!scene_authored_digest_capture(&s,&candidate,origins,&cv,clone->staged,candidate.mesh.generation,other,768,&again,&peak2));
        CHECK(!memcmp(&expected,&again,sizeof(expected)));
        for(i=0;i<candidate.mesh.face_count;i++)if(tail[i]!=65535)break;CHECK(i<candidate.mesh.face_count);
        clone->staged->terrain_bindings[i].projection.offset[0]+=.125f;
        CHECK(!reject_atomic(&s,&candidate,origins,&cv,clone->staged,candidate.mesh.generation));
        clone->staged->terrain_bindings[i]=stage->staged->terrain_bindings[i];
        clone->staged->terrain_noise->maps[tail[i]].base_seed^=1;
        CHECK(!reject_atomic(&s,&candidate,origins,&cv,clone->staged,candidate.mesh.generation));
        clone->staged->terrain_noise->maps[tail[i]].base_seed^=1;
        {uint32_t *ids=malloc(cv.count*4);rf_collision_composition_view bad=cv;CHECK(ids);memcpy(ids,cv.face_ids,cv.count*4);
         ids[1]=ids[0];bad.face_ids=ids;CHECK(!reject_atomic(&s,&candidate,origins,&bad,clone->staged,candidate.mesh.generation));free(ids);}
        scene_terrain_lighting_stage_discard(&clone);
        if(step==2) {
            scene_terrain_noise_owner *n=stage->staged->terrain_noise;scene_terrain_noise_map *m;
            rf_random_state chain=n->random;uint32_t axis;
            /* Valid unreferenced history is retained even if no final face uses it. */
            before_unused=expected;CHECK(n->count && n->count<1024);m=n->maps+n->count;*m=n->maps[0];m->base_seed=chain.value;
            if(n->x+m->width>512){n->y+=n->row;n->x=n->row=0;}m->x=n->x;m->y=n->y;CHECK(m->y+m->height<=512);
            for(axis=0;axis<2;axis++){uint32_t a=m->binding.projection.axes[axis];double scale=((axis?m->height:m->width)-2)/(double)(m->maximum[a]-m->minimum[a]);
                m->binding.projection.offset[axis]=(float)(((axis?m->y:m->x)+1-m->minimum[a]*scale)/512);}
            CHECK(!scene_checkpoint_base(stage->staged,m,&chain));n->random=chain;n->x+=m->width;if(m->height>n->row)n->row=m->height;n->count++;n->bake=n->count;
            CHECK(!scene_authored_digest_capture(&s,&candidate,origins,&cv,stage->staged,candidate.mesh.generation,tail,768,&expected,&peak));
            CHECK(!memcmp(before_unused.publication_digest,expected.publication_digest,32) && !memcmp(before_unused.collision_digest,expected.collision_digest,32));
            CHECK(memcmp(before_unused.material_digest,expected.material_digest,32));
            for(i=0;i<n->count;i++){for(j=0;j<candidate.mesh.face_count;j++)if(tail[j]==i)break;if(j==candidate.mesh.face_count)unused++;}CHECK(unused);
        }
        CHECK(!scene_terrain_lighting_stage_draw(stage));
        CHECK(!scene_terrain_publication_finish(&s,stage->staged->terrain_bindings,candidate.mesh.face_count,s.light_rgb.count+1));
        scene_terrain_lighting_stage_commit(stage);CHECK(stage->committed);
        CHECK(!rf_collision_composition_get(s.terrain_publication->composition,&cv));
        CHECK(!scene_authored_digest_capture(&s,&candidate,origins,&cv,&s,candidate.mesh.generation,other,768,&again,&peak2));
        CHECK(!memcmp(&expected,&again,sizeof(expected)) && !memcmp(tail,other,candidate.mesh.face_count*2));
        scene_terrain_lighting_stage_discard(&stage);
        {
            unsigned char *save=malloc(SCENE_CHECKPOINT_MAX);uint32_t bytes=0;
            scene_authored_checkpoint_stage *restore=NULL;rf_geomod_terrain *live=s.terrain;
            uint32_t live_serial=s.terrain_publication_serial;
            if(rf_geomod_piece_registry_count(s.detached_pieces)) {
                rf_geomod_piece_batch *batch;rf_geomod_owned_piece piece;rf_physics_body *body;
                CHECK(!rf_geomod_piece_registry_get(s.detached_pieces,0,&batch));
                CHECK(!rf_geomod_piece_batch_get(batch,0,&piece,&body));
                body->state.position[0]+=.125f;body->state.velocity[1]=-2;body->state.coefficients[0]=.2f;
            }
            CHECK(save);CHECK(!scene_authored_checkpoint_write(&s,save,SCENE_CHECKPOINT_MAX,&bytes));
            CHECK(bytes>416 && !memcmp(save,"RFDS",4) && checkpoint_u32(save+4)==2);
            {unsigned char guard[32],before[32];uint32_t written=0x12345678;
             memset(guard,0xa5,sizeof(guard));memcpy(before,guard,sizeof(guard));
             CHECK(scene_authored_checkpoint_write(&s,guard,sizeof(guard),&written)==RF_RANGE);
             CHECK(written==0x12345678 && !memcmp(guard,before,sizeof(guard)));}

            CHECK(!scene_authored_checkpoint_stage_prepare(&s,save,bytes,NULL,&restore));
            CHECK(restore && restore->cuts==step && restore->serial==live_serial);
            CHECK(!memcmp(&restore->expected,&expected,sizeof(expected)));
            CHECK(s.terrain==live && s.terrain_publication_serial==live_serial);
            scene_authored_checkpoint_stage_discard(&restore);
            CHECK(!restore && !s.terrain_publication->has_pending && s.terrain==live);
            if(checkpoint_u32(save+12)>16) {
                uint32_t offset=bytes-checkpoint_u32(save+12)+16+12;
                uint32_t old=checkpoint_u32(save+offset);
                checkpoint_put(save+offset,0x7fc00000u);
                CHECK(scene_authored_checkpoint_stage_prepare(&s,save,bytes,NULL,&restore)!=RF_OK);
                CHECK(!restore && !s.terrain_publication->has_pending && s.terrain==live);
                CHECK(s.terrain_publication_serial==live_serial);checkpoint_put(save+offset,old);
            }
            save[320]^=1; /* Publication digest corruption must reject atomically. */
            CHECK(scene_authored_checkpoint_stage_prepare(&s,save,bytes,NULL,&restore)!=RF_OK);
            CHECK(!restore && !s.terrain_publication->has_pending && s.terrain==live);
            CHECK(s.terrain_publication_serial==live_serial);
            save[320]^=1;
            CHECK(!scene_authored_checkpoint_stage_prepare(&s,save,bytes,NULL,&restore));
            CHECK(!scene_authored_checkpoint_stage_commit(restore));
            CHECK(!restore->owns_pending && !restore->core && restore->lighting->committed);
            CHECK(s.terrain!=live && s.terrain_publication_serial==live_serial);
            CHECK(scene_authored_checkpoint_stage_commit(restore)==RF_RANGE);
            scene_authored_checkpoint_stage_discard(&restore);
            {unsigned char *resaved=malloc(SCENE_CHECKPOINT_MAX);uint32_t again_bytes=0;CHECK(resaved);
             CHECK(!scene_authored_checkpoint_write(&s,resaved,SCENE_CHECKPOINT_MAX,&again_bytes));
             CHECK(again_bytes==bytes && !memcmp(save,resaved,bytes));free(resaved);}
            free(save);
        }

    }
    /* Reset restores all790 original rows, with explicitly cleared journal. */
    CHECK(!rf_geomod_piece_registry_begin(s.detached_pieces,1));
    CHECK(!rf_geomod_terrain_reset(s.terrain));rf_geomod_piece_registry_commit(s.detached_pieces);CHECK(!scene_terrain_publication_prepare(&s));
    CHECK(!scene_terrain_publication_candidate(&s,&candidate,&origins,&bindings));CHECK(!candidate.cuts && !candidate.mesh.face_count);
    CHECK(!rf_collision_composition_pending(s.terrain_publication->composition,&cv) && cv.count==790);
    CHECK(!scene_terrain_lighting_stage_clone_mode(&s,&candidate,1,&clone));memset(clone->staged->terrain_noise,0,sizeof(*s.terrain_noise));
    CHECK(!scene_authored_digest_capture(&s,&candidate,origins,&cv,clone->staged,candidate.mesh.generation,NULL,0,&again,&peak));
    CHECK(memcmp(expected.publication_digest,again.publication_digest,32) && memcmp(expected.collision_digest,again.collision_digest,32));
    CHECK(!reject_atomic(&s,&candidate,origins,&cv,&s,candidate.mesh.generation)); /* old cut journal cannot masquerade as reset */
    scene_terrain_lighting_stage_discard(&clone);
    CHECK(!scene_terrain_lighting_stage_empty(&s,&candidate,candidate.mesh.generation,0,1,&clone));
    CHECK(!scene_terrain_publication_finish(&s,clone->staged->terrain_bindings,0,s.light_rgb.count+1));
    scene_terrain_lighting_stage_commit(clone);scene_terrain_lighting_stage_discard(&clone);
    s.terrain_publication_serial=candidate.mesh.generation;
    {
        unsigned char *save=malloc(SCENE_CHECKPOINT_MAX);uint32_t bytes=0;
        scene_authored_checkpoint_stage *restore=NULL;CHECK(save);
        CHECK(!scene_authored_checkpoint_write(&s,save,SCENE_CHECKPOINT_MAX,&bytes));
        CHECK(!scene_authored_checkpoint_stage_prepare(&s,save,bytes,NULL,&restore));
        CHECK(restore && !restore->cuts && restore->serial==s.terrain_publication_serial);
        CHECK(restore->lighting->staged->terrain_noise->random.value==1);
        CHECK(!scene_authored_checkpoint_stage_commit(restore));
        scene_authored_checkpoint_stage_discard(&restore);CHECK(!s.terrain_publication->has_pending);
        CHECK(!s.terrain_noise->count && s.terrain_noise->random.value==1);
        {rf_collision_composition_view restored;CHECK(!rf_collision_composition_get(s.terrain_publication->composition,&restored));CHECK(restored.count==790);}
        free(save);
    }

    rf_geometry_collision_overlay_close(&s.terrain_collision);scene_terrain_publication_close(&s.terrain_publication);
    rf_geomod_piece_registry_close(&s.detached_pieces);free(campaign_surface_palette);campaign_surface_palette=NULL;
    rf_geomod_terrain_close(&s.terrain);scene_terrain_authored_close(&s.terrain_authored);
    free(s.terrain_noise);free(s.terrain_atlas_pixels);free(s.terrain_tile);free(s.terrain_bindings);free(s.terrain_colors);
    free(s.terrain_light_cache);free(s.terrain_tiles);free(s.terrain_draw);free(s.light_overlay_work);
    rf_lightmap_rgb_close(&s.light_rgb);rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);
    for(i=0;i<6;i++)rf_vpp_close(maps+i);rf_vpp_close(&archive);actor_follow_world=NULL;free(render.slots);
    printf("PASS installed authored three-digest private/live2cut, unused%u, dynamic exclusion, reset, atomic failures; scratch%u\n",unused,peak);return 0;
}
