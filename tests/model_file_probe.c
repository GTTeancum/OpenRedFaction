#include "rf/model_file.h"
#include <string.h>
static uint32_t hash_bytes(uint32_t hash,const void *data,uint32_t size)
{
    const unsigned char *bytes=data;uint32_t i;
    for(i=0;i<size;++i)hash=(hash^bytes[i])*16777619u;return hash;
}
int main(int argc, char **argv)
{
    rf_vpp archive;
    rf_model_file model;
    uint32_t i;
    int result;
    if ((argc != 3 && argc != 4) || rf_vpp_open(&archive, argv[1])) return 2;
    result = rf_model_file_open(&model, &archive, argv[2]);
    if(!result && argc==4 && !strcmp(argv[3],"--lod-selection")) {
        uint32_t submesh,mode;const double metrics[]={0,10,100,1000,1000000};
        for(i=0;i<model.lod_count;++i) {
            uint32_t bits;memcpy(&bits,&model.lods[i].threshold,4);
            printf("T %u %u\n",i,bits);
        }
        for(submesh=0;submesh<model.submeshes && !result;++submesh)
            for(mode=0;mode<5 && !result;++mode)for(i=0;i<5 && !result;++i) {
                uint32_t selected;rf_model_geometry g={0};
                result=rf_model_file_select_lod(&model,submesh,mode==1?9:0,mode==2,mode==3?1:0,mode==4,1,metrics[i],&selected);
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
    if (!result && argc == 4 && strcmp(argv[3],"--materials") && strcmp(argv[3],"--batches")) for (i = 0; i < model.lod_count && !result; ++i) {
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
