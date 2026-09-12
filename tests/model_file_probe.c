#include "rf/model_file.h"
#include "rf/model.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
static uint32_t hash_bytes(uint32_t hash,const void *data,uint32_t size)
{
    const unsigned char *bytes=data;uint32_t i;
    for(i=0;i<size;++i)hash=(hash^bytes[i])*16777619u;return hash;
}
int main(int argc, char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--sound-follow")) {
        int32_t index,status;float pose[4][12],orientation[9],position[3],out[3];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&index,4,1,stdin)==1) {
            if(fread(pose,sizeof(pose),1,stdin)!=1 || fread(orientation,36,1,stdin)!=1 || fread(position,12,1,stdin)!=1)return 2;
            memset(out,0xa5,12);status=rf_model_object_follow_point(pose,4,index,orientation,position,out);
            fwrite(&status,4,1,stdout);fwrite(out,12,1,stdout);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-spheres")) {
        uint32_t counts[3];float position[3],matrices[4][12],radius;
        rf_model_collision_sphere models[8];rf_physics_sphere spheres[12];rf_physics_bounds bounds;int status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(counts,sizeof(counts),1,stdin)==1) {
            if(counts[0]>8 || counts[1]>12 || counts[2]>4 || fread(position,12,1,stdin)!=1 ||
               fread(models,sizeof(*models),counts[0],stdin)!=counts[0] || fread(spheres,sizeof(*spheres),counts[1],stdin)!=counts[1] ||
               fread(matrices,48,counts[2],stdin)!=counts[2])return 2;
            memset(&bounds,0xa5,sizeof(bounds));memset(&radius,0xa5,4);
            status=rf_model_corpse_spheres_refresh(models,counts[0],matrices,counts[2],spheres,counts[1],position,&bounds,&radius);
            fwrite(&status,4,1,stdout);fwrite(spheres,sizeof(*spheres),counts[1],stdout);fwrite(&bounds,sizeof(bounds),1,stdout);fwrite(&radius,4,1,stdout);
        }
        return ferror(stdin)?1:0;
    }
    rf_vpp archive;
    rf_model_file model;
    uint32_t i;
    int result;
    if(argc==4 && !strcmp(argv[1],"--bound-sphere")) {
        float values[5];
        if(rf_vpp_open(&archive,argv[2]) || rf_model_file_open(&model,&archive,argv[3]))return 2;
        result=rf_model_file_bound_sphere(&model,values);
        if(!result)result=rf_model_origin_radius(values,values+4);
        rf_vpp_close(&archive);if(result)return 3;
        _setmode(_fileno(stdout),_O_BINARY);return fwrite(values,sizeof(values),1,stdout)==1?0:4;
    }
    if(argc==2 && !strcmp(argv[1],"--bone-query")) {
        uint32_t count;int32_t index,status;float pose[256][12];rf_model_bone_query out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&count,4,1,stdin)==1) {
            if(count>256 || fread(&index,4,1,stdin)!=1 || fread(pose,48,count,stdin)!=count)return 2;
            memset(&out,0xa5,sizeof(out));status=rf_model_query_bone(pose,count,index,&out);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&out,48,1,stdout)!=1)return 3;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sphere-pose")) {
        uint8_t raw[44];float matrices[256][12];uint32_t count;rf_model_collision_sphere sphere={0};
        struct {float value[4];int32_t status;} output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(raw,sizeof(raw),1,stdin)==1) {
            if(fread(&count,4,1,stdin)!=1 || count>256 || fread(matrices,48,count,stdin)!=count)return 2;
            memcpy(sphere.name,raw,24);memcpy(&sphere.parent,raw+24,4);memcpy(sphere.center,raw+28,12);memcpy(&sphere.radius,raw+40,4);
            memset(&output,0xa5,sizeof(output));output.status=rf_model_collision_sphere_pose(&sphere,matrices,count,output.value);
            if(fwrite(&output,sizeof(output),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?1:0;
    }
    if ((argc != 3 && argc != 4) || rf_vpp_open(&archive, argv[1])) return 2;
    result = rf_model_file_open(&model, &archive, argv[2]);
    if(!result && argc==4 && !strcmp(argv[3],"--spheres")) {
        rf_model_collision_sphere sphere,before;uint32_t index=0,j;int status;
        for(;;) {
            memset(&sphere,0xa5,sizeof(sphere));before=sphere;
            status=rf_model_file_collision_sphere(&model,index,&sphere);
            if(status==RF_NOT_FOUND) {if(memcmp(&sphere,&before,sizeof(sphere)))result=RF_FORMAT;break;}
            if(status) {result=status;break;}
            for(j=0;j<24;++j)printf("%02x",(unsigned char)sphere.name[j]);
            for(j=0;j<5;++j) {
                uint32_t bits;const void *field=j==0?(const void*)&sphere.parent:j<4?(const void*)&sphere.center[j-1]:(const void*)&sphere.radius;
                memcpy(&bits,field,4);printf("%08x",bits);
            }
            printf("\n");++index;
        }
        rf_vpp_close(&archive);return result?3:0;
    }
    if(!result && argc==4 && !strcmp(argv[3],"--render-geometry")) {
        float matrices[256][12]={{0}};uint32_t bone,b;
        rf_model_projection view={0};rf_model_lighting lights={0};rf_model_render_output output={1,{255,255,255},255,1,1};
        for(bone=0;bone<256;++bone)matrices[bone][0]=matrices[bone][4]=matrices[bone][8]=1;
        view.camera[2]=-100;view.rotation[0]=view.rotation[4]=view.rotation[8]=1;
        view.perspective=1;view.compute_clip=1;view.clipping=1;
        view.screen[0]=320;view.screen[1]=-240;view.screen[2]=320;view.screen[3]=240;
        lights.ambient[0]=40;lights.ambient[1]=50;lights.ambient[2]=60;
        for(i=0;i<model.lod_count && !result;++i) {
            rf_model_geometry g={0};result=rf_model_geometry_open(&g,&model,i,4*1024*1024);if(result)break;
            for(b=0;b<g.batch_count && !result;++b) {
                rf_model_draw_batch *draw=g.batches+b;uint32_t n,count=draw->vertices,fresh=0,reused=0;
                uint8_t *memory;rf_model_render_buffers buffers;
                if(count>65535) {result=RF_RANGE;break;}
                memory=malloc(count?count*96:1);if(!memory) {result=RF_IO;break;}
                memset(memory,0xa5,count*96);
                buffers.cache=(rf_model_render_cache*)memory;buffers.clip=(float(*)[3])(memory+count*32);
                buffers.second=(float(*)[3])(memory+count*44);buffers.vertices=(uint8_t(*)[40])(memory+count*56);buffers.capacity=count;
                result=rf_model_geometry_render_batch(&g,b,matrices,256,&view,&lights,&output,&buffers);
                for(n=0;n<count && !result;++n) {
                    uint32_t index=draw->first_vertex+n;int32_t distance=g.reuse[index];
                    uint8_t *v=buffers.vertices[n];
                    if(distance>0) {
                        ++reused;
                        if(memcmp(buffers.cache[n].world,buffers.cache[n-distance].world,12) || buffers.cache[n].clip!=buffers.cache[n-distance].clip)result=RF_FORMAT;
                        if(buffers.cache[n].clip)continue;
                    } else ++fresh;
                    if(memcmp(v+24,g.vertices[index].uv,8) || v[16]!=60 || v[17]!=50 || v[18]!=40 || v[19]!=255)result=RF_FORMAT;
                    if(v[20]!=0xa5 || v[21]!=0xa5 || v[22]!=0xa5 || v[32]!=0xa5 || v[39]!=0xa5)result=RF_FORMAT;
                }
                if(!result)printf("R %u %u %u %u %u\n",i,b,count,fresh,reused);
                free(memory);
            }
            rf_model_geometry_close(&g);
        }
        rf_vpp_close(&archive);return result?3:0;
    }
    if(!result && argc==4 && !strcmp(argv[3],"--lod-selection")) {
        uint32_t submesh,mode;const double metrics[]={0,10,100,1000,1000000};
        for(i=0;i<model.lod_count;++i) {
            uint32_t bits;memcpy(&bits,&model.lods[i].threshold,4);
            printf("T %u %u\n",i,bits);
        }
        for(submesh=0;submesh<model.submeshes && !result;++submesh)
            for(mode=0;mode<5 && !result;++mode)for(i=0;i<5 && !result;++i) {
                uint32_t selected;rf_model_geometry g={0};double metric;
                const float camera[3]={0,0,0};float position[3]={(float)metrics[i],0,0};
                result=rf_model_lod_metric(0x66,position,camera,1,1,&metric);if(result)break;
                result=rf_model_file_select_lod(&model,submesh,mode==1?9:0,mode==2,mode==3?1:0,mode==4,1,metric,&selected);
                if(result)break;
                result=rf_model_geometry_open(&g,&model,selected,4*1024*1024);if(result)break;
                printf("S %u %u %u %u %u %u\n",submesh,mode,i,selected,g.vertex_count,g.triangle_count);
                rf_model_geometry_close(&g);
            }
        i=99;
        if(rf_model_file_select_lod(&model,model.submeshes,0,0,0,0,0,0,&i)!=RF_RANGE || i!=99)result=RF_FORMAT;
        rf_vpp_close(&archive);return result?3:0;
    }
    if(!result && argc==4 && !strcmp(argv[3],"--geometry")) {
        uint32_t n;
        for(i=0;i<model.lod_count && !result;++i) {
            rf_model_geometry g={0};uint32_t budget;
            result=rf_model_geometry_open(&g,&model,i,4*1024*1024);if(result)break;
            budget=g.accounted_bytes;rf_model_geometry_close(&g);
            if(rf_model_geometry_open(&g,&model,i,budget-1)!=RF_RANGE || g.vertices || g.batches || g.accounted_bytes) { result=RF_FORMAT;break; }
            result=rf_model_geometry_open(&g,&model,i,budget);if(result)break;
            printf("G %u %u %u %u %u\n",i,g.batch_count,g.vertex_count,g.triangle_count,g.accounted_bytes);
            for(n=0;n<g.batch_count;++n) {
                rf_model_draw_batch *b=g.batches+n;
                printf("V %u %u %u %u %u\n",i,n,
                    hash_bytes(2166136261u,g.vertices+b->first_vertex,b->vertices*40),
                    hash_bytes(2166136261u,g.triangles+b->first_triangle,b->triangles*8),
                    hash_bytes(2166136261u,g.reuse+b->first_vertex,b->vertices*4));
                printf("M %u %u %u\n",i,n,b->material);
            }
            rf_model_geometry_close(&g);rf_model_geometry_close(&g);
        }
        rf_vpp_close(&archive);return result?3:0;
    }
    if(!result && argc==4 && !strcmp(argv[3],"--vertices")) {
        uint32_t b,n;
        _Static_assert(sizeof(rf_model_vertex)==40,"Vertex probe layout");
        _Static_assert(sizeof(rf_model_triangle)==8,"Triangle probe layout");
        for(i=0;i<model.lod_count && !result;++i)for(b=0;b<model.lods[i].batch_count && !result;++b) {
            rf_model_batch batch;rf_model_vertex v,before;rf_model_triangle t,tbefore;
            uint32_t vh=2166136261u,th=2166136261u,rh=2166136261u;int32_t reuse=99;
            result=rf_model_file_batch(&model,i,b,&batch);if(result)break;
            for(n=0;n<batch.vertices && !result;++n) {
                result=rf_model_file_vertex(&model,&batch,n,&v);if(!result)vh=hash_bytes(vh,&v,sizeof(v));
                if(!result)result=rf_model_file_vertex_reuse(&model,&batch,n,&reuse);
                if(!result)rh=hash_bytes(rh,&reuse,4);
            }
            for(n=0;n<batch.triangles && !result;++n) {
                result=rf_model_file_triangle(&model,&batch,n,&t);if(!result)th=hash_bytes(th,&t,sizeof(t));
            }
            memset(&v,0xa5,sizeof(v));before=v;memset(&t,0xa5,sizeof(t));tbefore=t;
            if(rf_model_file_vertex(&model,&batch,batch.vertices,&v)!=RF_RANGE || memcmp(&v,&before,sizeof(v)))result=RF_FORMAT;
            if(rf_model_file_triangle(&model,&batch,batch.triangles,&t)!=RF_RANGE || memcmp(&t,&tbefore,sizeof(t)))result=RF_FORMAT;
            reuse=99;
            if(rf_model_file_vertex_reuse(&model,&batch,batch.vertices,&reuse)!=RF_RANGE || reuse!=99)result=RF_FORMAT;
            if(batch.vertices) {
                uint32_t saved=batch.sizes[5];batch.sizes[5]=0;
                if(rf_model_file_vertex_reuse(&model,&batch,0,&reuse)!=RF_FORMAT || reuse!=99)result=RF_FORMAT;
                batch.sizes[5]=saved;
            }
            batch.format_bits=0;
            if(batch.vertices && (rf_model_file_vertex(&model,&batch,0,&v)!=RF_FORMAT || memcmp(&v,&before,sizeof(v))))result=RF_FORMAT;
            if(batch.triangles && (rf_model_file_triangle(&model,&batch,0,&t)!=RF_FORMAT || memcmp(&t,&tbefore,sizeof(t))))result=RF_FORMAT;
            if(!result)printf("V %u %u %u %u %u\n",i,b,vh,th,rh);
        }
        rf_vpp_close(&archive);return result?3:0;
    }
    if (!result) for (i = 0; i < model.section_count; ++i)
        printf("%u %u %u\n", model.sections[i].type, model.sections[i].offset, model.sections[i].size);
    if (!result && argc==4 && !strcmp(argv[3],"--materials")) {
        uint32_t mesh=0,n,j;
        for (i=0;i<model.section_count && !result;++i) if (model.sections[i].type==0x5355424d) {
            for (n=0;n<model.sections[i].material_count && !result;++n) {
                uint8_t raw[84]; result=rf_model_file_material(&model,mesh,n,raw);
                if (!result) { printf("M %u %u ",mesh,n);for(j=0;j<84;++j) printf("%02x",raw[j]);printf("\n"); }
            }
            { uint8_t raw[84],before[84];memset(raw,0xa5,84);memcpy(before,raw,84);
              if (rf_model_file_material(&model,mesh,n,raw)!=RF_RANGE || memcmp(raw,before,84)) result=RF_FORMAT; }
            ++mesh;
        }
    }
    if(!result && argc==4 && !strcmp(argv[3],"--collision-geometry")) {
        for(i=0;i<model.lod_count && !result;++i) {
            rf_model_collision_geometry g={0};uint32_t budget,b;
            if(!(model.lods[i].flags&32u)) {
                if(rf_model_collision_geometry_open(&g,&model,i,4*1024*1024)!=RF_NOT_FOUND || g.data || g.view.batches || g.accounted_bytes)result=RF_FORMAT;
                continue;
            }
            result=rf_model_collision_geometry_open(&g,&model,i,4*1024*1024);if(result)break;
            budget=g.accounted_bytes;rf_model_collision_geometry_close(&g);
            if(rf_model_collision_geometry_open(&g,&model,i,budget-1)!=RF_RANGE || g.data || g.view.batches || g.accounted_bytes){result=RF_FORMAT;break;}
            result=rf_model_collision_geometry_open(&g,&model,i,budget);if(result)break;
            printf("C %u %u %u\n",i,g.view.batch_count,budget);
            for(b=0;b<g.view.batch_count;++b) {
                const rf_collision_model_batch_view *view=g.view.batches+b;rf_model_batch batch;
                result=rf_model_file_batch(&model,i,b,&batch);if(result)break;
                printf("D %u %u %u %u %u %u %u\n",i,b,view->token_base,view->triangle_count,
                    hash_bytes(2166136261u,view->vertices,batch.vertices*12),hash_bytes(2166136261u,view->planes,batch.triangles*16),hash_bytes(2166136261u,view->triangles,batch.triangles*8));
            }
            rf_model_collision_geometry_close(&g);rf_model_collision_geometry_close(&g);
            if(g.data || g.view.batches || g.accounted_bytes)result=RF_FORMAT;
            if(model.lods[i].batch_count) {
                uint32_t saved=model.lods[i].attachment_offset;model.lods[i].attachment_offset=model.lods[i].offset;
                if(rf_model_collision_geometry_open(&g,&model,i,budget)!=RF_FORMAT || g.data || g.view.batches || g.accounted_bytes)result=RF_FORMAT;
                model.lods[i].attachment_offset=saved;
            }
        }
    }
    if(!result && argc==4 && !strcmp(argv[3],"--planes")) {
        uint32_t b,n;
        for(i=0;i<model.lod_count && !result;++i)for(b=0;b<model.lods[i].batch_count && !result;++b) {
            rf_model_batch batch;float plane[4],before[4];uint32_t hash=2166136261u,count=0;
            result=rf_model_file_batch(&model,i,b,&batch);if(result)break;
            memset(before,0xa5,16);
            for(n=0;n<batch.triangles;++n) {
                int status;memcpy(plane,before,16);status=rf_model_file_triangle_plane(&model,&batch,n,plane);
                if(!batch.sizes[4]) {if(status!=RF_NOT_FOUND || memcmp(plane,before,16))result=RF_FORMAT;}
                else if(status)result=status;else {hash=hash_bytes(hash,plane,16);++count;}
                if(result)break;
            }
            memcpy(plane,before,16);
            if(rf_model_file_triangle_plane(&model,&batch,batch.triangles,plane)!=RF_RANGE || memcmp(plane,before,16))result=RF_FORMAT;
            if(batch.sizes[4] && batch.triangles) {
                batch.sizes[4]=15;
                if(rf_model_file_triangle_plane(&model,&batch,0,plane)!=RF_FORMAT || memcmp(plane,before,16))result=RF_FORMAT;
            }
            printf("P %u %u %u %u\n",i,b,count,hash);
        }
    }
    if (!result && argc==4 && !strcmp(argv[3],"--batches")) {
        uint32_t n,j;
        for(i=0;i<model.lod_count && !result;++i) {
            for(n=0;n<model.lods[i].batch_count && !result;++n) {
                rf_model_batch b;uint32_t material;result=rf_model_file_batch(&model,i,n,&b);
                if(!result)result=rf_model_file_batch_material(&model,i,n,&material);
                if(!result) {
                    printf("B %u %u %u %u %u",i,n,b.vertices,b.triangles,b.format_bits);
                    for(j=0;j<8;++j)printf(" %u %u",b.offsets[j],b.sizes[j]);printf(" %u\n",material);
                }
            }
            { rf_model_batch b,before;memset(&b,0xa5,sizeof(b));before=b;
              if(rf_model_file_batch(&model,i,n,&b)!=RF_RANGE || memcmp(&b,&before,sizeof(b)))result=RF_FORMAT; }
            if(model.lods[i].batch_count) {
                rf_model_lod saved=model.lods[i];rf_model_batch b,before;
                memset(&b,0xa5,sizeof(b));before=b;
                model.lods[i].batch_offset=model.entry.size;
                if(rf_model_file_batch(&model,i,0,&b)!=RF_RANGE || memcmp(&b,&before,sizeof(b)))result=RF_FORMAT;
                model.lods[i]=saved;model.lods[i].attachment_offset=saved.offset;
                if(rf_model_file_batch(&model,i,0,&b)!=RF_FORMAT || memcmp(&b,&before,sizeof(b)))result=RF_FORMAT;
                model.lods[i]=saved;
            }
        }
    }
    if (!result && argc == 4 && strcmp(argv[3],"--materials") && strcmp(argv[3],"--batches") && strcmp(argv[3],"--planes") && strcmp(argv[3],"--collision-geometry")) for (i = 0; i < model.lod_count && !result; ++i) {
        uint32_t n, j;
        rf_model_lod *lod = &model.lods[i];
        printf("L %u %u %u %u\n", lod->offset, lod->size, lod->attachment_offset, lod->attachment_count);
        for (n = 0; n < lod->attachment_count; ++n) {
            rf_model_attachment a;
            result = rf_model_file_attachment(&model, i, n, &a);
            if (result) break;
            printf("A ");
            for (j = 0; j < 68; ++j) printf("%02x", (unsigned char)a.name[j]);
            for (j = 0; j < 8; ++j) {
                uint32_t bits;
                const void *field = j < 4 ? (const void *)&a.rotation[j] : j < 7 ? (const void *)&a.position[j-4] : (const void *)&a.parent;
                memcpy(&bits, field, 4); printf("%08x", bits);
            }
            printf("\n");
        }
    }
    rf_vpp_close(&archive);
    return result ? 3 : 0;
}
