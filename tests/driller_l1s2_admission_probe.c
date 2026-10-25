/* L1S2 campaign Driller: cavity admission for a drill cutter at the rock face
 * the Driller reaches driving +X from its authored pose (console October24:
 * cutter bounds 137.81,-2.327,-17.993 .. 142.187,0.729,-6.666 refused on 8123).
 * Prints the source and window bounds near the cutter and the admission result. */
#include "rf/geomod_authored_post.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"l1s2 admission line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void bounds(const rf_geomod_mesh_view *m,const rf_geomod_face *f,float lo[3],float hi[3])
{
    uint32_t v,k,first=f?f->first:0,count=f?f->count:m->vertex_count;
    for(k=0;k<3;k++)lo[k]=hi[k]=m->vertices[first].position[k];
    for(v=first;v<first+count;v++)for(k=0;k<3;k++){float x=m->vertices[v].position[k];if(x<lo[k])lo[k]=x;if(x>hi[k])hi[k]=x;}
}
int main(void)
{
    rf_vpp levels={0};rf_level level={0};rf_geometry geometry={0};rf_geomod_authored_post *o=NULL;rf_geomod_authored_post_view a;
    float lo[3],hi[3],clo[3]={137.81f,-2.327f,-17.993f},chi[3]={142.187f,.729f,-6.666f};uint32_t uid,i,reference=0;
    CHECK(!rf_vpp_open(&levels,"Installed_Game/levels1.vpp"));CHECK(!rf_level_open(&level,&levels,"L1S2.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    for(uid=8123;uid<=8219;uid+=96) {
        CHECK(!rf_geomod_authored_cavity_open_source(&level,&geometry,uid,2*1024*1024,&o));CHECK(!rf_geomod_authored_post_get(o,&a));
        bounds(&a.source,NULL,lo,hi);
        printf("SOURCE %u room %u faces %u windows %u: %g %g %g .. %g %g %g\n",uid,a.room,a.source.face_count,a.windows.face_count,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
        for(i=0;i<a.windows.face_count;i++) {
            bounds(&a.windows,a.windows.faces+i,lo,hi);
            if(hi[0]>130)printf("  window %u ref %u: %g %g %g .. %g %g %g\n",i,a.window_origins[i].reference,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
        }
        printf("  admit %d ref %u\n",rf_geomod_authored_cavity_admit(o,clo,chi,&reference),reference);
        {float z;for(z=-12.25f;z<=0.f;z+=1.5f){float l2[3]={clo[0],clo[1],z-5.66f},h2[3]={chi[0],chi[1],z+5.66f};
            printf("  centre z %g admit %d\n",z,rf_geomod_authored_cavity_admit(o,l2,h2,&reference));}}
        rf_geomod_authored_post_close(&o);
    }
    rf_geometry_close(&geometry);rf_vpp_close(&levels);return 0;
}
