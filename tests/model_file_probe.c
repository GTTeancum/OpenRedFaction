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
    if(!result && argc==4 && !strcmp(argv[3],"--vertices")) {
        uint32_t b,n;
        _Static_assert(sizeof(rf_model_vertex)==40,"Vertex probe layout");
        _Static_assert(sizeof(rf_model_triangle)==8,"Triangle probe layout");
        for(i=0;i<model.lod_count && !result;++i)for(b=0;b<model.lods[i].batch_count && !result;++b) {
            rf_model_batch batch;rf_model_vertex v,before;rf_model_triangle t,tbefore;
            uint32_t vh=2166136261u,th=2166136261u;
            result=rf_model_file_batch(&model,i,b,&batch);if(result)break;
            for(n=0;n<batch.vertices && !result;++n) {
                result=rf_model_file_vertex(&model,&batch,n,&v);if(!result)vh=hash_bytes(vh,&v,sizeof(v));
            }
            for(n=0;n<batch.triangles && !result;++n) {
                result=rf_model_file_triangle(&model,&batch,n,&t);if(!result)th=hash_bytes(th,&t,sizeof(t));
            }
            memset(&v,0xa5,sizeof(v));before=v;memset(&t,0xa5,sizeof(t));tbefore=t;
            if(rf_model_file_vertex(&model,&batch,batch.vertices,&v)!=RF_RANGE || memcmp(&v,&before,sizeof(v)))result=RF_FORMAT;
            if(rf_model_file_triangle(&model,&batch,batch.triangles,&t)!=RF_RANGE || memcmp(&t,&tbefore,sizeof(t)))result=RF_FORMAT;
            batch.format_bits=0;
            if(batch.vertices && (rf_model_file_vertex(&model,&batch,0,&v)!=RF_FORMAT || memcmp(&v,&before,sizeof(v))))result=RF_FORMAT;
            if(batch.triangles && (rf_model_file_triangle(&model,&batch,0,&t)!=RF_FORMAT || memcmp(&t,&tbefore,sizeof(t))))result=RF_FORMAT;
            if(!result)printf("V %u %u %u %u\n",i,b,vh,th);
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
                rf_model_batch b;result=rf_model_file_batch(&model,i,n,&b);
                if(!result) {
                    printf("B %u %u %u %u %u",i,n,b.vertices,b.triangles,b.format_bits);
                    for(j=0;j<8;++j)printf(" %u %u",b.offsets[j],b.sizes[j]);printf("\n");
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
