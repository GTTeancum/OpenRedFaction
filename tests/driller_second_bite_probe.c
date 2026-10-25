/* ctf06 source148 repeated Driller bites through the shared terrain core.
 * Console run October24: first bite accepted, every overlapping later bite
 * refused (RF_FORMAT, cut2). Bites step along the full host basis (+X).
 * Usage: rf_driller_second_bite_probe [step=0.6] [spin degrees per bite=0]. */
#include "rf/geomod_authored_post.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"second bite line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_geomod_template shape;
int main(int argc,char **argv)
{
    rf_vpp levels={0};rf_level level={0};rf_geometry geometry={0};rf_geomod_authored_post *owner=NULL;
    rf_geomod_authored_post_view a;rf_geomod_terrain *terrain=NULL;rf_collision_face_filter generated={0};
    float first[3]={41.3125f,6.73709393f,-167.045395f};
    float step=argc>1?(float)atof(argv[1]):.6f,turn=argc>2?(float)atof(argv[2]):0;int status,i,accepted=0;
    CHECK(!rf_vpp_open(&levels,"Installed_Game/levelsm.vpp"));CHECK(!rf_level_open(&level,&levels,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geomod_authored_cavity_open_source(&level,&geometry,148,2*1024*1024,&owner));CHECK(!rf_geomod_authored_post_get(owner,&a));
    CHECK(!rf_geomod_template_load("build/data/driller-double.bin",&shape));generated.face_flags=256;
    CHECK(!rf_geomod_terrain_open(&a.source,a.source_filters,&generated,1,4096,1024,2*1024*1024,&terrain));
    if(argc>1 && !strcmp(argv[1],"console")) {
        /* Exact October24 console inputs (rf_scene_drill_trace): accepted then refused. */
        static const float c1[3]={41.3125f,6.73523426f,-167.045349f},b1[9]={1.90241607e-07f,1.80442985e-05f,-1,-0.00415339973f,0.999992311f,1.80433108e-05f,0.999992311f,0.00415339973f,2.82282059e-07f};
        static const float c2[3]={43.4918404f,6.71386909f,-167.052536f},b2[9]={-0.000541562797f,-0.00166201522f,-0.999998689f,-0.00390340947f,0.999991596f,-0.00165987946f,0.999991179f,0.00390251423f,-0.00054856611f};
        int s1=rf_geomod_terrain_cut_template_scale(terrain,&shape,c1,b1,1,999),s2=rf_geomod_terrain_cut_template_scale(terrain,&shape,c2,b2,1,999);
        printf("CONSOLE first %d second %d\n",s1,s2);
        rf_geomod_terrain_close(&terrain);rf_geomod_authored_post_close(&owner);rf_geometry_close(&geometry);rf_vpp_close(&levels);return s1||s2;
    }
    for(i=0;i<12;i++){
        float c[3]={first[0]+step*i,first[1],first[2]},r=turn*i*3.14159265f/180,cs=cosf(r),sn=sinf(r);
        /* Spin about forward (+X): rows are right, up, forward. */
        float basis[9]={0,-sn,-cs,0,cs,-sn,1,0,0};rf_geomod_terrain_view view;
        status=rf_geomod_terrain_cut_template_scale(terrain,&shape,c,basis,1,999);accepted+=!status;
        CHECK(!rf_geomod_terrain_get(terrain,&view));
        printf("BITE %d x=%g status %d faces %u vertices %u\n",i+1,c[0],status,view.mesh.face_count,view.mesh.vertex_count);
    }
    printf("ACCEPTED %d\n",accepted);
    rf_geomod_terrain_close(&terrain);rf_geomod_authored_post_close(&owner);rf_geometry_close(&geometry);rf_vpp_close(&levels);return 0;
}
