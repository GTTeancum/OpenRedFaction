/* CPU-only actual authored room tree, private empty lighting restore. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"empty stage line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct empty_snapshot {
    scene_stream stream;scene_lighting_stage_telemetry telemetry;
    uint32_t hashes[9],geomod[8],debris[8];
} empty_snapshot;
static void buffers(scene_stream *s,void **p,size_t *n)
{
#define ROW(i,field,count) p[i]=s->field;n[i]=(count)*sizeof(*s->field)
    ROW(0,terrain_noise,1);ROW(1,terrain_atlas_pixels,512*512*2);ROW(2,terrain_tile,64*64*2);
    ROW(3,terrain_bindings,SCENE_TERRAIN_FACES);ROW(4,terrain_colors,SCENE_TERRAIN_DRAW_VERTICES);
    ROW(5,terrain_light_cache,1);ROW(6,terrain_tiles,SCENE_TERRAIN_FACES);
    p[7]=s->light_overlay_work;n[7]=1100*(sizeof(uint32_t)+sizeof(rf_vfx_light_source));ROW(8,terrain_draw,1);
#undef ROW
}
static void snapshot(scene_stream *s,empty_snapshot *b)
{
    void *p[9];size_t n[9];uint32_t i;b->stream=*s;buffers(s,p,n);
    for(i=0;i<9;i++)b->hashes[i]=npc_hash_bytes(2166136261u,p[i],n[i]);
    scene_lighting_stage_telemetry_get(&b->telemetry);memcpy(b->geomod,rf_scene_geomod,sizeof(b->geomod));
    memcpy(b->debris,rf_scene_debris,sizeof(b->debris));
}
static int unchanged(scene_stream *s,const empty_snapshot *b)
{
    empty_snapshot now;snapshot(s,&now);CHECK(!memcmp(s,&b->stream,sizeof(*s)));
    CHECK(!memcmp(now.hashes,b->hashes,sizeof(now.hashes)));
    CHECK(!memcmp(&now.telemetry,&b->telemetry,sizeof(now.telemetry)));
    CHECK(!memcmp(now.geomod,b->geomod,sizeof(now.geomod)) && !memcmp(now.debris,b->debris,sizeof(now.debris)));return 0;
}
static int all_zero(const void *p,size_t n)
{const unsigned char *v=p;size_t i;for(i=0;i<n;i++)if(v[i])return 0;return 1;}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geometry_collision_room room={0};
    scene_stream s={0};rf_geomod_terrain_view empty={0},invalid;
    scene_terrain_lighting_stage *stage=NULL,*sentinel=(scene_terrain_lighting_stage *)(uintptr_t)1;
    empty_snapshot before;void *p[9];size_t n[9];uint32_t i,j,k,tree_hash;
    const uint32_t cases[5][3]={{0,0,0},{0,0,1},{18,0,0},{18,0,1},{18,18,1}};
    CHECK(argc==2);CHECK(!rf_vpp_open(&archive,argv[1]));CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));CHECK(!rf_geometry_collision_room_open(&geometry,3,2*1024*1024,&room));
    CHECK(room.tree.face_count==790);empty.tree=&room.tree;
#define ALLOC(field,count) do{s.field=calloc((count),sizeof(*s.field));CHECK(s.field);}while(0)
    ALLOC(terrain_noise,1);ALLOC(terrain_atlas_pixels,512*512*2);ALLOC(terrain_tile,64*64*2);
    ALLOC(terrain_bindings,SCENE_TERRAIN_FACES);ALLOC(terrain_colors,SCENE_TERRAIN_DRAW_VERTICES);
    ALLOC(terrain_light_cache,1);ALLOC(terrain_tiles,SCENE_TERRAIN_FACES);ALLOC(terrain_draw,1);
#undef ALLOC
    s.light_overlay_work=calloc(1100,sizeof(uint32_t)+sizeof(rf_vfx_light_source));CHECK(s.light_overlay_work);
    s.terrain_history_count=17;s.terrain_atlas_registered=1;s.terrain_atlas_index=2;
    buffers(&s,p,n);tree_hash=npc_hash_bytes(2166136261u,room.tree.stack,room.tree.node_capacity*sizeof(uint32_t));
    for(k=0;k<5;k++) {
        for(i=0;i<9;i++)memset(p[i],0x5a,n[i]);
        s.terrain_atlas_pending=1;s.terrain_dirty[0]=40;s.terrain_dirty[1]=50;s.terrain_dirty[2]=60;s.terrain_dirty[3]=70;
        memset(rf_scene_terrain_noise,0x18,sizeof(rf_scene_terrain_noise));
        for(i=0;i<8;i++)rf_scene_terrain_atlas[i]=100+i;snapshot(&s,&before);
        CHECK(scene_terrain_lighting_stage_clone(&s,&empty,&sentinel)==RF_RANGE && sentinel==(void *)(uintptr_t)1);
        CHECK(scene_terrain_lighting_stage_empty(&s,&empty,18,18,0,&sentinel)==RF_FORMAT && sentinel==(void *)(uintptr_t)1);
        CHECK(scene_terrain_lighting_stage_empty(&s,&empty,18,17,1,&sentinel)==RF_FORMAT);
        CHECK(scene_terrain_lighting_stage_empty(&s,&empty,18,0,2,&sentinel)==RF_FORMAT);
        CHECK(scene_terrain_lighting_stage_empty(&s,&empty,UINT32_MAX,0,0,&sentinel)==RF_FORMAT);
        invalid=empty;invalid.cuts=1;CHECK(scene_terrain_lighting_stage_empty(&s,&invalid,18,0,0,&sentinel)==RF_RANGE);
        invalid=empty;invalid.mesh.face_count=1;CHECK(scene_terrain_lighting_stage_empty(&s,&invalid,18,0,0,&sentinel)==RF_RANGE);
        CHECK(!unchanged(&s,&before));
        CHECK(!scene_terrain_lighting_stage_empty(&s,&empty,cases[k][0],cases[k][1],cases[k][2],&stage));
        CHECK(stage->bake_ready && stage->draw_ready && stage->peak_bytes<=SCENE_LIGHTING_STAGE_BUDGET);
        CHECK(stage->candidate.mesh.generation==cases[k][0]);CHECK(!unchanged(&s,&before));
        /* Compare the actual ordinary authored reset routine on another private
         * clone. Only saved journal continuation differs intentionally. */
        {
            scene_terrain_lighting_stage *control=NULL;scene_lighting_stage_telemetry reset_telemetry;
            CHECK(!scene_terrain_lighting_stage_clone_mode(&s,&empty,1,&control));
            control->clone.terrain_publication_serial=cases[k][0];
            scene_terrain_authored_reset_cleanup(&control->clone);
            scene_lighting_stage_telemetry_get(&reset_telemetry);
            scene_lighting_stage_telemetry_set(&before.telemetry);
            control->clone.terrain_noise->generation=cases[k][1];
            control->clone.terrain_noise->random.value=cases[k][2];reset_telemetry.noise[7]=cases[k][1];
            CHECK(!memcmp(&reset_telemetry,&stage->telemetry,sizeof(reset_telemetry)));
            for(i=0;i<7;i++)CHECK(!memcmp(control->buffers[i].copy,stage->buffers[i].copy,control->buffers[i].bytes));
            CHECK(!memcmp(control->clone.terrain_draw->vertices,stage->clone.terrain_draw->vertices,sizeof(s.terrain_draw->vertices)));
            CHECK(!memcmp(control->clone.terrain_draw->faces,stage->clone.terrain_draw->faces,sizeof(s.terrain_draw->faces)));
            CHECK(control->clone.terrain_draw->view.generation==stage->clone.terrain_draw->view.generation);
            CHECK(!memcmp(control->clone.terrain_dirty,stage->clone.terrain_dirty,sizeof(s.terrain_dirty)));
            scene_terrain_lighting_stage_discard(&control);CHECK(!unchanged(&s,&before));
        }
        /* Disposal of a completely prepared empty state is still rollback. */
        scene_terrain_lighting_stage_discard(&stage);CHECK(!stage && !unchanged(&s,&before));
        CHECK(!scene_terrain_lighting_stage_empty(&s,&empty,cases[k][0],cases[k][1],cases[k][2],&stage));
        scene_terrain_lighting_stage_commit(stage);CHECK(stage->committed);
        CHECK(s.terrain_noise->generation==cases[k][1] && s.terrain_noise->random.value==cases[k][2]);
        CHECK(!s.terrain_noise->count && !s.terrain_noise->cuts && !s.terrain_noise->bake && !s.terrain_noise->sample);
        CHECK(!s.terrain_noise->x && !s.terrain_noise->y && !s.terrain_noise->row);
        CHECK(all_zero(s.terrain_noise->maps,sizeof(s.terrain_noise->maps)));
        CHECK(all_zero(p[1],n[1]) && all_zero(p[2],n[2]) && all_zero(p[5],n[5]));
        for(i=0;i<SCENE_TERRAIN_DRAW_VERTICES;i++)for(j=0;j<3;j++)CHECK(s.terrain_colors[i][j]==1);
        for(i=0;i<4;i++)CHECK(rf_scene_terrain_atlas[i]==before.telemetry.atlas[i]);
        CHECK(rf_scene_terrain_atlas[4]==cases[k][0] && rf_scene_terrain_atlas[7]==before.telemetry.atlas[7]);
        CHECK(rf_scene_terrain_noise[0]==1 && rf_scene_terrain_noise[6]==sizeof(*s.terrain_noise));
        CHECK(all_zero(p[6],n[6]) && all_zero(p[7],n[7]));
        for(i=0;i<SCENE_TERRAIN_FACES;i++)CHECK(s.terrain_bindings[i].image==UINT32_MAX);
        CHECK(s.terrain_draw->view.vertices==s.terrain_draw->vertices && s.terrain_draw->view.faces==s.terrain_draw->faces);
        CHECK(!s.terrain_draw->view.face_count && !s.terrain_draw->view.vertex_count && s.terrain_draw->view.generation==cases[k][0]);
        CHECK(s.terrain_atlas_pending && !s.terrain_dirty[0] && !s.terrain_dirty[1] && s.terrain_dirty[2]==512 && s.terrain_dirty[3]==512);
        CHECK(s.terrain_atlas_registered==1 && s.terrain_atlas_index==2 && s.terrain_history_count==17);
        CHECK(!memcmp(before.geomod,rf_scene_geomod,sizeof(before.geomod)) && !memcmp(before.debris,rf_scene_debris,sizeof(before.debris)));
        CHECK(tree_hash==npc_hash_bytes(2166136261u,room.tree.stack,room.tree.node_capacity*sizeof(uint32_t)));
        scene_terrain_lighting_stage_discard(&stage);
    }
    for(i=0;i<9;i++)free(p[i]);rf_geometry_collision_room_close(&room);rf_geometry_close(&geometry);rf_vpp_close(&archive);
    puts("PASS private empty lighting reset: discard, guards, lazy RNG0/1, initialized RNG1, serial and commit");return 0;
}
