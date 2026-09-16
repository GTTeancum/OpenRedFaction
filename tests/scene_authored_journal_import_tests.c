/* Process-local CPU ownership test; actual installed post + RFCT are inputs.
 * No renderer, game window, native capture or GPU upload is involved. */
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_authored_journal_import.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"lighting stage line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct stage_snapshot {
    scene_stream stream;scene_lighting_stage_telemetry telemetry;
    void *buffers[9],*copies[9];size_t sizes[9];
} stage_snapshot;
static int snapshot_open(scene_stream *s,stage_snapshot *b)
{
    uint32_t i;b->stream=*s;scene_lighting_stage_telemetry_get(&b->telemetry);
#define SNAP(n,p,bytes) b->buffers[n]=(p);b->sizes[n]=(bytes)
    SNAP(0,s->terrain_noise,sizeof(*s->terrain_noise));
    SNAP(1,s->terrain_atlas_pixels,512*512*2);
    SNAP(2,s->terrain_tile,64*64*2);
    SNAP(3,s->terrain_bindings,SCENE_TERRAIN_FACES*sizeof(*s->terrain_bindings));
    SNAP(4,s->terrain_colors,SCENE_TERRAIN_DRAW_VERTICES*sizeof(*s->terrain_colors));
    SNAP(5,s->terrain_light_cache,sizeof(*s->terrain_light_cache));
    SNAP(6,s->terrain_tiles,SCENE_TERRAIN_FACES*sizeof(*s->terrain_tiles));
    SNAP(7,s->light_overlay_work,1100*(sizeof(uint32_t)+sizeof(rf_vfx_light_source)));
    SNAP(8,s->terrain_draw,sizeof(*s->terrain_draw));
#undef SNAP
    for(i=0;i<9;i++){b->copies[i]=malloc(b->sizes[i]);CHECK(b->copies[i]);memcpy(b->copies[i],b->buffers[i],b->sizes[i]);}return 0;
}
static int snapshot_equal(scene_stream *s,const stage_snapshot *b)
{
    scene_lighting_stage_telemetry t;uint32_t i;
    CHECK(!memcmp(s,&b->stream,sizeof(*s)));scene_lighting_stage_telemetry_get(&t);
    CHECK(!memcmp(&t,&b->telemetry,sizeof(t)));
    for(i=0;i<9;i++)CHECK(!memcmp(b->copies[i],b->buffers[i],b->sizes[i]));return 0;
}
static void snapshot_close(stage_snapshot *b){uint32_t i;for(i=0;i<9;i++)free(b->copies[i]);}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geomod_authored_post *asset=NULL;
    rf_geomod_authored_post_view authored;rf_geomod_terrain *core=NULL;rf_geomod_terrain_view candidate;
    rf_geomod_template shape;rf_collision_face_filter generated;scene_stream s={0};stage_snapshot snapshot={0};
    scene_terrain_lighting_stage *stage=NULL;rf_preview_surface_lightmap bindings[SCENE_TERRAIN_FACES];
    uint32_t i,j,source_count=0,mapped_count=0,generated_count=0,stack_hash;int status;
    const float center[3]={-4.75f,-.9f,2.5f},basis[9]={1,0,0,0,1,0,0,0,1};
    if(argc!=3){fprintf(stderr,"usage: scene_terrain_lighting_stage_tests levelsm.vpp holey01.rfct\n");return 2;}
    CHECK(!rf_vpp_open(&archive,argv[1]));CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geomod_authored_post_open(&level,&geometry,2*1024*1024,&asset));
    CHECK(!rf_geomod_authored_post_get(asset,&authored));generated=authored.source_filters[0];
    CHECK(!rf_geomod_terrain_open(&authored.source,authored.source_filters,&generated,0,4096,768,1024*1024,&core));
    CHECK(!rf_geomod_terrain_set_mapping(core,256,128));CHECK(!rf_geomod_template_load(argv[2],&shape));
    CHECK(!rf_geomod_terrain_cut_template(core,&shape,center,basis,1.05000007f,0));
    CHECK(!rf_geomod_terrain_get(core,&candidate));CHECK(candidate.cuts==1);
    memset(bindings,0,sizeof(bindings));
    for(i=0;i<candidate.mesh.face_count;i++){
        const rf_geomod_face *face=candidate.mesh.faces+i;bindings[i].image=UINT32_MAX;
        if(face->source_face==UINT32_MAX){generated_count++;continue;}
        source_count++;
        for(j=0;j<authored.windows.face_count;j++)if(authored.windows.faces[j].source_face==face->source_face){
            rf_geometry_face f;rf_lightmap_mapping mapping;
            CHECK(!rf_geometry_get_face(&geometry,authored.window_origins[j].reference,&f));
            CHECK(!rf_geometry_get_lightmap_mapping(&geometry,f.lightmap_mapping,2,&mapping));
            CHECK(!rf_geometry_lightmap_projection(&geometry,f.lightmap_mapping,&bindings[i].projection));
            bindings[i].image=mapping.image;mapped_count++;break;
        }
    }
    CHECK(source_count && mapped_count && generated_count);
