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
    const float center[3][3]={{0,0,0},{5,0,0},{-1,0,0}},extent[3][3]={{1,12,12},{2,2,2},{2,2,2}};
    rf_geomod_mesh_view source={vertices,faces,24,6,0},view;rf_geomod_terrain *history=NULL;
    rf_geomod_terrain *replay=calloc(1,sizeof(*replay));geomod_face_lineage *tags=calloc(1,sizeof(*tags));
    geomod_step_support *support=calloc(1,sizeof(*support));uint32_t prefix;
    REQUIRE(replay && tags && support);box(lo,hi,0,vertices,faces);
    REQUIRE(!rf_geomod_terrain_open(&source,filters,&generated,0,4096,800,1048576,&history));
    for(prefix=0;prefix<3;prefix++)REQUIRE(!rf_geomod_terrain_cut_box(history,center[prefix],extent[prefix],7));
    REQUIRE(!rf_geomod_storage_open(&source,4096,800,1048576,&replay->mesh));
    memcpy(replay->work.cut_planes,history->work.cut_planes,sizeof(history->work.cut_planes));
    for(prefix=1;prefix<=3;prefix++) {
        REQUIRE(!prepare_chronological_step_clipped(replay->mesh,history->cuts,prefix,&replay->work,tags,support,0,&current_clip));
        REQUIRE(!rf_geomod_storage_commit(replay->mesh));REQUIRE(!rf_geomod_storage_view(replay->mesh,&view));
        if(prefix==1) {
            uint32_t words,*work,*labels,components,largest,old_faces[800],i;
            uint32_t bank=replay->mesh->current,other=bank^1;rf_geomod_mesh_view retained,piece;
            REQUIRE(!rf_geomod_component_work_size(&view,&words));work=malloc(words*4);labels=malloc(view.face_count*4);REQUIRE(work && labels);
            REQUIRE(!rf_geomod_mesh_components(&view,NULL,work,words,labels,&components,&largest));REQUIRE(components==2 && largest==0);
            REQUIRE(!rf_geomod_component_extract(&view,labels,1,replay->mesh->vertices[other],4096,replay->mesh->faces[other],800,old_faces,&retained,&piece));
            /* Stable compaction permits forward remapping of edge/plane support
             * arrays before overwriting the private active mesh. */
            for(i=0;i<retained.face_count;i++) {
                const rf_geomod_face *old=view.faces+old_faces[i],*fresh=retained.faces+i;
                memmove(replay->work.compact_edges+fresh->first,replay->work.compact_edges+old->first,fresh->count*sizeof(uint16_t));
                replay->work.compact_planes[i]=replay->work.compact_planes[old_faces[i]];
            }
            memcpy(replay->mesh->vertices[bank],retained.vertices,retained.vertex_count*sizeof(*retained.vertices));
            memcpy(replay->mesh->faces[bank],retained.faces,retained.face_count*sizeof(*retained.faces));
            replay->mesh->nv[bank]=retained.vertex_count;replay->mesh->nf[bank]=retained.face_count;
            free(labels);free(work);REQUIRE(!rf_geomod_storage_view(replay->mesh,&view));
        }
        REQUIRE(lineage_closed(&view));REQUIRE(fabs(mesh_volume(&view)-(prefix==3?3568:3600))<0.0001);
        for(uint32_t i=0;i<view.vertex_count;i++)REQUIRE(view.vertices[i].position[0]<=-1);
        if(prefix==2)REQUIRE(view.face_count==6 && view.vertex_count==24);
        printf("PASS extracted replay prefix=%u volume=%.9g faces=%u\n",prefix,mesh_volume(&view),view.face_count);
    }
    rf_geomod_storage_close(&replay->mesh);free(replay);free(tags);free(support);rf_geomod_terrain_close(&history);
    return 0;
}
