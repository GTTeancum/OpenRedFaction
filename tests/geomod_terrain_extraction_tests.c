#include "rf/geomod_piece_bank.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct staging {rf_geomod_piece_batch *batch[8];rf_random_state random;uint32_t count,reject;} staging;
static void clear(staging *s)
{for(uint32_t i=0;i<8;i++)rf_geomod_piece_batch_close(s->batch+i);memset(s,0,sizeof(*s));}
static int emit(const rf_geomod_mesh_view *mesh,const uint32_t *map,const rf_collision_face_filter *filters,
    uint32_t source_count,uint32_t prefix,uint32_t ordinal,void *opaque)
{
    staging *s=opaque;rf_collision_face_filter mapped[32],generated={0};int status;
    (void)ordinal;if(s->reject && prefix==s->reject)return RF_IO;
    if(mesh->face_count>32 || s->count==8)return RF_RANGE;
    for(uint32_t i=0;i<mesh->face_count;i++){if(map[i]>=source_count)return RF_FORMAT;mapped[i]=filters[map[i]];}
    status=rf_geomod_piece_batch_open(mesh,mapped,&generated,7,2.5f,.5f,.25f,&s->random,2097152,s->batch+s->count);
    if(!status)s->count++;return status;
}
static double volume(const rf_geomod_mesh_view *m)
{
    double sum=0;
    for(uint32_t f=0;f<m->face_count;f++) {
        const rf_geomod_face *face=m->faces+f;const float *a=m->vertices[face->first].position;
        for(uint32_t i=1;i+1<face->count;i++) {
            const float *b=m->vertices[face->first+i].position,*c=m->vertices[face->first+i+1].position;
            sum+=((double)a[0]*(b[1]*c[2]-b[2]*c[1])+(double)a[1]*(b[2]*c[0]-b[0]*c[2])+(double)a[2]*(b[0]*c[1]-b[1]*c[0]))/6;
        }
    }
    return sum;
}
static int cut(rf_geomod_terrain *t,const float center[3],const float extent[3],uint32_t star)
{
    rf_geomod_vertex v[36]={0};rf_geomod_face f[12];rf_geomod_mesh_view mesh={v,f,36,12,0};
    const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};const uint32_t corners[2][3]={{0,1,2},{0,2,3}};
    if(!star)return rf_geomod_terrain_cut_box(t,center,extent,7);
    for(uint32_t axis=0;axis<3;axis++)for(uint32_t side=0;side<2;side++)for(uint32_t tri=0;tri<2;tri++) {
        uint32_t face=(axis*2+side)*2+tri,a=(axis+1)%3,b=(axis+2)%3;
        f[face]=(rf_geomod_face){face*3,3,7,UINT32_MAX};
        for(uint32_t j=0;j<3;j++) {
            uint32_t k=corners[tri][j];if(!side)k=3-k;
            v[face*3+j].position[axis]=center[axis]+(side?extent[axis]:-extent[axis]);
            v[face*3+j].position[a]=center[a]+u[k]*extent[a];
            v[face*3+j].position[b]=center[b]+w[k]*extent[b];
        }
    }
    return rf_geomod_terrain_cut_star(t,&mesh,center);
}
static int run(uint32_t star)
{
    rf_geomod_vertex vertices[24]={0},saved[4096];rf_geomod_face faces[6],saved_faces[800];
    rf_collision_face_filter filters[6]={{0}},generated={0};rf_geomod_terrain *t=NULL,*reload=NULL;
    rf_geomod_mesh_view mesh={vertices,faces,24,6,0};rf_geomod_terrain_view view,after;
    const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};
    const float centers[4][3]={{0,0,0},{5,0,0},{-1,0,0},{-6,0,0}},extents[4][3]={{1,12,12},{2,2,2},{2,2,2},{1,12,12}};
    const double volumes[4]={3600,3600,3568,1568};staging stage={0},decoded={0};uint32_t bytes;
    unsigned char encoded[RF_GEOMOD_HISTORY_MAX_BYTES];
    for(uint32_t axis=0;axis<3;axis++)for(uint32_t side=0;side<2;side++) {
        uint32_t f=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;faces[f]=(rf_geomod_face){f*4,4,5,f};
        for(uint32_t j=0;j<4;j++) {uint32_t k=side?j:3-j;vertices[f*4+j].position[axis]=side?10:-10;
            vertices[f*4+j].position[a]=u[k]*10;vertices[f*4+j].position[b]=w[k]*10;}
    }
    CHECK(!rf_geomod_terrain_open(&mesh,filters,&generated,0,4096,800,1179648,&t));
    CHECK(!rf_geomod_terrain_set_extraction(t,emit,&stage));
    for(uint32_t c=0;c<4;c++) {
        clear(&stage);
        CHECK(!rf_geomod_terrain_get(t,&view));
        memcpy(saved,view.mesh.vertices,view.mesh.vertex_count*sizeof(*saved));
        memcpy(saved_faces,view.mesh.faces,view.mesh.face_count*sizeof(*saved_faces));
        stage.reject=c==3?4:1;
        CHECK(cut(t,centers[c],extents[c],star)==RF_IO);
        CHECK(stage.count==(c==3?1u:0u));
        CHECK(!rf_geomod_terrain_get(t,&after));
        CHECK(after.cuts==c && after.mesh.vertex_count==view.mesh.vertex_count && after.mesh.face_count==view.mesh.face_count);
        CHECK(!memcmp(saved,after.mesh.vertices,view.mesh.vertex_count*sizeof(*saved)));
        CHECK(!memcmp(saved_faces,after.mesh.faces,view.mesh.face_count*sizeof(*saved_faces)));
        clear(&stage);
        CHECK(!cut(t,centers[c],extents[c],star));
        CHECK(!rf_geomod_terrain_get(t,&view));CHECK(fabs(volume(&view.mesh)-volumes[c])<.0001);
        CHECK(stage.count==(c==3?2u:1u));
        printf("PASS production extraction star%u cut%u volume%g batches%u peak%u\n",star,c+1,volume(&view.mesh),stage.count,view.peak_bytes);
    }
    CHECK(rf_geomod_terrain_set_extraction(t,NULL,NULL)==RF_RANGE);
    CHECK(!rf_geomod_terrain_history_size(t,&bytes));CHECK(!rf_geomod_terrain_history_encode(t,encoded,bytes));
    CHECK(!rf_geomod_terrain_open(&mesh,filters,&generated,0,4096,800,1179648,&reload));
    CHECK(!rf_geomod_terrain_set_extraction(reload,emit,&decoded));
    CHECK(!rf_geomod_terrain_history_decode(reload,encoded,bytes));CHECK(!rf_geomod_terrain_get(reload,&after));
    CHECK(after.mesh.vertex_count==view.mesh.vertex_count && after.mesh.face_count==view.mesh.face_count);
    CHECK(!memcmp(after.mesh.vertices,view.mesh.vertices,view.mesh.vertex_count*sizeof(*saved)));
    CHECK(!memcmp(after.mesh.faces,view.mesh.faces,view.mesh.face_count*sizeof(*saved_faces)));
    CHECK(decoded.count==stage.count && decoded.random.value==stage.random.value);
    for(uint32_t b=0;b<stage.count;b++)CHECK(rf_geomod_piece_batch_count(stage.batch[b])==rf_geomod_piece_batch_count(decoded.batch[b]));
    puts("PASS production extraction rejection rollback and checkpoint replay");
    clear(&decoded);clear(&stage);rf_geomod_terrain_close(&reload);rf_geomod_terrain_close(&t);return 0;
}

int main(void){CHECK(!run(0));CHECK(!run(1));return 0;}
