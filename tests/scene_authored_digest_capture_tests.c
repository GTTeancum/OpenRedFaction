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
static int beam_capture(scene_stream *live,const rf_level *level,rf_vpp *maps,const rf_geomod_template *shape) {
    scene_stream *s=malloc(sizeof(*s));scene_terrain_lighting_stage *stage=NULL;
    rf_geomod_terrain_view view;const rf_geomod_publication_origin *origins;rf_preview_surface_lightmap *bindings;
    rf_collision_composition_view composition;rf_authored_owner_expected expected,again;
    uint16_t tail[768],other[768];uint32_t peak,peak2,i,cap=UINT32_MAX,hidden=0,map,material;
    float center[3]={-4.75f,2.25f,2.5f},basis[9]={1,0,0,0,1,0,0,0,1};
    CHECK(s);*s=*live;s->terrain=NULL;s->terrain_authored=NULL;s->terrain_publication=NULL;s->detached_pieces=NULL;
    s->terrain_sources=NULL;s->terrain_source_count=0;memset(&s->terrain_collision,0,sizeof(s->terrain_collision));
    CHECK(!scene_terrain_authored_open_source(s,level,maps,6,95));CHECK(!scene_terrain_publication_open(s));
    CHECK(!rf_geomod_piece_registry_begin(s->detached_pieces,0));
    CHECK(!rf_geomod_terrain_cut_template(s->terrain,shape,center,basis,1.05000007f,s->terrain_material));
    rf_geomod_piece_registry_commit(s->detached_pieces);
    CHECK(!scene_terrain_publication_prepare(s));CHECK(!scene_terrain_publication_candidate(s,&view,&origins,&bindings));
    CHECK(!rf_collision_composition_pending(s->terrain_publication->composition,&composition));
    CHECK(!scene_terrain_lighting_stage_prepare(s,&view,bindings,0,&stage));
    CHECK(!scene_authored_digest_capture(s,&view,origins,&composition,stage->staged,view.mesh.generation,tail,768,&expected,&peak));
    CHECK(expected.uid==95 && peak<512*1024);
    for(i=0;i<view.mesh.face_count;i++)if(origins[i].reference==UINT32_MAX) {
        CHECK(origins[i].kind==RF_GEOMOD_PUBLICATION_NEIGHBOR && tail[i]<stage->staged->terrain_noise->count);
        CHECK(stage->staged->terrain_noise->maps[tail[i]].material==view.mesh.faces[i].material);cap=i;hidden++;
    }
    CHECK(hidden && cap!=UINT32_MAX);map=tail[cap];material=stage->staged->terrain_noise->maps[map].material;
    CHECK(material!=s->terrain_material);
    CHECK(!scene_authored_digest_capture(s,&view,origins,&composition,stage->staged,view.mesh.generation,other,768,&again,&peak2));
    CHECK(!memcmp(&expected,&again,sizeof(expected)) && peak==peak2 && !memcmp(tail,other,view.mesh.face_count*2));
    stage->staged->terrain_noise->maps[map].material=s->terrain_material;
    CHECK(!reject_atomic(s,&view,origins,&composition,stage->staged,view.mesh.generation));
    stage->staged->terrain_noise->maps[map].material=material;
    stage->staged->terrain_noise->maps[map].base_seed^=1;
    CHECK(!reject_atomic(s,&view,origins,&composition,stage->staged,view.mesh.generation));
    stage->staged->terrain_noise->maps[map].base_seed^=1;
    printf("BEAM_DIGEST faces%u hidden%u maps%u scratch%u three_domains_verified\n",view.mesh.face_count,hidden,stage->staged->terrain_noise->count,peak);
    CHECK(!scene_terrain_lighting_stage_draw(stage));
    CHECK(!scene_terrain_publication_finish(s,stage->staged->terrain_bindings,view.mesh.face_count,s->light_rgb.count+1));
    scene_terrain_lighting_stage_commit(stage);scene_terrain_lighting_stage_discard(&stage);
    s->terrain_publication_serial=view.mesh.generation;s->terrain_history_count=0;
    CHECK(!scene_checkpoint_identity(s,level));
    {
        unsigned char *packet=malloc(SCENE_CHECKPOINT_MAX),*again_bytes=malloc(SCENE_CHECKPOINT_MAX);
        uint32_t bytes=0,rewritten=0,token;rf_authored_checkpoint_layout layout;scene_authored_checkpoint_stage *restore=NULL;
        CHECK(packet && again_bytes);CHECK(!scene_authored_checkpoint_write(s,packet,SCENE_CHECKPOINT_MAX,&bytes));
        CHECK(checkpoint_u32(packet+312)==2);CHECK(!rf_authored_checkpoint_layout_read(packet,bytes,&layout));
        token=checkpoint_u32(packet+layout.map_offset+map*88+40);CHECK(token==4);
        CHECK(!scene_authored_checkpoint_stage_prepare(s,packet,bytes,NULL,&restore));
        CHECK(!memcmp(&expected,&restore->expected,sizeof(expected)) && restore->extension.material_policy==2);
        CHECK(restore->lighting->staged->terrain_noise->maps[map].material==material);
        CHECK(!scene_authored_checkpoint_stage_commit(restore));scene_authored_checkpoint_stage_discard(&restore);
        CHECK(!scene_authored_checkpoint_write(s,again_bytes,SCENE_CHECKPOINT_MAX,&rewritten));
        CHECK(bytes==rewritten && !memcmp(packet,again_bytes,bytes));
        checkpoint_put(packet+layout.map_offset+map*88+40,UINT32_MAX);
        CHECK(scene_authored_checkpoint_stage_prepare(s,packet,bytes,NULL,&restore)!=RF_OK && !restore && !s->terrain_publication->has_pending);
        checkpoint_put(packet+layout.map_offset+map*88+40,token);checkpoint_put(packet+312,1);
        CHECK(scene_authored_checkpoint_stage_prepare(s,packet,bytes,NULL,&restore)!=RF_OK && !restore && !s->terrain_publication->has_pending);
        printf("BEAM_CHECKPOINT bytes%u material_token%u exact_rewrite_and_rejection_verified\n",bytes,token);
        free(packet);free(again_bytes);
    }
    scene_terrain_lighting_stage_discard(&stage);scene_terrain_publication_abort(s);
    rf_geometry_collision_overlay_close(&s->terrain_collision);scene_terrain_publication_close(&s->terrain_publication);
    rf_geomod_piece_registry_close(&s->detached_pieces);rf_geomod_terrain_close(&s->terrain);scene_terrain_authored_close(&s->terrain_authored);free(s);return 0;
}
static int connected_capture(scene_stream *live,const rf_level *level,rf_vpp *maps,const rf_geomod_template *shape) {
    scene_stream *s=malloc(sizeof(*s));unsigned char *packet=malloc(SCENE_CHECKPOINT_MAX),*again=malloc(SCENE_CHECKPOINT_MAX);
    rf_geomod_terrain_view view;rf_preview_surface_lightmap *bindings;scene_terrain_lighting_stage *lights=NULL;
    float center[3]={-4.75f,2.25f,2.5f},basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i,step,bytes,rewritten;
    CHECK(s && packet && again);*s=*live;
    s->terrain=NULL;s->terrain_authored=NULL;s->detached_pieces=NULL;s->terrain_publication=NULL;
    s->terrain_sources=calloc(2,sizeof(*s->terrain_sources));CHECK(s->terrain_sources);s->terrain_source_count=2;
    s->terrain_history_count=0;s->terrain_checkpoint_loaded=0;s->terrain_publication_serial=0;
    memset(&s->terrain_collision,0,sizeof(s->terrain_collision));
#define CONNECT_ALLOC(field,n) do{s->field=calloc((n),sizeof(*s->field));CHECK(s->field);}while(0)
    CONNECT_ALLOC(terrain_noise,1);CONNECT_ALLOC(terrain_atlas_pixels,512*512*2);CONNECT_ALLOC(terrain_tile,64*64*2);
    CONNECT_ALLOC(terrain_bindings,SCENE_TERRAIN_FACES);CONNECT_ALLOC(terrain_colors,SCENE_TERRAIN_DRAW_VERTICES);
    CONNECT_ALLOC(terrain_light_cache,1);CONNECT_ALLOC(terrain_tiles,SCENE_TERRAIN_FACES);CONNECT_ALLOC(terrain_draw,1);
#undef CONNECT_ALLOC
    s->light_overlay_work=calloc(1100,sizeof(uint32_t)+sizeof(rf_vfx_light_source));CHECK(s->light_overlay_work);
    for(i=0;i<2;i++) {
        scene_stream owner=*s;owner.terrain_sources=NULL;owner.terrain_source_count=0;
        CHECK(!scene_terrain_authored_open_source(&owner,level,maps,6,i?94:95));
        s->terrain_sources[i]=(scene_terrain_source_owner){owner.terrain_authored,owner.terrain,owner.detached_pieces};
    }
    CHECK(!scene_terrain_sources_select(s,0));CHECK(!scene_terrain_publication_open(s));
    CHECK(!scene_checkpoint_identity(s,level));
    for(step=0;step<2;step++) {
        scene_authored_collection_stage *restore=NULL;rf_authored_checkpoint_layout layout;
        CHECK(!scene_terrain_sources_select(s,step));if(step)center[1]=1.9f;
        CHECK(!rf_geomod_piece_registry_begin(s->detached_pieces,0));
        CHECK(!rf_geomod_terrain_cut_template(s->terrain,shape,center,basis,1.05000007f,s->terrain_material));
        rf_geomod_piece_registry_commit(s->detached_pieces);s->terrain_publication_serial=step+1;
        CHECK(!scene_terrain_publication_prepare(s));CHECK(!scene_terrain_publication_candidate(s,&view,NULL,&bindings));
        CHECK(!scene_terrain_lighting_stage_prepare(s,&view,bindings,0,&lights));CHECK(!scene_terrain_lighting_stage_draw(lights));
        CHECK(!scene_terrain_publication_finish(s,lights->staged->terrain_bindings,view.mesh.face_count,s->light_rgb.count+1));
        scene_terrain_lighting_stage_commit(lights);scene_terrain_lighting_stage_discard(&lights);
        CHECK(!scene_authored_collection_checkpoint_write(s,packet,SCENE_CHECKPOINT_MAX,&bytes));
        CHECK(!rf_authored_collection_layout_read(packet,bytes,&layout));CHECK(checkpoint_u32(packet+312)==2);
        CHECK(!scene_authored_collection_stage_prepare(s,packet,bytes,&restore));
        CHECK(!scene_authored_collection_stage_commit(restore,NULL));scene_authored_collection_stage_discard(&restore);
        CHECK(!scene_authored_collection_checkpoint_write(s,again,SCENE_CHECKPOINT_MAX,&rewritten));
        CHECK(bytes==rewritten && !memcmp(packet,again,bytes));
        CHECK(!s->terrain_publication->has_pending && s->terrain_publication->connected);
        checkpoint_put(packet+312,1);
        CHECK(scene_authored_collection_stage_prepare(s,packet,bytes,&restore)!=RF_OK && !restore && !s->terrain_publication->has_pending);
        checkpoint_put(packet+312,2);
        for(i=0;i<layout.maps;i++)if(checkpoint_u32(packet+layout.map_offset+i*88+40))break;
        CHECK(i<layout.maps);
        {uint32_t offset=layout.map_offset+i*88+40,token=checkpoint_u32(packet+offset);
         checkpoint_put(packet+offset,UINT32_MAX);
         CHECK(scene_authored_collection_stage_prepare(s,packet,bytes,&restore)!=RF_OK && !restore && !s->terrain_publication->has_pending);
         checkpoint_put(packet+offset,token);}
        CHECK(!scene_authored_collection_checkpoint_write(s,again,SCENE_CHECKPOINT_MAX,&rewritten));
        CHECK(bytes==rewritten && !memcmp(packet,again,bytes));
        printf("CONNECTED_CHECKPOINT step%u bytes%u maps%u exact_rewrite\n",step+1,bytes,s->terrain_noise->count);
    }
    rf_geometry_collision_overlay_close(&s->terrain_collision);scene_terrain_publication_close(&s->terrain_publication);
    scene_terrain_sources_close(s);
    free(s->terrain_noise);free(s->terrain_atlas_pixels);free(s->terrain_tile);free(s->terrain_bindings);free(s->terrain_colors);
    free(s->terrain_light_cache);free(s->terrain_tiles);free(s->terrain_draw);free(s->light_overlay_work);
    free(s);free(packet);free(again);return 0;
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
                {
                    rf_physics_body_state saved=body->state;rf_geomod_piece_registry *registry=s.detached_pieces;
                    const rf_collision_face *face=piece.collision;float center[3]={0},start[3],delta[3];uint32_t blocked=99,k,n;
                    /* Isolate the shared NPC/player obstruction path from static
                     * world walls, using a real extracted polygon outside world bounds. */
                    memset(body->state.orientation,0,36);body->state.orientation[0]=body->state.orientation[4]=body->state.orientation[8]=1;
                    for(k=0;k<3;k++)body->state.position[k]=s.collision->maximum[k]+1000;
                    for(n=0;n<face->count;n++)for(k=0;k<3;k++)center[k]+=face->vertices[n][k]/face->count;
                    for(k=0;k<3;k++){start[k]=body->state.position[k]+center[k]+2*face->plane[k];delta[k]=-4*face->plane[k];}
                    s.detached_pieces=NULL;CHECK(!combat_shot_obstructed(&s,start,delta,1,&blocked) && !blocked);
                    s.detached_pieces=registry;
                    CHECK(!combat_shot_obstructed(&s,start,delta,1,&blocked) && blocked);
                    CHECK(!combat_shot_obstructed(&s,start,delta,.1f,&blocked) && !blocked);
                    body->state=saved;
                }
                {
                    rf_physics_body_state saved=body->state;rf_checkpoint_placement player={0};
                    rf_physics_sphere sphere={0};float old_speed=rf_scene_actor_movement_values.speed;
                    float delta[3],center[3]={0};const rf_collision_face *top=NULL;uint32_t k,n;
                    for(n=0;n<piece.mesh.face_count;n++)if(piece.collision[n].plane[1]>.5f){top=piece.collision+n;break;}
                    CHECK(top && save);
                    for(n=0;n<top->count;n++)for(k=0;k<3;k++)center[k]+=top->vertices[n][k]/top->count;
                    delta[0]=4.45f-body->state.position[0];delta[1]=-.5f-body->state.position[1];delta[2]=2.5f-body->state.position[2];
                    for(k=0;k<3;k++) {
                        body->state.position[k]+=delta[k];body->state.next_position[k]+=delta[k];
                        body->state.bounds.minimum[k]+=delta[k];body->state.bounds.maximum[k]+=delta[k];
                        body->state.velocity[k]=body->state.vector_c8[k]=body->state.mass_vector_d4[k]=0;
                        player.position[k]=body->state.position[k]+center[k]+.101f*top->plane[k];
                    }
                    body->state.flags&=~0x80000000u;
                    player.world=&world;player.replaced_room=s.terrain_collision.room;player.query_flags=4;
                    player.spheres=&sphere;player.count=1;sphere.radius=.1f;
                    player.basis[0]=player.basis[4]=player.basis[8]=1;
                    rf_scene_actor_movement_values.speed=5;
                    CHECK(!scene_authored_checkpoint_write(&s,save,SCENE_CHECKPOINT_MAX,&bytes));
                    /* Empty-grid fallback now gives this long fragment a real
                     * body bound; its sleeping geometry can support a save. */
                    CHECK(body->state.bounds.radius>.5f);
                    CHECK(!scene_authored_checkpoint_stage_prepare(&s,save,bytes,&player,&restore));
                    scene_authored_checkpoint_stage_discard(&restore);
                    CHECK(!restore && s.terrain==live && !s.terrain_publication->has_pending);
                    /* The existing moving-support rejection still applies. */
                    body->state.flags|=0x80000000u;
                    CHECK(!scene_authored_checkpoint_write(&s,save,SCENE_CHECKPOINT_MAX,&bytes));
                    CHECK(scene_authored_checkpoint_stage_prepare(&s,save,bytes,&player,&restore)==RF_FORMAT);
                    CHECK(!restore && s.terrain==live && !s.terrain_publication->has_pending);
                    body->state.flags&=~0x80000000u;
                    CHECK(!scene_authored_checkpoint_write(&s,save,SCENE_CHECKPOINT_MAX,&bytes));
                    for(k=0;k<3;k++)player.position[k]-=.08f*top->plane[k];
                    CHECK(scene_authored_checkpoint_stage_prepare(&s,save,bytes,&player,&restore)==RF_FORMAT);
                    CHECK(!restore && s.terrain==live && !s.terrain_publication->has_pending);
                    body->state=saved;rf_scene_actor_movement_values.speed=old_speed;
                }

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

    {
        scene_stream *pair=malloc(sizeof(*pair));unsigned char *packet=malloc(SCENE_CHECKPOINT_MAX),*kept=malloc(SCENE_CHECKPOINT_MAX);
        scene_authored_sources_stage *staged_sources=NULL,*failed_sources;
        rf_authored_sources_layout layout,old;uint32_t uids[2]={94,93},bytes=0,encoded_bytes;CHECK(pair && packet && kept);
        *pair=s;pair->terrain=NULL;pair->terrain_authored=NULL;pair->detached_pieces=NULL;
        pair->terrain_sources=NULL;pair->terrain_source_count=0;pair->terrain_publication=NULL;
        CHECK(!scene_terrain_sources_open(pair,&level,maps,6,uids,2,6u*1024u*1024u));
        for(i=0;i<2;i++) {
            float center[3]={-4.75f,-.9f,i?-2.5f:2.5f};
            CHECK(!scene_terrain_sources_select(pair,i));
            CHECK(!rf_geomod_piece_registry_begin(pair->detached_pieces,0));
            CHECK(!rf_geomod_terrain_cut_template(pair->terrain,&shape,center,basis,1.05000007f,pair->terrain_material));
            rf_geomod_piece_registry_commit(pair->detached_pieces);
        }
        memset(packet,0xa5,SCENE_CHECKPOINT_MAX);
        CHECK(!scene_authored_sources_write(pair,packet,SCENE_CHECKPOINT_MAX,&bytes));
        CHECK(!scene_authored_sources_match(pair,packet,bytes,&layout) && layout.count==2);
        CHECK(!scene_authored_sources_stage_prepare(pair,packet,bytes,1,8u*1024u*1024u,&staged_sources));
        for(i=0;i<2;i++) {
            rf_geomod_terrain *decoded=staged_sources->cores[i];rf_geomod_terrain_view before,after;
            rf_geomod_piece_registry *restored_pieces=staged_sources->pieces[i];
            scene_terrain_authored_assets *asset=pair->terrain_sources[i].authored;
            CHECK(!memcmp(layout.sources[i].identity,asset->source_identity,32));
            CHECK(layout.sources[i].uid==uids[i] && layout.sources[i].piece_bytes>16);
            CHECK(rf_geomod_piece_registry_count(pair->terrain_sources[i].pieces)>0);
            CHECK(!rf_geomod_piece_registry_state_size(pair->terrain_sources[i].pieces,&encoded_bytes));
            CHECK(encoded_bytes==layout.sources[i].piece_bytes);
            CHECK(!rf_geomod_piece_registry_state_encode(pair->terrain_sources[i].pieces,kept,encoded_bytes));
            CHECK(!memcmp(kept,packet+layout.sources[i].piece_offset,encoded_bytes));
            CHECK(!scene_terrain_sources_select(pair,i));
            CHECK(!rf_geomod_piece_registry_state_encode(restored_pieces,kept,encoded_bytes));
            CHECK(!memcmp(kept,packet+layout.sources[i].piece_offset,encoded_bytes));
            CHECK(!rf_geomod_terrain_get(pair->terrain,&before));CHECK(!rf_geomod_terrain_get(decoded,&after));
            CHECK(before.cuts==1 && after.cuts==1 && before.mesh.face_count==after.mesh.face_count && before.mesh.vertex_count==after.mesh.vertex_count);
            CHECK(!memcmp(before.mesh.faces,after.mesh.faces,before.mesh.face_count*sizeof(*before.mesh.faces)));
            CHECK(!memcmp(before.mesh.vertices,after.mesh.vertices,before.mesh.vertex_count*sizeof(*before.mesh.vertices)));

        }
        memcpy(kept,packet,SCENE_CHECKPOINT_MAX);old=layout;
        failed_sources=staged_sources;
        CHECK(scene_authored_sources_stage_prepare(pair,packet,bytes,1,staged_sources->reserved_bytes-1,&failed_sources)==RF_RANGE);
        CHECK(failed_sources==staged_sources);
        CHECK(scene_authored_sources_stage_prepare(pair,packet,bytes,0,8u*1024u*1024u,&failed_sources)==RF_FORMAT);
        /* Corrupt later-source health after the first source has reconstructed. */
        checkpoint_put(packet+layout.sources[1].piece_offset+16+320,0x7fc00000u);
        CHECK(scene_authored_sources_stage_prepare(pair,packet,bytes,1,8u*1024u*1024u,&failed_sources)==RF_FORMAT);
        CHECK(failed_sources==staged_sources);memcpy(packet,kept,SCENE_CHECKPOINT_MAX);
        scene_authored_sources_stage_discard(&staged_sources);
        /* Different runtime slot: histories remain canonical, and restored
         * extraction bodies retain exactly the saved state. */
        pair->terrain_material--;
        CHECK(!scene_authored_sources_stage_prepare(pair,packet,bytes,1,8u*1024u*1024u,&staged_sources));
        for(i=0;i<2;i++) {
            CHECK(!rf_geomod_terrain_history_encode(staged_sources->cores[i],kept,layout.sources[i].core_bytes));
            CHECK(!scene_checkpoint_materials(kept,layout.sources[i].core_bytes,pair->terrain_material,0));
            CHECK(!memcmp(kept,packet+layout.sources[i].core_offset,layout.sources[i].core_bytes));
            CHECK(!rf_geomod_piece_registry_state_encode(staged_sources->pieces[i],kept,layout.sources[i].piece_bytes));
            CHECK(!memcmp(kept,packet+layout.sources[i].piece_offset,layout.sources[i].piece_bytes));
        }
        printf("PASS private source restore: later-body rollback, canonical material remap, reservation%u bytes\n",staged_sources->reserved_bytes);
        scene_authored_sources_stage_discard(&staged_sources);pair->terrain_material++;
        memcpy(kept,packet,SCENE_CHECKPOINT_MAX);
        packet[16+48+16]^=1;CHECK(scene_authored_sources_match(pair,packet,bytes,&layout)==RF_FORMAT);
        CHECK(!memcmp(&old,&layout,sizeof(old)));memcpy(packet,kept,SCENE_CHECKPOINT_MAX);
        encoded_bytes=777;CHECK(scene_authored_sources_write(pair,packet,bytes-1,&encoded_bytes)==RF_RANGE);
        CHECK(encoded_bytes==777 && !memcmp(packet,kept,SCENE_CHECKPOINT_MAX));
        pair->terrain_sources[1].authored->source_identity[0]^=1;
        CHECK(scene_authored_sources_match(pair,packet,bytes,&layout)==RF_FORMAT);
        pair->terrain_sources[1].authored->source_identity[0]^=1;
        /* A selected core replacement must use its active alias without
         * mutating the collection entry during this read-only snapshot. */
        {rf_geomod_terrain *core=pair->terrain_sources[1].terrain;pair->terrain_sources[1].terrain=NULL;
         CHECK(!scene_authored_sources_write(pair,packet,SCENE_CHECKPOINT_MAX,&encoded_bytes));
         CHECK(!pair->terrain_sources[1].terrain && encoded_bytes==bytes && !memcmp(packet,kept,bytes));
         pair->terrain_sources[1].terrain=core;}
        {
            rf_geomod_terrain_view view;const rf_geomod_publication_origin *pair_origins;rf_preview_surface_lightmap *pair_bindings;
            rf_collision_composition_view composed;scene_terrain_lighting_stage *lights=NULL;
            rf_authored_owner_expected first,second;uint16_t maps_first[768],maps_second[768];uint32_t bytes_first,bytes_second;
            memset(&pair->terrain_collision,0,sizeof(pair->terrain_collision));
            CHECK(!scene_terrain_publication_open(pair));CHECK(!scene_terrain_publication_prepare(pair));
            CHECK(!scene_terrain_publication_candidate(pair,&view,&pair_origins,&pair_bindings));
            CHECK(!scene_terrain_lighting_stage_prepare(pair,&view,pair_bindings,0,&lights));
            CHECK(!rf_collision_composition_pending(pair->terrain_publication->composition,&composed));
            CHECK(!scene_authored_digest_capture(pair,&view,pair_origins,&composed,lights->staged,view.mesh.generation,maps_first,768,&first,&bytes_first));
            CHECK(!scene_terrain_sources_select(pair,0));
            CHECK(!scene_authored_digest_capture(pair,&view,pair_origins,&composed,lights->staged,view.mesh.generation,maps_second,768,&second,&bytes_second));
            CHECK(!memcmp(&first,&second,sizeof(first)) && bytes_first==bytes_second);
            CHECK(!memcmp(maps_first,maps_second,view.mesh.face_count*sizeof(*maps_first)));
            pair->terrain_sources[1].authored->source_identity[0]^=1;
            CHECK(!scene_authored_digest_capture(pair,&view,pair_origins,&composed,lights->staged,view.mesh.generation,maps_second,768,&second,&bytes_second));
            CHECK(memcmp(first.material_digest,second.material_digest,32) && memcmp(first.collision_digest,second.collision_digest,32));
            pair->terrain_sources[1].authored->source_identity[0]^=1;
            lights->staged->terrain_noise->random.value^=1;
            CHECK(!reject_atomic(pair,&view,pair_origins,&composed,lights->staged,view.mesh.generation));
            lights->staged->terrain_noise->random.value^=1;
            printf("PASS real paired scene digests: source selection invariant, second identity bound, shared atlas%u maps, faces%u\n",lights->staged->terrain_noise->count,view.mesh.face_count);
            CHECK(!scene_terrain_lighting_stage_draw(lights));
            CHECK(!scene_terrain_publication_finish(pair,lights->staged->terrain_bindings,view.mesh.face_count,pair->light_rgb.count+1));
            CHECK(!scene_terrain_publication_view(pair,&view));
            lights->staged->terrain_publication_serial=view.mesh.generation;
            {
                rf_authored_checkpoint_layout saved_layout;rf_authored_owner_extension extension;scene_authored_sources_stage *rebuilt=NULL;
                uint32_t saved_bytes=0;
                CHECK(scene_authored_checkpoint_write(lights->staged,packet,SCENE_CHECKPOINT_MAX,&saved_bytes)==RF_RANGE && !saved_bytes);
                CHECK(!scene_authored_collection_checkpoint_write(lights->staged,packet,SCENE_CHECKPOINT_MAX,&saved_bytes));
                CHECK(!rf_authored_collection_layout_read(packet,saved_bytes,&saved_layout));
                CHECK(!rf_authored_owner_collection_decode(packet+288,128,&first,2,view.cuts,&extension) && extension.mode==1);
                CHECK(rf_authored_owner_extension_decode(packet+288,128,&first,view.cuts,&extension)==RF_FORMAT);
                CHECK(!scene_authored_sources_stage_prepare(pair,packet+saved_layout.core_offset,saved_layout.core_bytes,view.mesh.generation,8u*1024u*1024u,&rebuilt));
                scene_authored_sources_stage_discard(&rebuilt);
                CHECK(saved_layout.maps==4 && saved_layout.faces==36 && !saved_layout.piece_bytes);
                {
                    scene_terrain_lighting_stage *restored_lights=NULL;scene_authored_admission_state *admission=malloc(sizeof(*admission));
                    rf_authored_owner_expected restored_digest;uint16_t restored_maps[768];uint32_t restored_peak;
                    CHECK(admission);
                    CHECK(!scene_authored_collection_admission_import(packet,&saved_layout,pair->terrain_history_minimum,pair->terrain_history_maximum,admission));
                    CHECK(admission->count==pair->terrain_history_count && admission->random.value==pair->terrain_random.value);
                    CHECK(!scene_terrain_lighting_stage_clone(pair,&view,&restored_lights));
                    CHECK(!scene_authored_collection_journal_import(pair,restored_lights,&saved_layout,packet,pair_origins,pair_bindings,view.mesh.generation,view.cuts));
                    CHECK(!scene_terrain_lighting_stage_bake(restored_lights,pair_bindings,0));
                    CHECK(!scene_terrain_lighting_stage_draw(restored_lights));
                    CHECK(!rf_collision_composition_get(pair->terrain_publication->composition,&composed));
                    CHECK(!scene_authored_digest_capture(pair,&view,pair_origins,&composed,restored_lights->staged,view.mesh.generation,restored_maps,768,&restored_digest,&restored_peak));
                    CHECK(!memcmp(&first,&restored_digest,sizeof(first)) && !memcmp(maps_first,restored_maps,view.mesh.face_count*sizeof(*maps_first)));
                    CHECK(!memcmp(lights->staged->terrain_atlas_pixels,restored_lights->staged->terrain_atlas_pixels,512u*512u*2u));
                    CHECK(!pair->terrain_noise->count); /* live empty atlas owner was untouched */
                    scene_terrain_lighting_stage_discard(&restored_lights);
                    /* A malformed last map may dirty the private stage, but
                     * cannot alter the live owner or the saved input. */
                    CHECK(!scene_terrain_lighting_stage_clone(pair,&view,&restored_lights));
                    packet[saved_layout.map_offset+3*88+60]^=1;
                    CHECK(scene_authored_collection_journal_import(pair,restored_lights,&saved_layout,packet,pair_origins,pair_bindings,view.mesh.generation,view.cuts)==RF_FORMAT);
                    CHECK(!pair->terrain_noise->count);packet[saved_layout.map_offset+3*88+60]^=1;
                    scene_terrain_lighting_stage_discard(&restored_lights);free(admission);
                    puts("PASS paired shared restore: exact atlas pixels, three digests and face bindings; malformed later map leaves live owner untouched");
                }
                {
                    scene_authored_collection_stage *complete=NULL;rf_geomod_terrain *live_core=lights->staged->terrain;
                    uint32_t active_bank=pair->terrain_publication->active;
                    int prepared=scene_authored_collection_stage_prepare(lights->staged,packet,saved_bytes,&complete);
                    if(prepared)printf("COLLECTION_STAGE_REJECT %d\n",prepared);
                    CHECK(!prepared && complete && complete->shared.lighting->draw_ready);
                    CHECK(!memcmp(&first,&complete->shared.expected,sizeof(first)));
                    {
                        rf_checkpoint_placement player={0};rf_physics_sphere sphere={0};float old_speed=rf_scene_actor_movement_values.speed;
                        rf_scene_actor_movement_values.speed=5;
                        player.world=&world;player.replaced_room=3;player.query_flags=4;player.spheres=&sphere;player.count=1;sphere.radius=.1f;
                        player.basis[0]=player.basis[4]=player.basis[8]=1;
                        player.position[0]=-2.75f;player.position[1]=0;player.position[2]=2.5f;
                        {
                            const rf_collision_tree *tree=complete->shared.lighting->candidate.tree;
                            rf_collision_tree_hit ground={0};float down[3]={0,-8,0};uint32_t found=0;
                            CHECK(!rf_collision_thin_tree(tree->nodes,tree->node_count,tree->faces,tree->face_count,4,player.position,down,1,tree->stack,tree->node_capacity,&ground,&found) && found);
                            player.position[1]=down[1]*ground.hit.fraction+.101f;
                        }
                        CHECK(!scene_authored_collection_stage_player(complete,&player));
                        player.position[0]=-5;player.position[1]=1;player.position[2]=-2.5f;
                        CHECK(scene_authored_collection_stage_player(complete,&player)==RF_FORMAT);
                        CHECK(scene_authored_collection_stage_commit(complete,&player)==RF_FORMAT);
                        rf_scene_actor_movement_values.speed=old_speed;
                    }
                    CHECK(!memcmp(lights->staged->terrain_atlas_pixels,complete->shared.lighting->staged->terrain_atlas_pixels,512u*512u*2u));
                    CHECK(lights->staged->terrain==live_core && pair->terrain_publication->active==active_bank);
                    printf("PASS complete paired private restore: digests/atlas exact, reserved%u bytes\n",complete->shared.reserved_peak_bytes);
                    scene_authored_collection_stage_discard(&complete);CHECK(!pair->terrain_publication->has_pending);
                    packet[288+96]^=1;
                    CHECK(scene_authored_collection_stage_prepare(lights->staged,packet,saved_bytes,&complete)==RF_FORMAT);
                    CHECK(!complete && !pair->terrain_publication->has_pending && pair->terrain_publication->active==active_bank && lights->staged->terrain==live_core);
                    packet[288+96]^=1;
                }
                memcpy(kept,packet,SCENE_CHECKPOINT_MAX);encoded_bytes=777;
                CHECK(scene_authored_collection_checkpoint_write(lights->staged,packet,saved_bytes-1,&encoded_bytes)==RF_RANGE);
                CHECK(encoded_bytes==777 && !memcmp(packet,kept,SCENE_CHECKPOINT_MAX));
                lights->staged->terrain_noise->random.value^=1;
                CHECK(scene_authored_collection_checkpoint_write(lights->staged,packet,SCENE_CHECKPOINT_MAX,&encoded_bytes)==RF_FORMAT);
                CHECK(encoded_bytes==777 && !memcmp(packet,kept,SCENE_CHECKPOINT_MAX));
                lights->staged->terrain_noise->random.value^=1;
                {
                    scene_authored_collection_stage *commit=NULL;rf_geomod_terrain *prior[2];rf_geomod_piece_registry *prior_pieces[2];
                    uint32_t active=pair->terrain_publication->active,face,image_before;
                    CHECK(!scene_terrain_sources_select(lights->staged,1));
                    for(i=0;i<2;i++){prior[i]=pair->terrain_sources[i].terrain;prior_pieces[i]=pair->terrain_sources[i].pieces;}
                    CHECK(!scene_authored_collection_stage_prepare(lights->staged,packet,saved_bytes,&commit));
                    for(face=0;face<commit->shared.publication.mesh.face_count;face++)if(commit->shared.origins[face].kind==RF_GEOMOD_PUBLICATION_CRATER)break;
                    CHECK(face<commit->shared.publication.mesh.face_count);
                    image_before=commit->shared.lighting->staged->terrain_bindings[face].image;
                    commit->shared.lighting->staged->terrain_bindings[face].image=UINT32_MAX;
                    CHECK(scene_authored_collection_stage_commit(commit,NULL)==RF_NOT_FOUND);
                    CHECK(pair->terrain_publication->active==active && pair->terrain_publication->has_pending);
                    for(i=0;i<2;i++)CHECK(prior[i]==pair->terrain_sources[i].terrain && prior_pieces[i]==pair->terrain_sources[i].pieces);
                    commit->shared.lighting->staged->terrain_bindings[face].image=image_before;
                    CHECK(!scene_authored_collection_stage_commit(commit,NULL));
                    CHECK(!pair->terrain_publication->has_pending && pair->terrain_publication->active!=active);
                    for(i=0;i<2;i++)CHECK(prior[i]!=pair->terrain_sources[i].terrain && prior_pieces[i]!=pair->terrain_sources[i].pieces && !commit->sources->cores[i] && !commit->sources->pieces[i]);
                    CHECK(lights->staged->terrain==pair->terrain_sources[1].terrain && lights->staged->detached_pieces==pair->terrain_sources[1].pieces);
                    CHECK(scene_authored_collection_stage_commit(commit,NULL)==RF_RANGE);
                    scene_authored_collection_stage_discard(&commit);
                    CHECK(!scene_authored_collection_checkpoint_write(lights->staged,kept,SCENE_CHECKPOINT_MAX,&encoded_bytes));
                    CHECK(encoded_bytes==saved_bytes && !memcmp(packet,kept,saved_bytes));
                    /* This fixture's outer shell shares the collection but had
                     * its own active aliases/overlay before the tested commit. */
                    pair->terrain_authored=lights->staged->terrain_authored;pair->terrain=lights->staged->terrain;
                    pair->detached_pieces=lights->staged->detached_pieces;pair->terrain_collision=lights->staged->terrain_collision;
                    puts("PASS paired atomic commit: binding rejection retains both owners, successful transfer preserves selection and exact save bytes");
                }
                printf("PASS paired RFDS3 writer: real histories/bodies, shared maps, extension validation and atomic failures (%u bytes)\n",saved_bytes);
            }
            scene_terrain_lighting_stage_discard(&lights);scene_terrain_publication_abort(pair);
            rf_geometry_collision_overlay_close(&pair->terrain_collision);scene_terrain_publication_close(&pair->terrain_publication);
        }
        scene_terrain_sources_close(pair);free(pair);free(packet);free(kept);
        printf("PASS real paired source snapshot: verified source identities, exact cut meshes/body bytes and atomic rejection (%u bytes)\n",bytes);
    }

    CHECK(!connected_capture(&s,&level,maps,&shape));
    CHECK(!beam_capture(&s,&level,maps,&shape));
    rf_geometry_collision_overlay_close(&s.terrain_collision);scene_terrain_publication_close(&s.terrain_publication);
    rf_geomod_piece_registry_close(&s.detached_pieces);free(campaign_surface_palette);campaign_surface_palette=NULL;
    rf_geomod_terrain_close(&s.terrain);scene_terrain_authored_close(&s.terrain_authored);
    free(s.terrain_noise);free(s.terrain_atlas_pixels);free(s.terrain_tile);free(s.terrain_bindings);free(s.terrain_colors);
    free(s.terrain_light_cache);free(s.terrain_tiles);free(s.terrain_draw);free(s.light_overlay_work);
    rf_lightmap_rgb_close(&s.light_rgb);rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);
    for(i=0;i<6;i++)rf_vpp_close(maps+i);rf_vpp_close(&archive);actor_follow_world=NULL;free(render.slots);
    printf("PASS installed authored three-digest private/live2cut, unused%u, dynamic exclusion, reset, atomic failures; scratch%u\n",unused,peak);return 0;
}
