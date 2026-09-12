#include "particle_stretch_fixture.h"
#include "pc_raster.h"
#include <stdio.h>
#include <string.h>
#include "corpse_surface_fixture.h"
#include "packed_lightmap_fixture.h"
typedef struct corpse_pixel_sink {rf_pc_raster *raster;const rf_image *image;uint32_t calls;int fail;} corpse_pixel_sink;
static int corpse_present(void *context,const rf_particle_draw_vertex *vertices,uint32_t count,const rf_image *image,uint32_t mode)
{
    corpse_pixel_sink *sink=context;++sink->calls;
    if(image!=sink->image || mode!=RF_PARTICLE_NORMAL_MODE || count<3 || count>12)return RF_FORMAT;
    if(sink->fail)return RF_IO;
    return rf_pc_raster_particle(sink->raster,vertices,count,image,mode,RF_SCENE_PARTICLE_DEPTH_SCALE,RF_SCENE_PARTICLE_DEPTH_BIAS,0,0);
}
static int corpse_texture_test(const char *path,int samples)
{
    rf_pc_raster raster={0};rf_vpp archive;rf_particle_bitmap texture={0};rf_visibility_camera camera={0};
    rf_particle_vertex_environment environment={0};rf_corpse_surface_effect effect={0};
    corpse_pixel_sink sink;uint32_t i,j,changed[5],depth,hash,calls;int status;
    if(rf_pc_raster_open(&raster,1) || rf_vpp_open(&archive,path))return 1;
    status=rf_corpse_surface_texture_open(&texture,&archive,1,16420);rf_vpp_close(&archive);if(status)return 2;
    sink.raster=&raster;sink.image=&texture.image;sink.calls=0;sink.fail=0;
    corpse_surface_fixture(&camera,&environment,&effect);
    for(i=0;i<5;++i) {
        depth=i==4?0:16777215;changed[i]=0;hash=2166136261u;
        for(j=0;j<raster.pixels;++j){raster.rgb[j*3]=32;raster.rgb[j*3+1]=64;raster.rgb[j*3+2]=96;raster.depth[j]=depth;}
        effect.elapsed=i==0?0:i==1?2.5f:5;
        status=rf_corpse_surface_draw(&effect,&camera,&environment,&texture.image,RF_PARTICLE_NORMAL_MODE,corpse_present,&sink);if(status)return 3;
        for(j=0;j<raster.pixels;++j) {
            if(raster.depth[j]!=depth)return 4;
            if(raster.rgb[j*3]!=32 || raster.rgb[j*3+1]!=64 || raster.rgb[j*3+2]!=96)++changed[i];
            hash=(hash^raster.rgb[j*3])*16777619u;hash=(hash^raster.rgb[j*3+1])*16777619u;hash=(hash^raster.rgb[j*3+2])*16777619u;
        }
        if(!samples)printf("%u %u %u\n",i,changed[i],hash);
        else if(i<4){uint32_t x,y;for(y=0;y<16;++y)for(x=0;x<16;++x){j=((214+y*4)*raster.width+284+x*5)*3;printf("%u %u %u\n",raster.rgb[j],raster.rgb[j+1],raster.rgb[j+2]);}}

    }
    if(changed[0] || !changed[1] || changed[2]<changed[1] || changed[2]!=changed[3] || changed[4])return 5;
    calls=sink.calls;effect.position[2]=-4;effect.extent=-999;
    if(rf_corpse_surface_draw(&effect,&camera,&environment,&texture.image,RF_PARTICLE_NORMAL_MODE,corpse_present,&sink) || sink.calls!=calls || effect.extent!=.5f)return 6;
    effect.position[2]=4;effect.extent=-999;sink.fail=1;
    if(rf_corpse_surface_draw(&effect,&camera,&environment,&texture.image,RF_PARTICLE_NORMAL_MODE,corpse_present,&sink)!=RF_IO || effect.extent!=.5f || sink.calls!=calls+1)return 7;
    rf_particle_bitmap_close(&texture);rf_pc_raster_close(&raster);return 0;
}
static int packed_lightmap_test(int samples)
{
    rf_pc_raster raster={0};rf_image image={0};rf_particle_draw_vertex vertices[4];uint32_t i,x,y;int status;
    if(rf_pc_raster_open(&raster,1))return 1;
    status=packed_lightmap_fixture(&image,vertices);if(status)return 2;
    if(samples){uint32_t output[132];packed_lightmap_sample_fixture(&image,output);for(i=0;i<66;++i)printf("%u %u\n",output[i*2],output[i*2+1]);rf_image_close(&image);rf_pc_raster_close(&raster);return 0;}
    for(i=0;i<raster.pixels;++i){raster.rgb[i*3]=32;raster.rgb[i*3+1]=64;raster.rgb[i*3+2]=96;raster.depth[i]=16777215;}
    status=rf_pc_raster_particle(&raster,vertices,4,&image,RF_PARTICLE_NORMAL_MODE,1,0,0,0);if(status)return 3;
    for(y=0;y<2;++y)for(x=0;x<32;++x){i=((96+y*64)*raster.width+72+x*16)*3;printf("%u %u %u\n",raster.rgb[i],raster.rgb[i+1],raster.rgb[i+2]);}
    rf_image_close(&image);rf_pc_raster_close(&raster);return 0;
}
static int texture_test(const char *path)
{
    rf_pc_raster r={0};rf_vpp archive;rf_particle_definition definition={0};rf_particle_animation animation={0};rf_particle particle={0};
    rf_particle_draw_vertex v[4];unsigned i,j,x,y;int status;
    if(rf_pc_raster_open(&r,1) || rf_vpp_open(&archive,path))return 1;
    strcpy(definition.bitmap,"boom01.vbm");
    status=rf_particle_animation_open(&animation,&definition,&archive,1,1048576);if(status)return 2;
    rf_vpp_close(&archive);memset(&definition,0xdd,sizeof(definition));
    particle.frame_count=(uint16_t)animation.count;particle.life=1;
    for(i=0;i<6;i++) {
        uint32_t frame;particle.age=(float)(i%3==0?0:i%3==1?7:15)/animation.count;
        status=rf_particle_frame_index(&particle,&frame);if(status || frame>=animation.count)return 2;
        for(j=0;j<r.pixels;j++){r.rgb[j*3]=32;r.rgb[j*3+1]=64;r.rgb[j*3+2]=96;r.depth[j]=16777215;}
        memset(v,0,sizeof(v));
        for(j=0;j<4;j++) {
            v[j].screen[0]=32+((j==1 || j==2)?128:0);v[j].screen[1]=32+(j>=2?128:0);
            v[j].depth=1000;v[j].reciprocal_w=1;v[j].argb=0xffffffff;v[j].fog=0xff000000;
            v[j].uv[0]=(j==1 || j==2)?1:0;v[j].uv[1]=j>=2?1:0;
        }
        status=rf_pc_raster_particle(&r,v,4,animation.images+frame,i<3?RF_PARTICLE_NORMAL_MODE:RF_PARTICLE_GLOW_MODE,1,0,0,0);if(status)return 3;
        for(y=0;y<16;y++)for(x=0;x<16;x++){j=((36+y*8)*r.width+36+x*8)*3;printf("%u %u %u\n",r.rgb[j],r.rgb[j+1],r.rgb[j+2]);}
    }
    rf_particle_animation_close(&animation);rf_pc_raster_close(&r);return 0;
}
static int stretch_test(void)
{
    rf_pc_raster raster={0};unsigned char pixel[4]={255,255,255,255};rf_image image={1,1,4,0,pixel};
    uint32_t i,j,x,y,count;rf_particle_draw_vertex vertices[12];int status;
    if(rf_pc_raster_open(&raster,1))return 1;
    for(i=0;i<6;i++) {
        for(j=0;j<raster.pixels;j++){raster.rgb[j*3]=32;raster.rgb[j*3+1]=64;raster.rgb[j*3+2]=96;raster.depth[j]=16777215;}
        status=particle_stretch_fixture(i,vertices,&count);if(status)return 2;
        if(count){status=rf_pc_raster_particle(&raster,vertices,count,&image,RF_PARTICLE_NORMAL_MODE,
            RF_SCENE_PARTICLE_DEPTH_SCALE,RF_SCENE_PARTICLE_DEPTH_BIAS,0,0);if(status)return 3;}
        printf("%u\n",count);
        for(y=0;y<16;y++)for(x=0;x<16;x++){j=((15+y*30)*raster.width+20+x*40)*3;printf("%u %u %u\n",raster.rgb[j],raster.rgb[j+1],raster.rgb[j+2]);}
    }
    rf_pc_raster_close(&raster);return 0;
}
static int flash_test(void)
{
    const uint32_t colors[4]={0x00ff0000,0x80ff0000,0xffff0000,0x804080c0};
    const uint32_t points[5][2]={{0,0},{639,0},{0,479},{639,479},{320,240}};
    rf_pc_raster r={0};rf_particle_draw_vertex v[4];uint32_t i,j,k;
    if(rf_pc_raster_open(&r,1))return 1;
    for(i=0;i<4;++i) {
        for(j=0;j<r.pixels;++j){r.rgb[j*3]=32;r.rgb[j*3+1]=64;r.rgb[j*3+2]=96;r.depth[j]=0;}
        memset(v,0,sizeof(v));
        for(j=0;j<4;++j){v[j].screen[0]=(j==1 || j==2)?640:0;v[j].screen[1]=j>=2?480:0;
            v[j].reciprocal_w=1;v[j].depth=16777215;v[j].argb=colors[i];}
        if(rf_pc_raster_particle(&r,v,4,NULL,0x18000,1,0,0,0))return 2;
        for(j=0;j<r.pixels;++j) {
            if(r.depth[j]!=0)return 3;
            for(k=0;k<3;++k){int source=(colors[i]>>(16-k*8))&255,alpha=colors[i]>>24;
                int ref=(source*alpha+(32+32*k)*(255-alpha)+127)/255;
                int delta=(int)r.rgb[j*3+k]-ref;if(delta < -1 || delta > 1)return 4;}
        }
        for(j=0;j<5;++j){k=(points[j][1]*r.width+points[j][0])*3;printf("%u %u %u\n",r.rgb[k],r.rgb[k+1],r.rgb[k+2]);}
    }
    rf_pc_raster_close(&r);return 0;
}
int main(int argc,char **argv)
{
    if(argc==3 && !strcmp(argv[1],"--corpse-pixels"))return corpse_texture_test(argv[2],1);
    if(argc==3 && !strcmp(argv[1],"--corpse-texture"))return corpse_texture_test(argv[2],0);
    if(argc==2 && !strcmp(argv[1],"--packed-lightmap-samples"))return packed_lightmap_test(1);
    if(argc==2 && !strcmp(argv[1],"--packed-lightmap"))return packed_lightmap_test(0);
    if(argc==2 && !strcmp(argv[1],"--flash"))return flash_test();
    if(argc==2 && !strcmp(argv[1],"--stretch"))return stretch_test();
    if(argc==3 && !strcmp(argv[1],"--textures"))return texture_test(argv[2]);
    static const unsigned char expected[12][3]={{144,32,48},{160,64,96},{16,160,48},{88,48,72},{255,0,0},{127,0,0},{127,0,0},{63,128,0},{80,96,48},{24,48,80},{32,64,96},{255,0,0}};
    rf_pc_raster r={0};rf_particle_draw_vertex v[4];unsigned char texel[4];
    rf_image image={1,1,4,0,texel};uint32_t i,j,k;int status;
    if(rf_pc_raster_open(&r,1))return 1;
    for(i=0;i<12;i++) {
        for(j=0;j<r.pixels;j++){r.rgb[j*3]=32;r.rgb[j*3+1]=64;r.rgb[j*3+2]=96;r.depth[j]=16777215;}
        memset(v,0,sizeof(v));memset(texel,255,4);texel[3]=128;
        for(j=0;j<4;j++) {
            v[j].screen[0]=32+((j==1 || j==2)?80:0);v[j].screen[1]=48+(j>=2?80:0);
            v[j].depth=1000;v[j].reciprocal_w=1;v[j].argb=0xffff0000;v[j].fog=0xff000000;
            v[j].uv[0]=(j==1 || j==2)?1:0;v[j].uv[1]=j>=2?1:0;
        }
        if(i>=4 && i<8) {
            for(j=48;j<128;j++)for(k=32;k<112;k++){unsigned p=j*r.width+k;r.depth[p]=1000;r.rgb[p*3]=255;r.rgb[p*3+1]=r.rgb[p*3+2]=0;}
            for(j=0;j<4;j++){v[j].argb=0xff000000;v[j].depth=i>=6?500:2000;}
        }
        if(i==2 || i==8)for(j=0;j<4;j++)v[j].fog=i==8?0x80000000:0;
        if(i==3)for(j=0;j<4;j++)v[j].argb=0x80ff0000;
        if(i>=9){texel[3]=i==10?0:i==11?255:128;if(i==9){texel[0]=16;texel[1]=32;texel[2]=64;for(j=0;j<4;j++)v[j].argb=0xffffffff;}}
        status=rf_pc_raster_particle(&r,v,4,&image,i==1?RF_PARTICLE_GLOW_MODE:i==5?RF_PARTICLE_NORMAL_MODE&~(31u<<20):RF_PARTICLE_NORMAL_MODE,1,0,i==2 || i==8,0xff00);
        if(status)return 2;
        if(i==7){for(j=0;j<4;j++){v[j].argb=0xff00ff00;v[j].depth=750;}if(rf_pc_raster_particle(&r,v,4,&image,RF_PARTICLE_NORMAL_MODE,1,0,0,0))return 3;}
        for(j=56;j<120;j++)for(k=40;k<104;k++) {
            unsigned channel;for(channel=0;channel<3;channel++) {
                int delta=(int)r.rgb[(j*r.width+k)*3+channel]-expected[i][channel];
                if(delta < -1 || delta > 1){fprintf(stderr,"Particle %u pixel %u,%u mismatch\n",i,k,j);return 4;}
            }
            if(r.depth[j*r.width+k]!=(i>=4 && i<8?1000:16777215))return 5;
        }
        j=(88*r.width+72)*3;printf("%u %u %u\n",r.rgb[j],r.rgb[j+1],r.rgb[j+2]);
    }
    rf_pc_raster_close(&r);return 0;
}