#define ALLOC(field,n) do{s.field=calloc((n),sizeof(*s.field));CHECK(s.field);}while(0)
    ALLOC(terrain_noise,1);ALLOC(terrain_atlas_pixels,512*512*2);ALLOC(terrain_tile,64*64*2);
    ALLOC(terrain_bindings,SCENE_TERRAIN_FACES);ALLOC(terrain_colors,SCENE_TERRAIN_DRAW_VERTICES);
    ALLOC(terrain_light_cache,1);ALLOC(terrain_tiles,SCENE_TERRAIN_FACES);ALLOC(terrain_draw,1);
#undef ALLOC
    s.light_overlay_work=calloc(1100,sizeof(uint32_t)+sizeof(rf_vfx_light_source));CHECK(s.light_overlay_work);
    s.terrain_atlas_registered=1;s.terrain_atlas_index=2;s.terrain=core;
    memset(s.terrain_atlas_pixels,0x5a,512*512*2);memset(s.terrain_colors,0x3c,SCENE_TERRAIN_DRAW_VERTICES*sizeof(*s.terrain_colors));
    memset(rf_scene_terrain_noise,0x17,sizeof(rf_scene_terrain_noise));
    {
        rf_authored_checkpoint_layout layout;scene_terrain_lighting_stage *restored=NULL;
        rf_geomod_publication_origin origins[SCENE_TERRAIN_FACES];rf_image images[3]={{0}};
        unsigned char *data;uint32_t serial=candidate.mesh.generation,first_generated=UINT32_MAX,source_face=UINT32_MAX;
        s.light_rgb.images=images;s.light_rgb.count=3;
        CHECK(!snapshot_open(&s,&snapshot));
        CHECK(!scene_terrain_lighting_stage_prepare(&s,&candidate,bindings,0,&stage));
        /* Append a valid retained but unreferenced historical map, with its own
         * packing/projection and seed. It must survive import/bake unchanged. */
        {
            scene_terrain_noise_owner *o=stage->staged->terrain_noise;
            scene_terrain_noise_map *m=o->maps+o->count;uint32_t axis;rf_random_state chain=o->random;
            CHECK(o->count && o->count<1024);*m=o->maps[0];m->base_seed=chain.value;
            if(o->x+m->width>512){o->y+=o->row;o->x=o->row=0;}
            m->x=o->x;m->y=o->y;CHECK(m->y+m->height<=512);
            for(axis=0;axis<2;axis++){
                uint32_t a=m->binding.projection.axes[axis];double scale=((axis?m->height:m->width)-2)/(double)(m->maximum[a]-m->minimum[a]);
                m->binding.projection.offset[axis]=(float)(((axis?m->y:m->x)+1-m->minimum[a]*scale)/512);
            }
            CHECK(!scene_checkpoint_base(stage->staged,m,&chain));o->random=chain;o->x+=m->width;
            if(m->height>o->row)o->row=m->height;o->count++;o->bake=o->count;
        }
        CHECK(!rf_authored_checkpoint_layout_size(28,0,stage->staged->terrain_noise->count,candidate.mesh.face_count,&layout));
        data=calloc(1,layout.bytes);CHECK(data);memcpy(data,"RFDS",4);checkpoint_put(data+4,2);checkpoint_put(data+8,layout.bytes);
        memcpy(data+16,"ctf06.rfl",10);checkpoint_put(data+276,416);checkpoint_put(data+280,128);checkpoint_put(data+284,2);
        checkpoint_put(data+252,28);checkpoint_put(data+248,layout.maps);checkpoint_put(data+272,layout.faces);
        checkpoint_put(data+256,stage->staged->terrain_noise->random.value);checkpoint_put(data+260,stage->staged->terrain_noise->x);
        checkpoint_put(data+264,stage->staged->terrain_noise->y);checkpoint_put(data+268,stage->staged->terrain_noise->row);
        for(i=0;i<layout.maps;i++) {
            const scene_terrain_noise_map *m=stage->staged->terrain_noise->maps+i;unsigned char *p=data+layout.map_offset+i*88;
            for(j=0;j<4;j++)checkpoint_put_float(p+j*4,m->plane[j]);
            for(j=0;j<3;j++){checkpoint_put_float(p+16+j*4,m->minimum[j]);checkpoint_put_float(p+28+j*4,m->maximum[j]);}
            checkpoint_put(p+44,m->x);checkpoint_put(p+48,m->y);checkpoint_put(p+52,m->width);checkpoint_put(p+56,m->height);checkpoint_put(p+60,m->base_seed);
            for(j=0;j<2;j++){checkpoint_put(p+64+j*4,m->binding.projection.axes[j]);checkpoint_put_float(p+72+j*4,m->binding.projection.scale[j]);checkpoint_put_float(p+80+j*4,m->binding.projection.offset[j]);}
        }
        for(i=0;i<layout.faces;i++) {
            const rf_geomod_face *f=candidate.mesh.faces+i;uint32_t map=65535;
            origins[i]=(rf_geomod_publication_origin){f->source_face==UINT32_MAX?1:0,94,f->source_face,f->source_face};
            if(f->source_face==UINT32_MAX){
                if(first_generated==UINT32_MAX)first_generated=i;
                for(j=0;j<layout.maps;j++)if(!memcmp(&stage->staged->terrain_noise->maps[j].binding,&stage->staged->terrain_bindings[i],sizeof(bindings[i])))break;
                CHECK(j<layout.maps-1);map=j;
            }else source_face=i;
            data[layout.face_offset+i*2]=(unsigned char)map;data[layout.face_offset+i*2+1]=(unsigned char)(map>>8);
        }
        CHECK(first_generated!=UINT32_MAX && source_face!=UINT32_MAX);
        for(i=0;i<7;i++) {
            unsigned char saved[88];uint32_t offset=layout.map_offset;int expected=RF_FORMAT;
            if(i==1)offset=layout.map_offset+(layout.maps-1)*88; /* unused map still validated */
            if(i==2)offset=256;if(i==3)offset=layout.face_offset+source_face*2;
            if(i==4)offset=layout.face_offset+first_generated*2;
            memcpy(saved,data+offset,i==3 || i==4?2:i==2?4:88);
            if(i==0)data[offset+60]^=1;
            if(i==1)checkpoint_put_float(data+offset,0.f),checkpoint_put_float(data+offset+4,0.f),checkpoint_put_float(data+offset+8,0.f);
            if(i==2)data[offset]^=1;if(i==3)data[offset]=data[offset+1]=0;
            if(i==4)data[offset]=data[offset+1]=255;
            if(i==5)expected=RF_OK;
            if(i==6){expected=RF_OK;s.terrain_atlas_registered=0;s.light_rgb.count=2;snapshot_close(&snapshot);CHECK(!snapshot_open(&s,&snapshot));}
            CHECK(!scene_terrain_lighting_stage_clone(&s,&candidate,&restored));
            status=scene_authored_journal_import(&s,restored,&layout,data,origins,bindings,serial,candidate.cuts);CHECK(status==expected);
            CHECK(!snapshot_equal(&s,&snapshot));
            if(!status) {
                CHECK(restored->staged->terrain_noise->count==layout.maps);
                CHECK(!memcmp(restored->staged->terrain_noise,stage->staged->terrain_noise,sizeof(*s.terrain_noise)));
                if(i==6)CHECK(restored->staged->terrain_checkpoint_loaded==2 && !restored->staged->terrain_atlas_registered && restored->staged->terrain_atlas_index==2);
                CHECK(!scene_terrain_lighting_stage_bake(restored,bindings,0));
                CHECK(!scene_terrain_lighting_stage_draw(restored));CHECK(!snapshot_equal(&s,&snapshot));
            }
            scene_terrain_lighting_stage_discard(&restored);
            memcpy(data+offset,saved,i==3 || i==4?2:i==2?4:88);
        }
        snapshot_close(&snapshot);scene_terrain_lighting_stage_discard(&stage);free(data);
    }
    free(s.terrain_noise);free(s.terrain_atlas_pixels);free(s.terrain_tile);free(s.terrain_bindings);free(s.terrain_colors);
    free(s.terrain_light_cache);free(s.terrain_tiles);free(s.terrain_draw);free(s.light_overlay_work);
    rf_geomod_terrain_close(&core);rf_geomod_authored_post_close(&asset);rf_geometry_close(&geometry);rf_vpp_close(&archive);
    puts("PASS private journal import actual cut, unused map, malformed rejection, bake and atlas reservation");return 0;
}
