#include "rf/preview.h"
#include "rf/material.h"
#include "rf/lightmap.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include "rf/scene_preview.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
static float edge(const float *a, const float *b, float x, float y) { return (x-a[0])*(b[1]-a[1])-(y-a[1])*(b[0]-a[0]); }
static int scene_last(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world)
{(void)context;(void)frame;(void)mesh;(void)materials;(void)world;return RF_OK;}
static int miner_skin(const char *path,const char *skin,rf_entity_assets *assets)
{
    char compiled[64];int result=rf_entity_assets_load(path,"miner1",skin,assets,512*1024);
    if(!result)result=rf_entity_skeletal_filename(assets->model,compiled);
    /* Pose diagnostic currently owns miner geometry; do not pair another mesh
     * with that pose stream if the supplied table has a different declaration. */
    if(!result && strcmp(compiled,"miner.v3c"))result=RF_FORMAT;
    return result;
}
static int address(int value, int size, int clamp)
{ return clamp ? (value < 0 ? 0 : value >= size ? size-1 : value) : (value % size + size) % size; }
static void sample(const rf_image *image, float s, float t, int clamp, float color[4])
{
    float x, y, fx, fy;
    int ix, iy, a, b;
    unsigned c;
    s = clamp ? fminf(1, fmaxf(0, s)) : s-floorf(s);
    t = clamp ? fminf(1, fmaxf(0, t)) : t-floorf(t);
    x = s*image->width-0.5f; y = t*image->height-0.5f;
    ix = (int)floorf(x); iy = (int)floorf(y); fx = x-ix; fy = y-iy;
    for (c = 0; c < 4; ++c) color[c] = 0;
    for (b = 0; b < 2; ++b) for (a = 0; a < 2; ++a) {
        float weight = (a ? fx : 1-fx)*(b ? fy : 1-fy);
        unsigned pixel = (unsigned)(address(iy+b, (int)image->height, clamp)*(int)image->width + address(ix+a, (int)image->width, clamp));
        for (c = 0; c < 4; ++c) color[c] += weight*image->rgba[pixel*4+c]/255.0f;
    }
}
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_level level;
    rf_geometry geometry={0};
    rf_preview_mesh mesh={0};
    rf_materials materials = {0};
    rf_lightmaps lightmaps = {0};
    float *depth;
    unsigned char *rgb;
    uint32_t i;
    FILE *output;
    rf_entity_assets skin_assets={0};const char *skin_names[64];
    uint32_t world_vertices=0;
    int door_motion=argc>1 && !strcmp(argv[1],"--scene-door-motion-last");
    int door_view=door_motion || (argc>1 && !strcmp(argv[1],"--scene-door-states-last"));
    int actor_drive=argc>1 && !strcmp(argv[1],"--scene-contact-last")?2:argc>1 && !strcmp(argv[1],"--scene-drive-last");
    int actor_body=actor_drive || (argc>1 && !strcmp(argv[1],"--scene-body-last"));
    int scene_states=actor_body || door_view || (argc>1 && !strcmp(argv[1],"--scene-states-last"));
    int scene_stream=scene_states || (argc>1 && !strcmp(argv[1],"--scene-close-last"));
    int scene_close=scene_stream || (argc>1 && !strcmp(argv[1],"--scene-close"));
    int scene_mode=scene_close || (argc>1 && !strcmp(argv[1],"--scene"));
    int skin_mode=argc>1 && (!strcmp(argv[1],"--model-skin") || !strcmp(argv[1],"--model-skin-last"));
    int model_mode=skin_mode || (argc>1 && (!strcmp(argv[1],"--model") || !strcmp(argv[1],"--model-last")));
    const char *output_path=(model_mode || scene_mode)?(argc>4?argv[4]:NULL):(argc>3?argv[3]:NULL);
    if (argc < 4 || argc > 20) return 2;
    rf_scene_actor_drive(actor_drive);
    if(scene_mode && argc<10)return 2;
    if(skin_mode) {
        if(argc<8 || miner_skin(argv[5],argv[6],&skin_assets))return 1;
        for(i=0;i<skin_assets.texture_count;++i)skin_names[i]=skin_assets.textures[i];
    }
    if(model_mode) {
        if(argc<6 || rf_vpp_open(&archive,argv[2]) || rf_animation_preview(argv[2],argv[3],(!strcmp(argv[1],"--model-last") || !strcmp(argv[1],"--model-skin-last"))?63:0,&mesh,1024*1024))return 1;
    } else {
        if(rf_vpp_open(&archive, argv[scene_mode?2:1]) || rf_level_open(&level, &archive, argv[scene_mode?3:2]))return 1;
        if(scene_close && !door_motion && rf_scene_preview_camera(&level,(int32_t)strtol(argv[8],NULL,10)))return 1;
        if(door_view && rf_scene_preview_mover_camera(&level,8544,6.0f))return 1;
        if(rf_geometry_open(&geometry,&level,8*1024*1024) || rf_preview_build(&mesh,&geometry,&level,8*1024*1024))return 1;
    }
    if (argc > 4) {
        rf_vpp archives[16];
        uint32_t opened = 0;
        int result = RF_OK;
        for (i = scene_mode?9:skin_mode?7:model_mode?5:4; i < (uint32_t)argc; ++i) {
            result = rf_vpp_open(archives+opened, argv[i]);
            if (result) break;
            ++opened;
        }
        if(!result && model_mode) {
            rf_model_file model;rf_model_materials bundle={0};
            result=rf_model_file_open(&model,&archive,"miner.v3c");
            if(!result)result=rf_model_materials_open_skin(&bundle,&model,skin_names,skin_assets.texture_count,archives,opened,4*1024*1024);
            if(!result)for(i=0;i<mesh.count;++i) {
                uint32_t slot=mesh.vertices[i].material;
                if(slot>=bundle.count) {result=RF_FORMAT;break;}
                memcpy(&mesh.vertices[i].material,bundle.items[slot].record.bytes+0x10,4);
            }
            if(!result) {materials=bundle.textures;memset(&bundle.textures,0,sizeof(bundle.textures));}
            rf_model_materials_close(&bundle);
        } else if(!result && scene_mode) {
            rf_preview_close(&mesh);
            if(door_motion) {
                rf_scene_world_geometry owned={0};rf_group_attached_pose *poses=NULL;FILE *input=NULL;
                result=rf_scene_world_open_retained(&level,&geometry,archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,&owned);
                if(!result) {
                    uint32_t capacity=mesh.bytes+1024*1024;void *buffer=realloc(mesh.vertices,capacity);
                    size_t size=owned.movers.count*sizeof(*poses);
                    if(buffer)mesh.vertices=buffer;else result=RF_RANGE;
                    poses=malloc(size?size:1);input=fopen(argv[8],"rb");
                    if(!poses || !input)result=RF_IO;
                    if(!result && (fread(poses,1,size,input)!=size || fgetc(input)!=EOF || ferror(input)))result=RF_FORMAT;
                    if(!result)result=rf_scene_world_update(&owned,poses,owned.movers.count,&mesh,capacity);
                }
                if(input)fclose(input);free(poses);rf_scene_world_geometry_close(&owned);
            } else result=rf_scene_world_open(&level,&geometry,archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024);
        } else if (!result) result = rf_materials_open(&materials, &geometry, archives, opened, 4*1024*1024);
        if(!result && door_motion)world_vertices=mesh.count;
        if(!result && scene_mode && !door_motion) {
            world_vertices=mesh.count;
            if(actor_body) {
                rf_geometry_collision_world collision={0};result=rf_geometry_collision_world_open(&geometry,8*1024*1024,&collision);
                if(!result)result=rf_scene_stream_miner_body(&level,(int32_t)strtol(argv[8],NULL,10),argv[5],argv[6],argv[7],
                    archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,scene_last,NULL,&collision,&geometry);
                rf_geometry_collision_world_close(&collision);
            }
            else if(scene_states)result=rf_scene_stream_miner_states(&level,(int32_t)strtol(argv[8],NULL,10),argv[5],argv[6],argv[7],
                archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,scene_last,NULL);
            else if(scene_stream)result=rf_scene_stream_miner(&level,(int32_t)strtol(argv[8],NULL,10),argv[5],argv[6],argv[7],
                archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024,scene_last,NULL);
            else result=rf_scene_preview_miner(&level,(int32_t)strtol(argv[8],NULL,10),argv[5],argv[6],argv[7],
                archives,opened,&mesh,&materials,8*1024*1024,4*1024*1024);
            if(!result)printf("Combined %u world and %u actor triangles\n",world_vertices/3,(mesh.count-world_vertices)/3);
        }
        while (opened) rf_vpp_close(archives + --opened);
        if (result) { rf_preview_close(&mesh); rf_geometry_close(&geometry); rf_vpp_close(&archive); return 1; }
        if (!model_mode && rf_lightmaps_open(&lightmaps, &level, 4*1024*1024)) return 1;
    }
    depth = malloc(640*480*sizeof(float)); rgb = malloc(640*480*3);
    if (!depth || !rgb) return 1;
    for (i = 0; i < 640*480; ++i) { depth[i] = 16777216; rgb[i*3] = 16; rgb[i*3+1] = 16; rgb[i*3+2] = 24; }
    for (i = 0; i + 2 < mesh.count; i += 3) {
        int actor_triangle=model_mode || (scene_mode && i>=world_vertices);
        const rf_preview_vertex *a = mesh.vertices+i, *b = a+1, *c = a+2;
        float area = edge(a->position,b->position,c->position[0],c->position[1]);
        int x, y, xmin, xmax, ymin, ymax;
        if (fabsf(area) < 0.00001f) continue;
        xmin = (int)floorf(fminf(a->position[0],fminf(b->position[0],c->position[0])));
        xmax = (int)ceilf(fmaxf(a->position[0],fmaxf(b->position[0],c->position[0])));
        ymin = (int)floorf(fminf(a->position[1],fminf(b->position[1],c->position[1])));
        ymax = (int)ceilf(fmaxf(a->position[1],fmaxf(b->position[1],c->position[1])));
        if (xmin < 0) xmin = 0; if (xmax > 639) xmax = 639;
        if (ymin < 0) ymin = 0; if (ymax > 479) ymax = 479;
        for (y = ymin; y <= ymax; ++y) for (x = xmin; x <= xmax; ++x) {
            float u = edge(b->position,c->position,x+0.5f,y+0.5f)/area;
            float v = edge(c->position,a->position,x+0.5f,y+0.5f)/area;
            float w = 1-u-v, z;
            uint32_t pixel = (uint32_t)(y*640+x), channel;
            if (u < 0 || v < 0 || w < 0) continue;
            z = u*a->position[2]+v*b->position[2]+w*c->position[2];
            if (z >= depth[pixel]) continue;
            if(!actor_triangle)depth[pixel] = z;
            if(!actor_triangle || !materials.count)for (channel = 0; channel < 3; ++channel) rgb[pixel*3+channel] = (unsigned char)(a->color[channel]*255);
            if (materials.count) {
                const rf_image *image = a->material < materials.count && materials.items[a->material].status == RF_OK ? &materials.items[a->material].image : NULL;
                float q = u*a->texture[2]+v*b->texture[2]+w*c->texture[2];
                float s = (u*a->texture[0]+v*b->texture[0]+w*c->texture[0])/q;
                float t = (u*a->texture[1]+v*b->texture[1]+w*c->texture[1])/q;
                float base[4]={1,1,1,1}, light[4] = {0.5f, 0.5f, 0.5f,1};
                if (image) sample(image, s, t, 0, base);
                else for (channel = 0; channel < 3; ++channel) base[channel] = a->color[channel];
                if (a->lightmap < lightmaps.count) {
                    float ls = (u*a->lightmap_texture[0]+v*b->lightmap_texture[0]+w*c->lightmap_texture[0])/q;
                    float lt = (u*a->lightmap_texture[1]+v*b->lightmap_texture[1]+w*c->lightmap_texture[1])/q;
                    sample(lightmaps.images+a->lightmap, ls, lt, 1, light);
                }
                if(actor_triangle && base[3]>=1)depth[pixel]=z;
                for (channel = 0; channel < 3; ++channel) {
                    float color=fminf(1,base[channel]*light[channel]*2)*255;
                    if(actor_triangle)color=color*base[3]+rgb[pixel*3+channel]*(1-base[3]);
                    rgb[pixel*3+channel]=(unsigned char)floorf(color+0.5f);
                }
            }
        }
    }
    output = fopen(output_path,"wb");
    if (!output) return 1;
    fprintf(output,"P6\n640 480\n255\n");
    if (fwrite(rgb,3,640*480,output) != 640*480 || fclose(output)) return 1;
    printf("Prepared %u triangles (%u bytes), %s\n",mesh.count/3,mesh.bytes,scene_close?"close level/actor inspection":model_mode?"posed miner inspection":"original spawn");
    free(depth); free(rgb); rf_lightmaps_close(&lightmaps); rf_materials_close(&materials); rf_preview_close(&mesh); rf_geometry_close(&geometry); rf_vpp_close(&archive);
    return 0;
}
