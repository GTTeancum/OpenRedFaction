/* Internal chronological-rebuild infrastructure, not a full UV correction. */
#include "../src/core/geomod.c"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"lineage line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void quad(rf_geomod_vertex v[4],float x)
{
    static const float xy[4][2]={{0,0},{1,0},{1,1},{0,1}};unsigned i;
    for(i=0;i<4;i++){v[i]=(rf_geomod_vertex){{xy[i][0]+x,xy[i][1],0},{xy[i][0]+x,xy[i][1]}};}
}
int main(void)
{
    rf_geomod_storage *s=NULL;rf_geomod_multi_work *w=calloc(1,sizeof(*w));
    geomod_face_lineage *tags=calloc(1,sizeof(*tags));rf_geomod_vertex v[6];
    rf_geomod_face f={0,4,47,UINT32_MAX};rf_geomod_mesh_view source,pending;unsigned i;
    CHECK(w && tags);quad(v,0);source=(rf_geomod_mesh_view){v,&f,4,1,1};
    CHECK(!rf_geomod_storage_open(&source,128,16,32768,&s));CHECK(!rf_geomod_storage_begin(s));
    CHECK(!append_compact_lineage(s,v,4,47,UINT32_MAX,w,NULL,0,tags,7));
    quad(v,3);CHECK(!append_compact_lineage(s,v,4,47,UINT32_MAX,w,NULL,0,tags,9));
    quad(v,1);CHECK(!append_compact_lineage(s,v,4,47,UINT32_MAX,w,NULL,0,tags,7));
    CHECK(!rf_geomod_storage_pending(s,&pending));CHECK(pending.face_count==2);
    CHECK(tags->pending[0]==9 && tags->pending[1]==7); /* merge removed index0 */
    quad(v,2);CHECK(!append_compact_lineage(s,v,4,47,UINT32_MAX,w,NULL,0,tags,8));
    CHECK(!rf_geomod_storage_pending(s,&pending));CHECK(pending.face_count==3);
    CHECK(tags->pending[0]==9 && tags->pending[1]==7 && tags->pending[2]==8);
    memset(w->compact_planes,255,sizeof(w->compact_planes));
    CHECK(!repair_cavity_pending_lineage(s,w,tags));
    CHECK(!rf_geomod_storage_pending(s,&pending));CHECK(pending.face_count==3);
    CHECK(tags->pending[0]==9 && tags->pending[1]==7 && tags->pending[2]==8);
    rf_geomod_storage_abort(s);CHECK(!rf_geomod_storage_begin(s));
    {
        static const float xy[6][2]={{0,0},{2,0},{2,1},{1,1},{1,2},{0,2}};
        for(i=0;i<6;i++)v[i]=(rf_geomod_vertex){{xy[i][0],xy[i][1],0},{xy[i][0],xy[i][1]}};
        CHECK(!rf_geomod_storage_append(s,v,6,47,UINT32_MAX));tags->pending[0]=13;
        CHECK(!repair_cavity_pending_lineage(s,w,tags));
        CHECK(!rf_geomod_storage_pending(s,&pending));CHECK(pending.face_count>1);
        for(i=0;i<pending.face_count;i++)CHECK(tags->pending[i]==13);
    }
    rf_geomod_storage_close(&s);
    /* Repaired chronological meshes may exceed the old768 support guard.
     * Both cached800 and uncached1024 owners must use their actual arrays. */
    for(unsigned capacity=800;capacity<=1024;capacity+=224) {
        uint16_t edges[4]={1,2,3,4};quad(v,0);
        CHECK(!rf_geomod_storage_open(&source,4096,capacity,300000,&s));
        CHECK(!rf_geomod_storage_begin(s));
        for(i=0;i<capacity;i++) {
            quad(v,2.f*i);
            CHECK(!append_compact_lineage(s,v,4,i,UINT32_MAX,w,edges,(uint16_t)i,tags,1));
        }
        CHECK(!rf_geomod_storage_pending(s,&pending) && pending.face_count==capacity);
        for(i=0;i<capacity;i++)CHECK(w->compact_planes[i]==i && tags->pending[i]==1 &&
            !memcmp(w->compact_edges+4*i,edges,sizeof(edges)));
        quad(v,2.f*capacity);
        CHECK(append_compact_lineage(s,v,4,capacity,UINT32_MAX,w,edges,0,tags,1)==RF_RANGE);
        CHECK(!rf_geomod_storage_pending(s,&pending) && pending.face_count==capacity);
        rf_geomod_storage_close(&s);
    }
    {
        geomod_diagonals *d=calloc(1,sizeof(*d));uint16_t id,reverse,edges[3];
        const float a[3]={-42.3562775f,-8.54142284f,-7.99167967f};
        const float b[3]={-41.3957748f,-8.23730373f,-9.30689907f};
        const float short_a[3]={-41.7697449f,-8.35571194f,-8.79481888f};
        const float cut_plane[4]={.545082092f,.831822276f,.104676485f,30.6184616f};
        geomod_corner_support support={w,0,NULL,d};float expected[3];unsigned variant;
        CHECK(d && !diagonal_register(d,a,b,&id) && !diagonal_register(d,b,a,&reverse));
        CHECK(id==reverse && d->count==1 && !diagonal_intersection(d,id,cut_plane,expected));
        memcpy(w->source_planes[0],(float[4]){-.701539516f,.607983351f,-.371750712f,-27.4924736f},16);
        memcpy(w->source_planes[1],cut_plane,16);edges[0]=id;edges[1]=edges[2]=0;
        for(variant=0;variant<2;variant++) {
            rf_geomod_vertex triangle[3]={0},front[64],back[64];uint16_t fe[64],be[64];uint32_t nf,nb,found=0;
            memcpy(triangle[0].position,variant?short_a:a,12);memcpy(triangle[1].position,b,12);
            memcpy(triangle[2].position,(float[3]){-41.9889717f,-8.21230984f,-8.14658356f},12);
            CHECK(!polygon_split_edges(triangle,3,cut_plane,front,64,back,64,&nf,&nb,edges,1,fe,be,&support));
            for(i=0;i<nf;i++)if(!memcmp(front[i].position,expected,12))found++;
            for(i=0;i<nb;i++)if(!memcmp(back[i].position,expected,12))found++;
            CHECK(found==2);
        }
        free(d);
    }
    for(unsigned variant=0;variant<5;variant++) {
        geomod_step_support *provenance=calloc(1,sizeof(*provenance));rf_geomod_vertex first[4];
        CHECK(provenance);quad(v,0);memcpy(first,v,sizeof(first));
        CHECK(!rf_geomod_storage_open(&source,128,16,32768,&s) && !rf_geomod_storage_begin(s));
        CHECK(!rf_geomod_storage_append(s,v,4,47,UINT32_MAX));tags->pending[0]=7;w->compact_planes[0]=0;
        for(i=0;i<4;i++)w->compact_edges[i]=1;
        v[1].uv[0]+=.000001f;
        if(variant==4)for(i=0;i<2;i++){rf_geomod_vertex swap=v[i];v[i]=v[3-i];v[3-i]=swap;}
        CHECK(!rf_geomod_storage_append(s,v,4,variant==1?48:47,UINT32_MAX));
        tags->pending[1]=variant==2?8:7;w->compact_planes[1]=variant==3?2:0;
        for(i=4;i<8;i++)w->compact_edges[i]=1;
        CHECK(!repair_cavity_pending_provenance(s,w,tags,provenance));
        CHECK(!rf_geomod_storage_pending(s,&pending) && pending.face_count==(variant?2u:1u));
        CHECK(!memcmp(first,pending.vertices,sizeof(first)) && tags->pending[0]==7);
        rf_geomod_storage_close(&s);free(provenance);
    }
    {
        /* Captured admission4 polygon requires the center-fan fallback. */
        rf_geomod_vertex polygon[7]={
            {{-22.0769176f,-11.6484413f,5.15037346f},{0.576053143f,0.537973881f}},
            {{-22.414629f,-11.1988411f,5.2013998f},{0.667458534f,0.509026885f}},
            {{-22.1631241f,-10.2872276f,5.54866076f},{0.677046061f,0.364985287f}},
            {{-22.0758324f,-10.9216585f,5.37529659f},{0.621132135f,0.441316336f}},
            {{-22.0758495f,-10.9327126f,5.37187481f},{0.620446503f,0.442786455f}},
            {{-22.0768509f,-11.6044664f,5.16398239f},{0.578780711f,0.532125473f}},
            {{-22.0769043f,-11.6392584f,5.15321541f},{0.576622725f,0.536752582f}}};
        geomod_step_support *provenance=calloc(1,sizeof(*provenance));CHECK(provenance);
        CHECK(!rf_geomod_storage_open(&source,128,16,32768,&s) && !rf_geomod_storage_begin(s));
        CHECK(!rf_geomod_storage_append(s,polygon,7,47,UINT32_MAX));tags->pending[0]=7;w->compact_planes[0]=0;
        for(i=0;i<7;i++)w->compact_edges[i]=0;
        CHECK(!repair_cavity_pending_provenance(s,w,tags,provenance));
        CHECK(!rf_geomod_storage_pending(s,&pending) && pending.face_count==7 && pending.vertex_count==21);
        CHECK(provenance->diagonals.count==7);
        for(i=0;i<7;i++) {
            uint32_t first=pending.faces[i].first;
            CHECK(tags->pending[i]==7 && provenance->planes[i]==0);
            CHECK(provenance->edges[first]>=GEOMOD_DIAGONAL_BASE && provenance->edges[first+1]==0 && provenance->edges[first+2]>=GEOMOD_DIAGONAL_BASE);
            CHECK(provenance->edges[first+2]==provenance->edges[pending.faces[(i+1)%7].first]);
        }
        rf_geomod_storage_close(&s);free(provenance);
    }
    {
        rf_geomod_vertex triangle[3]={{{1,0,0},{0,0}},{{0,1,0},{0,0}},{{0,0,1},{0,0}}};
        rf_geomod_face face={0,3,0,UINT32_MAX};rf_geomod_mesh_view cutter={triangle,&face,3,1,0};
        geomod_corner_support support={w,0,&cutter,NULL};uint16_t ids[3]={33,35,0};float point[3];
        const float a[4]={1,1,8,-1},b[4]={1,8,1,-1},cut[4]={1,0,0,-.55f};
        w->star_count[0]=1;w->star_kernels[0][0]=w->star_kernels[0][1]=w->star_kernels[0][2]=.1f;
        memcpy(w->star_planes[0][0][1],a,sizeof(a));memcpy(w->star_planes[0][0][3],b,sizeof(b));memcpy(w->source_planes[0],cut,sizeof(cut));
        CHECK(corner_seed_edge(&support,ids,point));
        CHECK(point[0]==.55f && fabsf(point[1]-.05f)<1e-8f && point[1]==point[2]);
        ids[0]=35;ids[1]=33;{float reversed[3];CHECK(corner_seed_edge(&support,ids,reversed) && !memcmp(point,reversed,sizeof(point)));}
    }
    free(tags);free(w);
    puts("PASS birth separation, same-birth merge/index movement, repair and concave partition propagation; original diagonal intersections across shortened edges");return 0;
}
