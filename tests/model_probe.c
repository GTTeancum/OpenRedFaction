#include "rf/model.h"
#include "rf/model_file.h"
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <math.h>
int main(int argc,char **argv)
{
    struct { uint32_t counts[3]; char names[3][16][32]; char query[32]; } input;
    struct { int32_t status, index; } output;
    rf_model_name names[3][16];
    rf_model_name_group groups[3];
    uint32_t g, n;
    _Static_assert(sizeof(input) == 1580, "Probe wire layout");
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--register-motion")) {
        struct {uint32_t count,capacity,identity,flag,identities[32];uint8_t flags[32];} data;
        struct {int32_t status,index,added;uint32_t count,identities[32];uint8_t flags[32];} result;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_motion_registry registry={data.identities,data.flags,data.count,data.capacity};
            if(data.capacity>32 && data.capacity!=UINT32_MAX)return 2;
            result.index=result.added=-12345;
            result.status=rf_model_register_motion(&registry,data.identity,(uint8_t)data.flag,&result.index,&result.added);
            result.count=registry.count;memcpy(result.identities,data.identities,128);memcpy(result.flags,data.flags,32);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--near-clip-guards")) {
        rf_model_vertex vertices[3]={0};int32_t reuse[3]={0};
        rf_model_draw_batch batch={0,3,0,1,0};rf_model_triangle triangle={{0,1,2},0x20};
        rf_model_geometry geometry={&batch,vertices,&triangle,reuse,1,3,1,0};
        rf_model_render_cache cache[3]={0},saved[3];float clip[3][3]={{0,0,-.125f},{-.125f,0,.5f},{.125f,.125f,.5f}};
        uint8_t emitted[16][40]={0};uint16_t indices[32];rf_model_clip_pool pool;
        rf_model_render_buffers buffers={cache,clip,NULL,emitted,3};
        rf_model_projection view={0};rf_model_clip_planes planes={.125f,1000,{0},{0}};
        rf_model_clip_projection projection={{320,240},{0,0},0,1};
        rf_model_render_output attributes={0,{255,255,255},255,1,1};
        rf_model_triangle_output result={emitted,indices,3,16,0,32};
        view.perspective=view.compute_clip=view.clipping=1;view.rotation[0]=view.rotation[4]=view.rotation[8]=1;
        view.screen[0]=320;view.screen[1]=-240;view.screen[2]=320;view.screen[3]=240;
        for(n=0;n<3;++n) {float position[3];uint32_t visible;memcpy(position,clip[n],12);
            if(rf_model_project_vertex(position,&view,cache+n,clip[n],emitted[n],&visible))return 3;}
        if(rf_model_geometry_clip_near(&geometry,0,&buffers,.125f) || !(cache[0].clip&1) || (cache[0].clip&128))return 3;
        if(rf_model_geometry_emit_batch(&geometry,0,&buffers,&view,&planes,&projection,&attributes,0,&pool,&result))return 3;
        if(result.vertex_count!=5 || result.index_count!=6)return 3;
        for(n=3;n<5;++n) {float q;memcpy(&q,emitted[n]+12,4);if(q!=8)return 3;}
        memcpy(saved,cache,sizeof(cache));
        if(rf_model_geometry_clip_near(&geometry,0,&buffers,NAN)!=RF_RANGE || memcmp(saved,cache,sizeof(cache)))return 3;
        clip[2][2]=NAN;
        if(rf_model_geometry_clip_near(&geometry,0,&buffers,.125f)!=RF_FORMAT || memcmp(saved,cache,sizeof(cache)))return 3;
        reuse[2]=1;
        if(rf_model_geometry_clip_near(&geometry,0,&buffers,.125f) || cache[2].clip!=cache[1].clip)return 3;
        memcpy(saved,cache,sizeof(cache));reuse[0]=1;
        if(rf_model_geometry_clip_near(&geometry,0,&buffers,.125f)!=RF_RANGE || memcmp(saved,cache,sizeof(cache)))return 3;
        puts("PASS: near crossing emits two intersections and six indices; reuse and failure preservation");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--local-view")) {
        struct {rf_model_projection view;float position[3],orientation[9];} data;
        _Static_assert(sizeof(data)==160,"local view probe layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            if(rf_model_local_view(&data.view,data.position,data.orientation,&data.view))return 2;
            if(fwrite(&data.view,sizeof(data.view),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--batch-triangles")) {
        const float positions[9][3]={{0,0,1},{.5f,0,1},{0,.5f,1},{-2,0,1},{0,.5f,1},{0,-.5f,1},{-3,0,1},{-2,.5f,1},{-2,-.5f,1}};
        const uint16_t expected[9]={0,1,2,4,5,9,4,9,10};
        rf_model_vertex vertices[9]={0};int32_t reuse[9]={0};rf_model_render_cache cache[9]={0};float clip[9][3],second[9][3];
        uint8_t gpu[64][40];uint16_t indices[128];rf_model_triangle triangles[3]={{{0,1,2},32},{{3,4,5},32},{{6,7,8},32}};
        rf_model_draw_batch draw={0,9,0,3,0};rf_model_geometry geometry={&draw,vertices,triangles,reuse,1,9,3,0};
        rf_model_render_buffers buffers={cache,clip,second,gpu,9};rf_model_projection view={0};rf_model_clip_planes planes={0};
        rf_model_clip_projection projection={{320,240},{0,0},0,1};rf_model_render_output attributes={0,{40,50,60},255,1,1};
        rf_model_clip_pool pool={0};rf_model_triangle_output out={gpu,indices,9,64,0,128};uint32_t i;
        view.perspective=view.compute_clip=view.clipping=1;
        memset(gpu,0xa5,sizeof(gpu));memset(indices,0xa5,sizeof(indices));
        for(i=0;i<9;++i) {memcpy(cache[i].world,positions[i],12);memcpy(clip[i],positions[i],12);cache[i].clip=positions[i][0]<-1?4:0;}
        if(rf_model_geometry_emit_batch(&geometry,0,&buffers,&view,&planes,&projection,&attributes,0,&pool,&out) ||
            out.vertex_count!=11 || out.index_count!=9 || memcmp(indices,expected,sizeof(expected)))return 1;
        for(i=9;i<11;++i) {
            float xy[2];memcpy(xy,gpu[i],8);
            if(xy[0]!=0 || xy[1]!=(i==9?300:180) || gpu[i][16]!=60 || gpu[i][17]!=50 || gpu[i][18]!=40 || gpu[i][19]!=255 || gpu[i][32]!=0xa5)return 1;
        }
        out.vertex_count=9;out.index_count=0;triangles[2].indices[2]=9;
        if(rf_model_geometry_emit_batch(&geometry,0,&buffers,&view,&planes,&projection,&attributes,0,&pool,&out)!=RF_RANGE || out.index_count)return 1;
        triangles[2].indices[2]=8;draw.triangles=1;out.index_capacity=3;
        if(rf_model_geometry_emit_batch(&geometry,0,&buffers,&view,&planes,&projection,&attributes,0,&pool,&out)!=RF_RANGE || out.index_count)return 1;
        view.compute_clip=0;
        if(rf_model_geometry_emit_batch(&geometry,0,&buffers,&view,&planes,&projection,&attributes,0,&pool,&out) || out.index_count!=3)return 1;
        puts("PASS: mixed direct/clipped/rejected batch, generated coordinates, source preflight and strict direct capacity gates");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--emit-clip-guards")) {
        uint8_t records[3][48],saved[3][48],vertices[8][40],before[8][40],*pointers[3];uint16_t indices[16],old_indices[16],triangle[3]={0,1,2};
        rf_model_clip_projection view={{320,240},{0,0},0,1};rf_model_render_output attributes={0,{40,50,60},255,1,1};
        rf_model_triangle_output out={vertices,indices,3,6,0,16};float position[3]={0,0,1};unsigned i;
        memset(records,0xa5,sizeof(records));memset(vertices,0xa5,sizeof(vertices));memset(indices,0xa5,sizeof(indices));
        for(i=0;i<3;++i) {pointers[i]=records[i];memcpy(records[i],position,12);records[i][25]=4;records[i][26]=(uint8_t)i;}
        memcpy(saved,records,sizeof(saved));memcpy(before,vertices,sizeof(before));memcpy(old_indices,indices,sizeof(indices));
        for(i=0;i<4;++i) {
            out.vertex_capacity=i==0?6:8;out.index_capacity=i==1?3:16;
            pointers[1]=i==2?NULL:records[1];records[1][25]=i==3?0:4;records[1][26]=i==3?3:1;
            memcpy(saved,records,sizeof(saved));
            if(rf_model_emit_clip_polygon(pointers,3,0,triangle,0,&view,&attributes,1,&out)!=RF_RANGE || out.vertex_count!=3 || out.index_count ||
                memcmp(saved,records,sizeof(saved)) || memcmp(before,vertices,sizeof(before)) || memcmp(old_indices,indices,sizeof(indices)))return 1;
        }
        if(rf_model_emit_clip_polygon(NULL,3,1,NULL,0,NULL,NULL,1,NULL)!=RF_OK)return 1;
        puts("PASS: 5 clip emission guards");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--emit-clip")) {
        struct {uint8_t records[8][48];rf_model_clip_projection view;rf_model_render_output attributes;
            float depth_factor;uint32_t count;uint16_t triangle[3],base;} data;
        _Static_assert(sizeof(data)==440,"clip emission probe layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            uint8_t vertices[64][40],*records[8];uint16_t indices[144];uint32_t i;
            rf_model_triangle_output out={vertices,indices,5,64,3,144};int32_t status;
            memset(vertices,0xa5,sizeof(vertices));memset(indices,0xa5,sizeof(indices));
            for(i=0;i<8;++i)records[i]=data.records[i];
            status=rf_model_emit_clip_polygon(records,data.count,0,data.triangle,data.base,&data.view,&data.attributes,data.depth_factor,&out);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&out.vertex_count,4,1,stdout)!=1 || fwrite(&out.index_count,4,1,stdout)!=1 ||
                fwrite(data.records,sizeof(data.records),1,stdout)!=1 || fwrite(vertices,sizeof(vertices),1,stdout)!=1 || fwrite(indices,sizeof(indices),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--project-clip")) {
        struct {uint8_t record[48];rf_model_clip_projection view;} data;
        _Static_assert(sizeof(data)==72,"clip projection probe layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            if(rf_model_project_clip_vertex(&data.view,data.record))return 2;
            if(fwrite(data.record,48,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--clip-polygon")) {
        struct {uint8_t records[3][48];rf_model_clip_planes planes;rf_model_projection view;uint32_t mode,attributes;uint8_t masks[4];} data;
        _Static_assert(sizeof(data)==300,"clip polygon probe input layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_clip_pool pool;uint8_t *original[3],*result[48];uint32_t count=0,i,ids[48];int32_t status;
            memset(&pool,0xa5,sizeof(pool));rf_model_clip_pool_reset(&pool);memset(ids,0xff,sizeof(ids));
            for(i=0;i<3;++i)original[i]=data.records[i];
            status=rf_model_clip_polygon(&pool,original,3,&data.planes,&data.view,data.mode,data.attributes,result,&count,data.masks);
            if(!status)for(i=0;i<count;++i) {
                uint32_t j;for(j=0;j<3;++j)if(result[i]==original[j])break;
                if(j==3)for(j=0;j<48;++j)if(result[i]==pool.records[j]) {j+=3;break;}
                ids[i]=j;
            }
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&count,4,1,stdout)!=1 || fwrite(data.masks,2,1,stdout)!=1 || fwrite(ids,192,1,stdout)!=1 ||
                fwrite(&pool.used,4,1,stdout)!=1 || fwrite(pool.order,192,1,stdout)!=1 || fwrite(pool.records,2304,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--clip-pool")) {
        rf_model_clip_pool pool;uint32_t command[2];memset(&pool,0xa5,sizeof(pool));rf_model_clip_pool_reset(&pool);
        while(fread(command,sizeof(command),1,stdin)==1) {
            int32_t status=0;uint32_t slot=99;
            if(command[0]==0)rf_model_clip_pool_reset(&pool);
            else if(command[0]==1)status=rf_model_clip_pool_allocate(&pool,&slot);
            else if(command[0]==2)status=rf_model_clip_pool_release(&pool,command[1]);else return 2;
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&slot,4,1,stdout)!=1 || fwrite(&pool.used,4,1,stdout)!=1 ||
                fwrite(pool.order,sizeof(pool.order),1,stdout)!=1 || fwrite(pool.records,sizeof(pool.records),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--classify-clip")) {
        struct {uint8_t record[48];rf_model_projection view;uint32_t mode;} data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            if(rf_model_classify_clip_vertex(data.mode,&data.view,data.record))return 2;
            if(fwrite(data.record,48,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--clip-intersection")) {
        struct {float inside[3],outside[3];rf_model_clip_planes planes;uint32_t plane;} data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            int32_t status;float position[3]={99,99,99};double factor=99;
            status=rf_model_clip_intersection(data.plane,data.inside,data.outside,&data.planes,position,&factor);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(position,12,1,stdout)!=1 || fwrite(&factor,8,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--clip-attributes")) {
        struct {uint8_t inside[48],outside[48];double factor;uint32_t flags,pad;} data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            int32_t status;uint8_t result[48];memset(result,0xa5,48);
            status=rf_model_clip_attributes(data.inside,data.outside,data.factor,data.flags,result);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(result,48,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--prepare-clip")) {
        struct {rf_model_vertex vertices[4];int32_t reuse[4];rf_model_render_cache cache[4];float clip[4][3];uint16_t indices[3],pad;rf_model_render_output output;} data;
        _Static_assert(sizeof(data)==376,"Clip input wire layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            int32_t status;uint8_t records[3][48];memset(records,0xa5,sizeof(records));
            status=rf_model_prepare_clip_triangle(data.vertices,data.reuse,data.cache,data.clip,4,data.indices,&data.output,records);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(records,sizeof(records),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--triangle-route")) {
        struct {rf_model_render_cache cache[3];rf_model_projection view;uint16_t indices[3],flags;} data;
        _Static_assert(sizeof(data)==216,"Triangle route wire layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            struct {int32_t status;uint32_t route;} result={0,99};
            result.status=rf_model_route_triangle(data.cache,3,data.indices,data.flags,&data.view,&result.route);
            if(fwrite(&result,8,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--triangle-facing")) {
        struct {float vertices[3][3],camera[3],forward[3];uint32_t flags,perspective;} data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            uint32_t accepted;
            if(rf_model_triangle_facing(data.vertices[0],data.vertices[1],data.vertices[2],(uint16_t)data.flags,data.perspective,data.camera,data.forward,&accepted))return 2;
            if(fwrite(&accepted,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-batch")) {
        struct {rf_model_vertex vertices[8];int32_t reuse[8];float matrices[4][12];rf_model_projection view;rf_model_lighting lights;rf_model_render_output output;} data;
        _Static_assert(sizeof(data)==756,"Batch probe wire layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_render_cache cache[8];float clip[8][3],second[8][3];uint8_t vertices[8][40];int32_t status;
            rf_model_draw_batch draw={0,8,0,0,0};rf_model_geometry geometry={0};
            rf_model_render_buffers buffers={cache,clip,second,vertices,8};
            geometry.batches=&draw;geometry.batch_count=1;geometry.vertices=data.vertices;geometry.reuse=data.reuse;geometry.vertex_count=8;
            memset(cache,0xa5,sizeof(cache));memset(clip,0xa5,sizeof(clip));memset(second,0xa5,sizeof(second));memset(vertices,0xa5,sizeof(vertices));
            status=rf_model_geometry_render_batch(&geometry,0,data.matrices,4,&data.view,&data.lights,&data.output,&buffers);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(cache,sizeof(cache),1,stdout)!=1 || fwrite(clip,sizeof(clip),1,stdout)!=1 ||
                fwrite(second,sizeof(second),1,stdout)!=1 || fwrite(vertices,sizeof(vertices),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--project-vertex")) {
        struct {float world[3];rf_model_projection view;} data;
        _Static_assert(sizeof(data)==124,"Projection probe wire layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_render_cache cache;float clip[3];uint8_t vertex[40];uint32_t visible=99;
            memset(&cache,0xa5,sizeof(cache));memset(clip,0xa5,sizeof(clip));memset(vertex,0xa5,sizeof(vertex));
            if(rf_model_project_vertex(data.world,&data.view,&cache,clip,vertex,&visible))return 2;
            if(fwrite(&cache,32,1,stdout)!=1 || fwrite(clip,12,1,stdout)!=1 || fwrite(vertex,40,1,stdout)!=1 || fwrite(&visible,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-reuse")) {
        struct {rf_model_render_cache cache[8];rf_model_render_output output;uint32_t index;int32_t distance;float uv[2];} data;
        _Static_assert(sizeof(data)==288,"Reuse probe wire layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            int32_t status;uint8_t vertex[40];memset(vertex,0xa5,sizeof(vertex));
            status=rf_model_render_reuse_vertex(data.cache,8,data.index,data.distance,&data.output,data.uv,vertex);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(data.cache,sizeof(data.cache),1,stdout)!=1 || fwrite(vertex,40,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-vertex-lighting")) {
        struct {float vector[3],lights[3][6],ambient[3];} data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            float normal[3];uint8_t rgb[3];
            if(rf_model_render_vertex_lighting(data.vector,data.lights,data.ambient,normal,rgb))return 2;
            if(fwrite(normal,sizeof(normal),1,stdout)!=1 || fwrite(rgb,3,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--select-lod-camera")) {
        struct {float thresholds[3];uint32_t count,flags;int32_t alternate,minimum,scaled,animated;
            uint32_t mode;float position[3],camera[3],numerator,denominator;} data;
        _Static_assert(sizeof(data)==72,"Camera LOD probe wire layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            struct {int32_t status;uint32_t index;double metric;} result={0,99,0};
            result.status=rf_model_lod_metric(data.mode,data.position,data.camera,data.numerator,data.denominator,&result.metric);
            if(!result.status)result.status=rf_model_select_lod(data.thresholds,data.count,data.flags,data.alternate,data.minimum,data.scaled,data.animated,result.metric,&result.index);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--select-lod")) {
        struct {float thresholds[3];uint32_t count,flags;int32_t alternate,minimum,scaled,animated;float metric;} data;
        _Static_assert(sizeof(data)==40,"LOD probe wire layout");
        while(fread(&data,sizeof(data),1,stdin)==1) {
            struct {int32_t status;uint32_t index;} result={0,99};
            result.status=rf_model_select_lod(data.thresholds,data.count,data.flags,data.alternate,data.minimum,data.scaled,data.animated,data.metric,&result.index);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--prepare-skinning")) {
        struct { float stored[4][12],pose[4][12],prepared[4][12];uint16_t stamps[4],generation,pad; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            if(rf_model_prepare_skinning(data.stored,data.pose,4,data.generation,data.prepared,data.stamps,4)!=RF_OK)return 2;
            if(fwrite(data.prepared,192,1,stdout)!=1 || fwrite(data.stamps,8,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--lighting-setup")) {
        struct { rf_model_lighting_input input;rf_model_local_light lights[8];float colors[8][3]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_lighting out;
            if(rf_model_lighting_setup(&data.input,data.lights,data.colors,8,&out)!=RF_OK)return 2;
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--local-light-direction")) {
        struct { float delta[3];uint32_t flags;float rotation[9]; } data;float out[3];
        while(fread(&data,sizeof(data),1,stdin)==1) {
            if(rf_model_local_light_direction(data.delta,data.flags,data.rotation,out)!=RF_OK)return 2;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--local-light-color")) {
        float data[5],out[3];
        while(fread(data,sizeof(data),1,stdin)==1) {
            if(rf_model_local_light_color(data[0],data[1],data+2,out)!=RF_OK)return 2;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--choose-light")) {
        struct { float position[3];rf_model_local_light lights[8]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_light_choice out;
            if(rf_model_choose_local_light(data.position,data.lights,8,&out)!=RF_OK)return 2;
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--vertex-lighting")) {
        struct { float vector[3],lights[3][6],ambient[3]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            uint8_t rgb[3];int32_t status=rf_model_vertex_lighting(data.vector,data.lights,data.ambient,rgb);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(rgb,3,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--render-vertex-pair")) {
        struct { float position[3],second[3];uint8_t weights[4],bones[4];float matrices[4][12]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            struct { int32_t status;float value[6]; } out={0,{99,99,99,99,99,99}};
            out.status=rf_model_render_vertex_pair(data.position,data.second,data.weights,data.bones,data.matrices,4,out.value);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--collision-vertex")) {
        struct { float position[3];uint8_t weights[4],bones[4];float matrices[4][12]; } data;
        while(fread(&data,sizeof(data),1,stdin)==1) {
            struct { int32_t status;float value[3]; } out={0,{99,99,99}};
            out.status=rf_model_collision_vertex(data.position,data.weights,data.bones,data.matrices,4,out.value);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if (argc==2 && !strcmp(argv[1],"--material-disk")) {
        struct { uint8_t raw[84]; int32_t primary,secondary; uint32_t transparent,budget; } data;
        while (fread(&data,sizeof(data),1,stdin)==1) {
            rf_model_material_instance instance={0}; int32_t status; uint32_t scalar=0;
            status=rf_model_material_from_disk(&instance,data.raw,84,data.primary,data.secondary,data.transparent,data.budget);
            if (instance.counts[1]) scalar=instance.arrays[1][0];
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&instance.record,200,1,stdout)!=1 || fwrite(&scalar,4,1,stdout)!=1) return 1;
            rf_model_material_instance_close(&instance);
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--material-copy")) {
        struct { int32_t kind; rf_model_material_record source,destination; } data;
        while (fread(&data,sizeof(data),1,stdin)==1) {
            uint32_t counts[3]={99,99,99}; int32_t status;
            status=rf_model_material_prepare_copy(&data.destination,&data.source,data.kind,counts);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&data.destination,200,1,stdout)!=1 || fwrite(counts,12,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--material-init")) {
        rf_model_material_record material;
        while (fread(&material,sizeof(material),1,stdin)==1) {
            if (rf_model_material_initialize(&material)!=RF_OK) return 2;
            if (fwrite(&material,sizeof(material),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--materials")) {
        struct { int32_t kind,lods,static_count,mesh_count,direct_count,counts[8]; uint32_t capacity; } data;
        _Static_assert(sizeof(data)==56,"Material count fixture layout");
        while (fread(&data,sizeof(data),1,stdin)==1) {
            int32_t result[2]={0,-99};
            if (data.capacity>8) return 2;
            result[0]=rf_model_material_count(data.kind,data.lods,data.static_count,data.mesh_count,
                data.counts,data.capacity,data.direct_count,&result[1]);
            if (fwrite(result,sizeof(result),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    while (fread(&input, sizeof(input), 1, stdin) == 1) {
        for (g = 0; g < 3; ++g) {
            if (input.counts[g] > 16) return 2;
            groups[g].names = names[g]; groups[g].count = input.counts[g];
            for (n = 0; n < input.counts[g]; ++n) {
                if (!memchr(input.names[g][n], 0, 32)) return 2;
                names[g][n].data = input.names[g][n];
                names[g][n].length = strlen(input.names[g][n]);
            }
        }
        if (!memchr(input.query, 0, 32)) return 2;
        {
            rf_model_name query = {input.query, strlen(input.query)};
            output.index = -1;
            output.status = rf_model_find_tag(groups, query, &output.index);
        }
        if (fwrite(&output, sizeof(output), 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
