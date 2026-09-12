#include "rf/preview.h"
#include "rf/model_file.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;} } while(0)
static rf_model_render_cache cache[4096];
static float clip[4096][3];
static uint8_t gpu[4096][40];
static uint16_t indices[24576];
static rf_model_clip_pool pool;
int main(void)
{
    rf_model_vertex vertices[3]={0};int32_t reuse[3]={0};
    rf_model_triangle triangle={{0,1,2},0};
    rf_model_draw_batch draw={0,3,0,1,7};
    rf_model_geometry geometry={&draw,vertices,&triangle,reuse,1,3,1,0};
    rf_model_render_buffers buffers={cache,clip,NULL,gpu,4096};
    rf_model_projection view={0};rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};
    float faces[1][4]={{0,0,-1,1}};rf_preview_vertex output[16],prefix;
    rf_preview_mesh mesh;uint32_t emitted,n,mode;
    view.rotation[0]=view.rotation[4]=view.rotation[8]=1;
    view.fixed_depth=view.depth_factor=1;view.screen[0]=320;view.screen[1]=-240;
    view.screen[2]=320;view.screen[3]=240;view.bounds[2]=640;view.bounds[3]=480;
    view.far_depth=1000;view.perspective=view.compute_clip=view.clipping=view.far_clip=1;
    planes.near_depth=.1f;planes.far_depth=1000;
    projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    lights.ambient[0]=lights.ambient[1]=lights.ambient[2]=128;
    vertices[0].position[0]=-.025f;vertices[0].position[1]=-.025f;
    vertices[1].position[0]=.25f;vertices[1].position[1]=-.25f;
    vertices[2].position[1]=.25f;
    for(n=0;n<3;++n){vertices[n].position[2]=1;vertices[n].normal[2]=1;vertices[n].uv[0]=(float)n/2;}
    memset(&prefix,0x5a,sizeof(prefix));
    for(mode=0;mode<4;++mode) {
        vertices[0].position[2]=mode==1?.05f:1;
        faces[0][3]=mode==2?-1:1;
        memset(cache,0xa5,sizeof(cache));memset(gpu,0xa5,sizeof(gpu));
        CHECK(rf_model_geometry_render_static_batch(&geometry,0,&view,&lights,&attributes,NULL,&buffers)==RF_OK);
        /* Static projection must leave the skeletal world cache poisoned. */
        for(n=0;n<3;++n)CHECK(((uint8_t*)cache[n].world)[0]==0xa5);
        output[0]=prefix;mesh.vertices=output;mesh.count=1;mesh.bytes=sizeof(prefix);emitted=999;
        CHECK(rf_preview_static_model_emit(&geometry,0,&buffers,indices,&pool,&view,&planes,&projection,&attributes,
            &mesh,mode==3?sizeof(prefix):sizeof(output),&emitted,faces,NULL)==(mode==3?RF_RANGE:RF_OK));
        CHECK(!memcmp(output,&prefix,sizeof(prefix)));
        if(mode==3){CHECK(mesh.count==1 && mesh.bytes==sizeof(prefix) && emitted==999);continue;}
        if(emitted!=(mode==1?6u:mode==2?0u:3u))fprintf(stderr,"mode %u emitted %u\n",mode,emitted);
        CHECK(emitted==(mode==1?6u:mode==2?0u:3u));
        CHECK(mesh.count==1+emitted && mesh.bytes==mesh.count*sizeof(prefix));
        for(n=1;n<mesh.count;++n) {
            CHECK(output[n].material==7 && output[n].lightmap==UINT32_MAX);
            CHECK(isfinite(output[n].position[0]) && isfinite(output[n].position[1]));
            CHECK(output[n].texture[2]>0 && output[n].texture[2]<=10.001f);
        }
    }
    CHECK(rf_preview_static_model_emit(&geometry,0,&buffers,indices,&pool,&view,&planes,&projection,&attributes,
        &mesh,sizeof(output),&emitted,NULL,NULL)==RF_RANGE);
    puts("Static preview: direct, near clip, stored-plane rejection, preserved prefix and capacity guards passed.");
    return 0;
}
