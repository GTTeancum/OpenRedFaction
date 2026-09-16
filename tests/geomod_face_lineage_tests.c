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
    rf_geomod_storage_close(&s);free(tags);free(w);
    puts("PASS birth separation, same-birth merge/index movement, repair and concave partition propagation; optional scratch2048bytes");return 0;
}
