/* Private replay composition; does not enable extraction in the live scene. */
#define RF_GEOMOD_TEST_CURRENT_SOLID 1
#define main legacy_current_solid_main
#include "geomod_chronological_solid_tests.c"
#undef main
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"extracted replay line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static double mesh_volume(const rf_geomod_mesh_view *mesh)
{
    double volume=0;uint32_t f,j;
    for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;const float *a=mesh->vertices[face->first].position;
        for(j=1;j+1<face->count;j++) {
            const float *b=mesh->vertices[face->first+j].position,*c=mesh->vertices[face->first+j+1].position;
            volume+=((double)a[0]*(b[1]*c[2]-b[2]*c[1])+(double)a[1]*(b[2]*c[0]-b[0]*c[2])+(double)a[2]*(b[0]*c[1]-b[1]*c[0]))/6;
        }
    }
    return volume;
}
int main(void)
{
    rf_geomod_vertex vertices[24];rf_geomod_face faces[6];rf_collision_face_filter filters[6]={{0}},generated={0};
    const float lo[3]={-10,-10,-10},hi[3]={10,10,10};
    const float center[4][3]={{0,0,0},{5,0,0},{-1,0,0},{-6,0,0}},extent[4][3]={{1,12,12},{2,2,2},{2,2,2},{1,12,12}};
    rf_geomod_mesh_view source={vertices,faces,24,6,0},view;rf_geomod_terrain *history=NULL;
    rf_geomod_terrain *replay=calloc(1,sizeof(*replay));geomod_face_lineage *tags=calloc(1,sizeof(*tags));
    geomod_step_support *support=calloc(1,sizeof(*support));uint32_t prefix,round,bytes;
    unsigned char encoded[RF_GEOMOD_HISTORY_MAX_BYTES];
    static rf_geomod_vertex saved_vertices[4][256];static rf_geomod_face saved_faces[4][64];
    uint32_t saved_nv[4],saved_nf[4];
    REQUIRE(replay && tags && support);box(lo,hi,0,vertices,faces);
    REQUIRE(!rf_geomod_terrain_open(&source,filters,&generated,0,4096,800,1048576,&history));
    for(prefix=0;prefix<2;prefix++)REQUIRE(!rf_geomod_terrain_cut_box(history,center[prefix],extent[prefix],7));
    REQUIRE(!rf_geomod_terrain_history_size(history,&bytes) && bytes<=sizeof(encoded));
    REQUIRE(!rf_geomod_terrain_history_encode(history,encoded,bytes));
    for(prefix=2;prefix<4;prefix++)REQUIRE(!rf_geomod_terrain_cut_box(history,center[prefix],extent[prefix],7));
    for(round=0;round<2;round++) {
    if(round) {
        rf_geomod_terrain_close(&history);
        REQUIRE(!rf_geomod_terrain_open(&source,filters,&generated,0,4096,800,1048576,&history));
        REQUIRE(!rf_geomod_terrain_history_decode(history,encoded,bytes));
        for(prefix=2;prefix<4;prefix++)REQUIRE(!rf_geomod_terrain_cut_box(history,center[prefix],extent[prefix],7));
    }
    memset(&replay->work,0,sizeof(replay->work));memset(tags,0,sizeof(*tags));memset(support,0,sizeof(*support));
    REQUIRE(!rf_geomod_storage_open(&source,4096,800,1048576,&replay->mesh));
    memcpy(replay->work.cut_planes,history->work.cut_planes,sizeof(history->work.cut_planes));
    for(prefix=1;prefix<=4;prefix++) {
        REQUIRE(!prepare_chronological_step_clipped(replay->mesh,history->cuts,prefix,&replay->work,tags,support,0,&current_clip));
        REQUIRE(!rf_geomod_storage_commit(replay->mesh));REQUIRE(!rf_geomod_storage_view(replay->mesh,&view));
        {
            uint32_t words,*scratch,*labels,*old_faces,removed;
            REQUIRE(!rf_geomod_component_work_size(&view,&words));
            scratch=malloc(words*4);labels=malloc(view.face_count*4);old_faces=malloc(view.face_count*4);
            REQUIRE(scratch && labels && old_faces);
            memset(clip_filters,0,sizeof(clip_filters));
            REQUIRE(!extract_replay_components(replay->mesh,&replay->work,&current_clip,scratch,words,labels,old_faces,&removed));
            REQUIRE(removed==((prefix==1 || prefix==4)?1u:0u));
            free(old_faces);free(labels);free(scratch);REQUIRE(!rf_geomod_storage_view(replay->mesh,&view));
        }
        REQUIRE(lineage_closed(&view));REQUIRE(fabs(mesh_volume(&view)-(prefix==4?1568:prefix==3?3568:3600))<0.0001);
        for(uint32_t i=0;i<view.vertex_count;i++)REQUIRE(view.vertices[i].position[0]<=-1);
        if(prefix==4)for(uint32_t i=0;i<view.vertex_count;i++)REQUIRE(view.vertices[i].position[0]>=-5);
        if(prefix==2)REQUIRE(view.face_count==6 && view.vertex_count==24);
        REQUIRE(view.vertex_count<=256 && view.face_count<=64);
        if(!round) {
            saved_nv[prefix-1]=view.vertex_count;saved_nf[prefix-1]=view.face_count;
            memcpy(saved_vertices[prefix-1],view.vertices,view.vertex_count*sizeof(*view.vertices));
            memcpy(saved_faces[prefix-1],view.faces,view.face_count*sizeof(*view.faces));
        } else {
            REQUIRE(saved_nv[prefix-1]==view.vertex_count && saved_nf[prefix-1]==view.face_count);
            REQUIRE(!memcmp(saved_vertices[prefix-1],view.vertices,view.vertex_count*sizeof(*view.vertices)));
            REQUIRE(!memcmp(saved_faces[prefix-1],view.faces,view.face_count*sizeof(*view.faces)));
        }
        printf("PASS extracted replay reload=%u prefix=%u volume=%.9g faces=%u\n",round,prefix,mesh_volume(&view),view.face_count);
    }
    rf_geomod_storage_close(&replay->mesh);
    }
    free(replay);free(tags);free(support);rf_geomod_terrain_close(&history);
    return 0;
}
