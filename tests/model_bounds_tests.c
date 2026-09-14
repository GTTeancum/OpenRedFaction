#include "rf/model_bounds.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static uint32_t random_state=7219;
static uint32_t next(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static float number(void){return ((int)(next()%2001)-1000)*.001f;}
int main(void)
{
    rf_model_vertex vertices[24]={0};rf_model_triangle triangles[8]={0};
    int32_t reuse[24]={0};rf_model_draw_batch draw={0};rf_model_geometry g={0};
    rf_model_bounds bounds,saved;uint32_t case_id,i,j,k,culled=0,accepted=0;
    draw.vertices=24;draw.triangles=8;g.vertices=vertices;g.vertex_count=24;
    g.triangles=triangles;g.triangle_count=8;g.batches=&draw;g.batch_count=1;g.reuse=reuse;
    for(i=0;i<8;++i)for(j=0;j<3;++j)triangles[i].indices[j]=(uint16_t)(i*3+j);
    for(case_id=0;case_id<12000;++case_id) {
        uint32_t bones=case_id%51,visible=99,common=31;
        float matrices[50][12]={{0}};rf_model_projection view={0};
        float angle=number()*3.14159f,c=cosf(angle),s=sinf(angle);
        for(i=0;i<24;++i) {
            uint32_t remaining=case_id%5?256:next()%256;
            for(j=0;j<3;++j)vertices[i].position[j]=number()*2;
            for(j=0;j<4;++j) {
                uint32_t weight=j==3?remaining:(remaining?(next()%(remaining+1)):0);
                if(weight>255)weight=255;
                vertices[i].weights[j]=(uint8_t)weight;vertices[i].bones[j]=(uint8_t)(next()%(bones?bones:1));
                remaining-=weight;
            }
            reuse[i]=(int32_t)(i && i%7==0?i:0);
        }
        for(i=0;i<bones;++i) {
            float a=number()*3.14159f,co=cosf(a),si=sinf(a);
            matrices[i][0]=co;matrices[i][2]=si;matrices[i][4]=1;matrices[i][6]=-si;matrices[i][8]=co;
            for(j=0;j<3;++j)matrices[i][9+j]=number();
            if(case_id%4==0)for(j=0;j<9;++j)matrices[i][j]=number()*2; /* Shear, reflection and nonuniform scale. */
        }
        view.camera[0]=number()*30;view.camera[1]=number()*20;view.camera[2]=number()*30;
        view.rotation[0]=c;view.rotation[2]=s;view.rotation[4]=1;view.rotation[6]=-s;view.rotation[8]=c;
        view.perspective=1;view.screen[0]=500;view.screen[1]=-500;view.screen[2]=320;view.screen[3]=240;
        if(case_id%3==0)view.screen[0]=-500;
        if(case_id%5==0)view.screen[1]=500;
        if(case_id%7==0){view.screen[2]=number()*400+320;view.screen[3]=number()*300+240;}
        CHECK(!rf_model_bounds_build(&g,0,bones,&bounds));
        CHECK(!rf_model_bounds_visible(&bounds,bones?matrices:NULL,&view,640,480,&visible));
        /* Independent brute-force float vertex skinning: a rejected bound must
         * have a common outside halfspace for EVERY emitted triangle vertex.
         * This also covers triangles crossing a plane with no inside corner. */
        for(i=0;i<24;++i) {
            uint32_t source=i,mask=0;float posed[3]={0},camera[3]={0},plane[5];
            while(reuse[source]>0)source-=(uint32_t)reuse[source];
            if(!bones)memcpy(posed,vertices[source].position,12);
            else for(j=0;j<4 && vertices[source].weights[j];++j) {
                const float *m=matrices[vertices[source].bones[j]],*p=vertices[source].position;
                for(k=0;k<3;++k)posed[k]+=(((p[0]*m[k]+p[1]*m[3+k])+p[2]*m[6+k])+m[9+k])*(vertices[source].weights[j]/256.0f);
            }
            for(j=0;j<3;++j)for(k=0;k<3;++k)camera[j]+=(posed[k]-view.camera[k])*view.rotation[j*3+k];
            plane[0]=camera[2];plane[1]=view.screen[0]*camera[0]+view.screen[2]*camera[2];
            plane[2]=-view.screen[0]*camera[0]+(640-view.screen[2])*camera[2];
            plane[3]=view.screen[1]*camera[1]+view.screen[3]*camera[2];
            plane[4]=-view.screen[1]*camera[1]+(480-view.screen[3])*camera[2];
            for(j=0;j<5;++j)if(plane[j]<0)mask|=1u<<j;
            common&=mask;
        }
        if(!visible){CHECK(common!=0);++culled;}else ++accepted;
    }
    CHECK(culled>1000 && accepted>1000);
    saved=bounds;vertices[0].weights[0]=vertices[0].weights[1]=255;
    vertices[0].bones[0]=vertices[0].bones[1]=0;vertices[0].weights[2]=vertices[0].weights[3]=0;
    CHECK(rf_model_bounds_build(&g,0,1,&bounds)==RF_NOT_FOUND);CHECK(!memcmp(&bounds,&saved,sizeof(bounds)));
    CHECK(!rf_model_bounds_build(&g,0,0,&bounds)); /* Rigid ignores weights. */
    saved=bounds;reuse[0]=1;CHECK(rf_model_bounds_build(&g,0,0,&bounds)==RF_FORMAT);
    CHECK(!memcmp(&bounds,&saved,sizeof(bounds)));reuse[0]=0;
    triangles[0].indices[0]=24;CHECK(rf_model_bounds_build(&g,0,0,&bounds)==RF_FORMAT);
    printf("Conservative model bounds: 12000 brute-force cases, %u rejected, %u retained, malformed/unsupported guards passed\n",culled,accepted);
    return 0;
}
