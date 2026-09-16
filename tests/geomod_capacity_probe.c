/* Isolated larger-profile core probe: no scene/native acceptance claim. */
#define main legacy_history_check_tests
#include "geomod_history_check_tests.c"
#undef main
#include <math.h>
#include <time.h>
#include "geomod_lineage_coverage.inc"
static void ray(const rf_geomod_terrain_view *v,float x,float start,float delta,float expected)
{
    float p[3]={x,start,0},d[3]={0,delta,0};rf_collision_tree_hit hit;uint32_t matched;
    CHECK(!rf_collision_thin_tree(v->tree->nodes,v->tree->node_count,v->tree->faces,
        v->tree->face_count,0,p,d,1,query_stack,8192,&hit,&matched));
    CHECK(matched && fabs((double)hit.hit.fraction-expected)<1e-5);
}
static void holes(const rf_geomod_terrain_view *v,uint32_t cavity)
{
    uint32_t j;float start=cavity?0:15,delta=cavity?15:-10;
    for(j=0;j<v->cuts;j++) {
        float x=-30+4*(float)j;
        ray(v,x,start,delta,cavity?11.f/15:.8f);
        ray(v,x+1.5f,start,delta,cavity?10.f/15:.5f);
    }
    CHECK(lineage_closed(&v->mesh));
}
static void run(uint32_t cavity)
{
    rf_geomod_mesh_view source;rf_geomod_terrain *live=NULL,*reload=NULL;
    rf_geomod_terrain_view a,b;uint32_t i,j,bytes=0;const float extent[3]={1,2,1};
    const uint32_t budget=1024*1024;
    make_source(&source);
    for(i=0;i<24;i++)original_vertices[i].position[0]*=4;
    if(cavity)for(i=0;i<6;i++)for(j=0;j<2;j++) {
        rf_geomod_vertex v=original_vertices[i*4+j];
        original_vertices[i*4+j]=original_vertices[i*4+3-j];original_vertices[i*4+3-j]=v;
    }
    CHECK(!rf_geomod_terrain_open(&source,filters,&generated,cavity,4096,800,budget,&live));
    CHECK(!rf_geomod_terrain_open(&source,filters,&generated,cavity,4096,800,budget,&reload));
    CHECK(!rf_geomod_terrain_set_mapping(live,64,64));CHECK(!rf_geomod_terrain_set_mapping(reload,64,64));
    for(i=0;i<RF_GEOMOD_CUT_LIMIT;i++) {
        float center[3]={-30+4*(float)i,9,0};clock_t begin=clock();
        int status=rf_geomod_terrain_cut_box(live,center,extent,3);
        CHECK(!rf_geomod_terrain_get(live,&a));
        printf("CAPACITY cavity=%u cut=%u status=%d faces=%u resident=%u peak=%u cpu_ms=%.3f\n",
            cavity,i+1,status,a.mesh.face_count,a.resident_bytes,a.peak_bytes,1000.*(clock()-begin)/CLOCKS_PER_SEC);
        fflush(stdout);CHECK(!status && a.cuts==i+1);holes(&a,cavity);
        if(i==RF_GEOMOD_CUT_LIMIT-2) {
            CHECK(!rf_geomod_terrain_history_size(live,&bytes) && bytes<=sizeof(encoded));
            CHECK(!rf_geomod_terrain_history_encode(live,encoded,bytes));
            CHECK(!rf_geomod_terrain_history_decode(reload,encoded,bytes));
            CHECK(!rf_geomod_terrain_get(reload,&b));compare(&a,&b);holes(&b,cavity);
        }
        if(i==RF_GEOMOD_CUT_LIMIT-1) {
            CHECK(!rf_geomod_terrain_cut_box(reload,center,extent,3));
            CHECK(!rf_geomod_terrain_get(reload,&b));compare(&a,&b);holes(&b,cavity);
        }
    }
    CHECK(!rf_geomod_terrain_history_size(live,&bytes));CHECK(!rf_geomod_terrain_history_encode(live,encoded,bytes));
    {float center[3]={35,9,0};CHECK(rf_geomod_terrain_cut_box(live,center,extent,3)==RF_RANGE);}
    CHECK(!rf_geomod_terrain_history_encode(live,again,bytes) && !memcmp(encoded,again,bytes));
    CHECK(!rf_geomod_terrain_get(live,&a));compare(&a,&b);holes(&a,cavity);
    printf("PASS capacity=%u cavity=%u saved_bytes=%u; retained holes, reload/next-cut and overflow rollback\n",RF_GEOMOD_CUT_LIMIT,cavity,bytes);
    rf_geomod_terrain_close(&live);rf_geomod_terrain_close(&reload);
}
int main(void){run(0);run(1);return 0;}
