#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene_debris_render_plane.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"debris plane line%d: %s\n",__LINE__,#x);return 1;}}while(0)
/* Reproduce the former stored-float offset accumulation explicitly. This is
 * the reconstructed static validator arithmetic, not an original RF.exe RE
 * claim. Volatile stores make every product/subtraction rounding observable. */
static double old_offset_residual(const rf_geomod_vertex v[3],const rf_collision_face *plane)
{
    volatile float offset=0,product;uint32_t j;double residual;
    for(j=0;j<3;j++){product=plane->plane[j]*v[0].position[j];offset=offset-product;}
    residual=offset;for(j=0;j<3;j++)residual+=(double)plane->plane[j]*v[0].position[j];
    return fabs(residual);
}
static int translated_cases(const rf_geomod_vertex source[3],const rf_collision_face *original)
{
    static const float shift[5]={-1024,-128,0,128,1024};
    uint32_t x,y,z,i,j,count=0;double maximum_residual=0,maximum_old=0;
    for(x=0;x<5;x++)for(y=0;y<5;y++)for(z=0;z<5;z++){
        const float delta[3]={shift[x],shift[y],shift[z]};
        rf_geomod_vertex v[3],reverse[3];rf_collision_face plane,back;double length=0,alignment=0;
        memcpy(v,source,sizeof(v));
        for(i=0;i<3;i++)for(j=0;j<3;j++)v[i].position[j]=(float)((double)v[i].position[j]+delta[j]);
        CHECK(!scene_debris_render_plane(v,&plane));
        for(j=0;j<4;j++)CHECK(isfinite(plane.plane[j]));
        for(j=0;j<3;j++){length+=(double)plane.plane[j]*plane.plane[j];alignment+=(double)plane.plane[j]*original->plane[j];}
        CHECK(fabs(length-1)<1e-6 && alignment>.9999);
        reverse[0]=v[0];reverse[1]=v[2];reverse[2]=v[1];CHECK(!scene_debris_render_plane(reverse,&back));
        for(j=0;j<4;j++)CHECK(back.plane[j]==-plane.plane[j]);
        for(i=0;i<3;i++){
            double residual=plane.plane[3];
            double ulp=fmax(fabs((double)nextafterf(plane.plane[3],INFINITY)-plane.plane[3]),
                            fabs((double)nextafterf(plane.plane[3],-INFINITY)-plane.plane[3]));
            for(j=0;j<3;j++)residual+=(double)plane.plane[j]*v[i].position[j];
            CHECK(fabs(residual)<=ulp+1e-6);
            if(fabs(residual)>maximum_residual)maximum_residual=fabs(residual);
        }
        {double old=old_offset_residual(v,&plane);if(old>maximum_old)maximum_old=old;}
        count++;
    }
    CHECK(count==125);printf("TRANSLATIONS %u max_residual %.9g old_float_accumulation %.9g\n",count,maximum_residual,maximum_old);
    return 0;
}
int main(void)
{
    /* Actual ctf06 frame494/chunk1/face9 failure, retained round-trip floats. */
    rf_geomod_vertex v[3]={
        {{-21.2214336f,-112.429863f,-.806796908f},{.0163035691f,.00621501449f}},
        {{-21.2760887f,-112.367043f,-.669759631f},{.00232336111f,.00339923846f}},
        {{-21.1958199f,-112.397339f,-.592218161f},{-.00611062068f,.0150751527f}}};
    rf_geomod_vertex reversed[3],invalid[3];rf_collision_face plane,back,sentinel;
    uint32_t i,j;double length=0,edge1[3],edge2[3],dot=0;
    CHECK(!scene_debris_render_plane(v,&plane));CHECK(plane.count==3 && !plane.vertices);
    CHECK(old_offset_residual(v,&plane)>1e-5);
    printf("CAPTURED_OLD_RESIDUAL %.9g\n",old_offset_residual(v,&plane));
    CHECK(!translated_cases(v,&plane));
    for(j=0;j<3;j++){length+=(double)plane.plane[j]*plane.plane[j];edge1[j]=(double)v[1].position[j]-v[0].position[j];edge2[j]=(double)v[2].position[j]-v[0].position[j];}
    CHECK(fabs(length-1)<1e-6);
    for(j=0;j<3;j++)dot+=plane.plane[j]*(edge1[(j+1)%3]*edge2[(j+2)%3]-edge1[(j+2)%3]*edge2[(j+1)%3]);
    CHECK(dot>0); /* Original winding retained; no flipped outward normal. */
    reversed[0]=v[0];reversed[1]=v[2];reversed[2]=v[1];CHECK(!scene_debris_render_plane(reversed,&back));
    for(j=0;j<4;j++)CHECK(back.plane[j]==-plane.plane[j]);
    for(i=0;i<3;i++){
        double residual=plane.plane[3],bound=0;
        for(j=0;j<3;j++)residual+=(double)plane.plane[j]*v[i].position[j];
        /* Explicit float offset quantization, no static1e-5 demand. */
        bound=fabs((double)nextafterf(plane.plane[3],INFINITY)-plane.plane[3]);
        CHECK(fabs(residual)<=bound+1e-6);
    }
    memset(&sentinel,0x5a,sizeof(sentinel));
    memcpy(invalid,v,sizeof(v));invalid[1]=invalid[0];plane=sentinel;
    CHECK(scene_debris_render_plane(invalid,&plane)==RF_FORMAT && !memcmp(&plane,&sentinel,sizeof(plane)));
    memcpy(invalid,v,sizeof(v));invalid[2].position[1]=NAN;
    CHECK(scene_debris_render_plane(invalid,&plane)==RF_FORMAT && !memcmp(&plane,&sentinel,sizeof(plane)));
    invalid[2].position[1]=INFINITY;CHECK(scene_debris_render_plane(invalid,&plane)==RF_FORMAT);
    CHECK(scene_debris_render_plane(NULL,&plane)==RF_RANGE && scene_debris_render_plane(v,NULL)==RF_RANGE);
    puts("PASS actual frame494 render plane, winding reversal, finite/degenerate guards and failure atomicity");return 0;
}
