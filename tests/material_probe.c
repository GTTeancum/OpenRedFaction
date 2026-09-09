#include "rf/material.h"
#include "rf/preview.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(int argc, char **argv)
{
    rf_vpp level_archive, archives[16];
    rf_level level;
    rf_geometry geometry;
    rf_materials materials;
    uint32_t count, i;
    int result;
    if(argc>=5 && argc<=21 && !strcmp(argv[1],"--geometry")) {
        rf_geometry world={0};rf_geometry_movers movers={0};
        rf_geometry_materials bundle={0},check={0};const rf_geometry **sources;
        uint32_t g,j,k,hash,peak;
        if(rf_vpp_open(&level_archive,argv[2]))return 1;
        if(rf_level_open(&level,&level_archive,argv[3]) ||
            rf_geometry_open(&world,&level,8*1024*1024) ||
            rf_geometry_movers_open(&level,8*1024*1024,&movers))return 1;
        rf_vpp_close(&level_archive);
        sources=malloc((movers.count+1)*sizeof(*sources));if(!sources)return 1;
        sources[0]=&world;for(g=0;g<movers.count;++g)sources[g+1]=&movers.items[g].geometry;
        count=(uint32_t)argc-5;
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+5]))return 1;
        result=rf_geometry_materials_open(&bundle,sources,movers.count+1,archives,count,
            (uint32_t)strtoul(argv[4],NULL,10));if(result)return 1;
        peak=bundle.peak_bytes;
        if(rf_geometry_materials_open(&check,sources,movers.count+1,archives,count,peak-1)!=RF_RANGE ||
            check.offsets || check.slots || check.textures.items || check.resident_bytes)return 3;
        if(rf_geometry_materials_open(&check,sources,movers.count+1,archives,count,peak) ||
            check.peak_bytes!=peak || check.textures.count!=bundle.textures.count ||
            memcmp(check.slots,bundle.slots,bundle.offsets[bundle.count]*sizeof(uint32_t)))return 3;
        rf_geometry_materials_close(&check);rf_geometry_materials_close(&check);
        {
            rf_preview_mesh combined={0},part={0},short_mesh={0};uint32_t at=0;
            if(rf_preview_build_world(&combined,&world,&movers,NULL,&bundle,&level,8*1024*1024))return 3;
            if(combined.bytes && rf_preview_build_world(&short_mesh,&world,&movers,NULL,&bundle,&level,combined.bytes-1)!=RF_RANGE)return 3;
            if(short_mesh.vertices || short_mesh.bytes)return 3;
            if(rf_preview_build_world(&short_mesh,&world,&movers,NULL,&bundle,&level,combined.bytes) ||
                short_mesh.count!=combined.count || (combined.bytes && memcmp(short_mesh.vertices,combined.vertices,combined.bytes)))return 3;
            rf_preview_close(&short_mesh);
            for(g=0;g<bundle.count;++g) {
                result=g?rf_preview_build_transformed(&part,sources[g],&level,movers.items[g-1].position,
                    movers.items[g-1].orientation,0,8*1024*1024):rf_preview_build(&part,&world,&level,8*1024*1024);
                if(result || part.count>combined.count-at)return 3;
                for(j=0;j<part.count;++j)part.vertices[j].material=bundle.slots[bundle.offsets[g]+part.vertices[j].material];
                if(part.bytes && memcmp(part.vertices,combined.vertices+at,part.bytes))return 3;
                at+=part.count;rf_preview_close(&part);
            }
            if(at!=combined.count)return 3;
            rf_preview_close(&combined);
        }
        {
            rf_group_attached_pose *poses=malloc(movers.count*sizeof(*poses));
            rf_preview_mesh live={0},part={0};void *address;uint32_t frame,at;
            if(movers.count && !poses)return 3;
            live.vertices=malloc(8*1024*1024);address=live.vertices;if(!address)return 3;
            for(frame=0;frame<3;++frame) {
                for(g=0;g<movers.count;++g) {
                    memset(poses+g,0xa5,sizeof(*poses));
                    for(j=0;j<3;++j)poses[g].position[j]=movers.items[g].position[j]+frame*(float)(j+1);
                    memcpy(poses[g].output_matrix,movers.items[g].orientation,36);
                }
                if(rf_preview_update_world(&live,8*1024*1024,&world,&movers,poses,&bundle,&level) || live.vertices!=address)return 3;
                at=0;
                for(g=0;g<bundle.count;++g) {
                    result=g?rf_preview_build_transformed(&part,sources[g],&level,poses[g-1].position,
                        (const float (*)[3])poses[g-1].output_matrix,0,8*1024*1024):rf_preview_build(&part,&world,&level,8*1024*1024);
                    if(result || part.count>live.count-at)return 3;
                    for(j=0;j<part.count;++j)part.vertices[j].material=bundle.slots[bundle.offsets[g]+part.vertices[j].material];
                    if(part.bytes && memcmp(part.vertices,live.vertices+at,part.bytes))return 3;
                    at+=part.count;rf_preview_close(&part);
                }
                if(at!=live.count)return 3;
                {
                    rf_preview_mesh before=live;void *copy=malloc(live.bytes?live.bytes:1);
                    if(!copy)return 3;if(live.bytes)memcpy(copy,live.vertices,live.bytes);
                    if(live.bytes) {
                        /* Empty metadata forces the count pass to discover the
                         * short capacity instead of rejecting the old size. */
                        live.count=live.bytes=0;
                        if(rf_preview_update_world(&live,before.bytes-1,&world,&movers,poses,&bundle,&level)!=RF_RANGE ||
                            live.vertices!=address || live.bytes || live.count)return 3;
                        live=before;
                    }
                    if(movers.count) {
                        float value=poses[movers.count-1].position[0];poses[movers.count-1].position[0]=NAN;
                        if(rf_preview_update_world(&live,8*1024*1024,&world,&movers,poses,&bundle,&level)!=RF_FORMAT ||
                            memcmp(&live,&before,sizeof(live)))return 3;
                        poses[movers.count-1].position[0]=value;
                    }
                    if(live.bytes && memcmp(copy,live.vertices,live.bytes))return 3;
                    free(copy);
                }
            }
            free(poses);rf_preview_close(&live);
        }
        printf("B %u %u %u %u %u %u\n",bundle.count,bundle.textures.count,
            bundle.textures.loaded,bundle.textures.missing,bundle.resident_bytes,peak);
        for(g=0;g<bundle.count;++g)for(j=0;j<sources[g]->textures;++j) {
            char name[61];if(rf_geometry_texture_name(sources[g],j,name,sizeof(name)))return 3;
            printf("N %u %u %u %s\n",g,j,bundle.slots[bundle.offsets[g]+j],name);
        }
        free(sources);rf_geometry_close(&world);rf_geometry_movers_close(&movers);
        for(i=0;i<count;++i)rf_vpp_close(archives+i);
        for(i=0;i<bundle.textures.count;++i) {
            rf_material *item=bundle.textures.items+i;hash=2166136261u;
            for(k=0;k<item->image.bytes;++k)hash=(hash^item->image.rgba[k])*16777619u;
            printf("T %u %d %u %u %u %u\n",i,item->status,item->archive_index,
                item->image.width,item->image.height,hash);
        }
        rf_geometry_materials_close(&bundle);rf_geometry_materials_close(&bundle);return 0;
    }
    if (argc>=6 && argc<=21 && (!strcmp(argv[1],"--model") || !strcmp(argv[1],"--model-skin"))) {
        rf_model_file model;rf_model_materials bundle={0};uint32_t j,n=0;
        char storage[64][64];const char *names[64];
        if(!strcmp(argv[1],"--model-skin")) {
            while(n<64 && fgets(storage[n],sizeof(storage[n]),stdin)) {
                storage[n][strcspn(storage[n],"\r\n")]=0;names[n]=storage[n];++n;
            }
            if(ferror(stdin) || !feof(stdin))return 2;
        }
        count=(uint32_t)argc-5;
        if(rf_vpp_open(&level_archive,argv[2]))return 1;
        result=rf_model_file_open(&model,&level_archive,argv[3]);if(result){rf_vpp_close(&level_archive);return 1;}
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+5])) {while(i)rf_vpp_close(archives+--i);rf_vpp_close(&level_archive);return 1;}
        result=rf_model_materials_open_skin(&bundle,&model,names,n,archives,count,(uint32_t)strtoul(argv[4],NULL,10));
        for(i=0;i<count;++i)rf_vpp_close(archives+i);rf_vpp_close(&level_archive);
        if(!result) {
            printf("%u %u %u %u\n",bundle.count,bundle.textures.count,bundle.resident_bytes,bundle.peak_bytes);
            for(i=0;i<bundle.count;++i) {
                rf_model_material_instance *item=bundle.items+i;
                printf("M ");for(j=0;j<200;++j)printf("%02x",item->record.bytes[j]);printf(" %u\n",item->arrays[1][0]);
            }
        } else {if(bundle.items || bundle.textures.items || bundle.resident_bytes)return 3;printf("%d\n",result);}
        rf_model_materials_close(&bundle);return result?1:0;
    }
    if (argc>=4 && argc<=19 && !strcmp(argv[1],"--named")) {
        char storage[512][62];const char *names[512];uint32_t n=0,j,hash;
        count=(uint32_t)argc-3;
        while (n<512 && fgets(storage[n],sizeof(storage[n]),stdin)) {
            storage[n][strcspn(storage[n],"\r\n")]=0; names[n]=storage[n];++n;
        }
        if (ferror(stdin) || !feof(stdin)) return 2;
        for(i=0;i<count;++i) if(rf_vpp_open(archives+i,argv[i+3])) { while(i)rf_vpp_close(archives+--i);return 1; }
        result=rf_materials_open_names(&materials,names,n,archives,count,(uint32_t)strtoul(argv[2],NULL,10));
        for(i=0;i<count;++i)rf_vpp_close(archives+i);
        if(!result) {
            printf("%u %u %u %u\n",materials.count,materials.loaded,materials.missing,materials.allocated_bytes);
            for(i=0;i<n;++i) {
                rf_image *image=&materials.items[i].image;hash=2166136261u;
                for(j=0;j<image->bytes;++j)hash=(hash^image->rgba[j])*16777619u;
                printf("%s %d %u %u %u %u %u %d\n",names[i],materials.items[i].status,materials.items[i].archive_index,image->width,image->height,hash,image->source_format,rf_image_format_has_alpha(image->source_format));
            }
        } else { if(materials.items || materials.count || materials.allocated_bytes)return 3;printf("%d\n",result); }
        rf_materials_close(&materials);return result ? 1 : 0;
    }
    if (argc < 5 || argc > 20) return 2;
    count = (uint32_t)argc - 4;
    if (rf_vpp_open(&level_archive, argv[1])) return 1;
    result = rf_level_open(&level, &level_archive, argv[2]);
    if (!result) result = rf_geometry_open(&geometry, &level, 8*1024*1024);
    rf_vpp_close(&level_archive);
    if (result) return 1;
    for (i = 0; i < count; ++i) {
        result = rf_vpp_open(archives+i, argv[i+4]);
        if (result) {
            while (i) rf_vpp_close(archives + --i);
            rf_geometry_close(&geometry); return 1;
        }
    }
    result = rf_materials_open(&materials, &geometry, archives, count, (uint32_t)strtoul(argv[3], NULL, 10));
    for (i = 0; i < count; ++i) rf_vpp_close(archives+i);
    if (!result) {
        printf("%u %u %u %u\n", materials.count, materials.loaded, materials.missing, materials.allocated_bytes);
        for (i = 0; i < materials.count; ++i) {
            char name[61];
            rf_geometry_texture_name(&geometry, i, name, sizeof(name));
            printf("%s %d %u %u %u\n", name, materials.items[i].status, materials.items[i].archive_index,
                   materials.items[i].image.width, materials.items[i].image.height);
        }
    } else {
        if (materials.items || materials.count || materials.allocated_bytes) return 3;
        printf("%d\n", result);
    }
    rf_materials_close(&materials); rf_geometry_close(&geometry);
    return result ? 1 : 0;
}
