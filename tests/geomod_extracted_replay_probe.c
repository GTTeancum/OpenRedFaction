/* Private replay composition; does not enable extraction in the live scene. */
#define RF_GEOMOD_TEST_CURRENT_SOLID 1
#define main legacy_current_solid_main
#include "geomod_chronological_solid_tests.c"
#undef main
#include "rf/geomod_piece_bank.h"
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"extracted replay line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct capture_context {rf_geomod_piece_bank *bank;uint32_t prefix;} capture_context;
static int capture_piece(const rf_geomod_mesh_view *mesh,const uint32_t *old_faces,
    const rf_collision_face_filter *filters,uint32_t source_count,uint32_t ordinal,void *opaque)
{
    capture_context *context=opaque;rf_geomod_owned_piece piece;uint32_t i,j;
    int status=rf_geomod_piece_bank_append(context->bank,mesh,old_faces,filters,source_count,(context->prefix<<16)|ordinal);
    if(status)return status;
    REQUIRE(!rf_geomod_piece_bank_get(context->bank,rf_geomod_piece_bank_count(context->bank)-1,&piece));
    REQUIRE(!memcmp(piece.mesh.faces,mesh->faces,mesh->face_count*sizeof(*mesh->faces)));
    for(i=0;i<mesh->vertex_count;i++) {
        REQUIRE(!memcmp(piece.mesh.vertices[i].uv,mesh->vertices[i].uv,8));
        for(j=0;j<3;j++)REQUIRE(fabs((double)piece.mesh.vertices[i].position[j]+piece.placement.origin[j]-mesh->vertices[i].position[j])<0.00001);
    }
    for(i=0;i<mesh->face_count;i++)REQUIRE(piece.old_faces[i]==old_faces[i] && !memcmp(piece.filters+i,filters+old_faces[i],sizeof(*filters)));
    return RF_OK;
}
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
    rf_geomod_piece_bank *owned[2]={NULL,NULL},*tiny=NULL;
    REQUIRE(!rf_geomod_piece_bank_open(1,6,1,4096,&tiny));
    REQUIRE(replay && tags && support);box(lo,hi,0,vertices,faces);
    REQUIRE(!rf_geomod_terrain_open(&source,filters,&generated,0,4096,800,1048576,&history));
    for(prefix=0;prefix<2;prefix++)REQUIRE(!rf_geomod_terrain_cut_box(history,center[prefix],extent[prefix],7));
    REQUIRE(!rf_geomod_terrain_history_size(history,&bytes) && bytes<=sizeof(encoded));
    REQUIRE(!rf_geomod_terrain_history_encode(history,encoded,bytes));
    for(prefix=2;prefix<4;prefix++)REQUIRE(!rf_geomod_terrain_cut_box(history,center[prefix],extent[prefix],7));
    for(round=0;round<2;round++) {
    REQUIRE(!rf_geomod_piece_bank_open(128,32,4,16384,owned+round));
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
            if(prefix==1 || prefix==4) {
                rf_geomod_vertex before_vertices[256];rf_geomod_face before_faces[64];rf_geomod_mesh_view after;
                capture_context rejected={tiny,prefix};removed=999;
                REQUIRE(view.vertex_count<=256 && view.face_count<=64);
                memcpy(before_vertices,view.vertices,view.vertex_count*sizeof(*view.vertices));
                memcpy(before_faces,view.faces,view.face_count*sizeof(*view.faces));
                REQUIRE(extract_replay_components(replay->mesh,&replay->work,&current_clip,scratch,words,labels,old_faces,&removed,capture_piece,&rejected)==RF_RANGE);
                REQUIRE(removed==999 && !rf_geomod_piece_bank_count(tiny));
                REQUIRE(!rf_geomod_storage_view(replay->mesh,&after));
                REQUIRE(view.vertices==after.vertices && view.faces==after.faces && view.vertex_count==after.vertex_count && view.face_count==after.face_count);
                REQUIRE(!memcmp(before_vertices,after.vertices,view.vertex_count*sizeof(*view.vertices)) && !memcmp(before_faces,after.faces,view.face_count*sizeof(*view.faces)));
            }
            {
                capture_context accepted={owned[round],prefix};
                REQUIRE(!extract_replay_components(replay->mesh,&replay->work,&current_clip,scratch,words,labels,old_faces,&removed,capture_piece,&accepted));
            }
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
    REQUIRE(rf_geomod_piece_bank_count(owned[round])==2);
    }
    for(prefix=0;prefix<2;prefix++) {
        rf_geomod_owned_piece a,b;
        REQUIRE(!rf_geomod_piece_bank_get(owned[0],prefix,&a) && !rf_geomod_piece_bank_get(owned[1],prefix,&b));
        REQUIRE(a.id==b.id && !memcmp(&a.placement,&b.placement,sizeof(a.placement)));
        REQUIRE(a.mesh.vertex_count==b.mesh.vertex_count && a.mesh.face_count==b.mesh.face_count);
        REQUIRE(!memcmp(a.mesh.vertices,b.mesh.vertices,a.mesh.vertex_count*sizeof(*a.mesh.vertices)));
        REQUIRE(!memcmp(a.mesh.faces,b.mesh.faces,a.mesh.face_count*sizeof(*a.mesh.faces)));
        REQUIRE(!memcmp(a.old_faces,b.old_faces,a.mesh.face_count*4));
        REQUIRE(fabs(mesh_volume(&a.mesh)-(prefix?1200:3600))<0.0001);
    }
    printf("PASS owned extracted pieces survive replay scratch reuse; bank_bytes=%u\n",rf_geomod_piece_bank_bytes(owned[0]));
    {
        rf_geomod_piece_bank *budgeted=NULL;uint32_t bytes=rf_geomod_piece_bank_bytes(owned[0]);
        rf_geomod_owned_piece piece;uint32_t before=rf_geomod_piece_bank_count(owned[0]);
        REQUIRE(rf_geomod_piece_bank_open(128,32,4,bytes-1,&budgeted)==RF_RANGE && !budgeted);
        REQUIRE(!rf_geomod_piece_bank_open(128,32,4,bytes,&budgeted));
        REQUIRE(rf_geomod_piece_bank_bytes(budgeted)==bytes);rf_geomod_piece_bank_close(&budgeted);
        REQUIRE(!rf_geomod_piece_bank_get(owned[0],0,&piece));
        REQUIRE(rf_geomod_piece_bank_append(owned[0],&piece.mesh,piece.old_faces,piece.filters,piece.mesh.face_count,piece.id)==RF_FORMAT);
        REQUIRE(rf_geomod_piece_bank_count(owned[0])==before);
    }
    rf_geomod_piece_bank_close(owned);rf_geomod_piece_bank_close(owned+1);rf_geomod_piece_bank_close(&tiny);
    free(replay);free(tags);free(support);rf_geomod_terrain_close(&history);
    return 0;
}
