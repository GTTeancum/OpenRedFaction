#include "rf/material.h"
#include "rf/preview.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>
extern char rf_material_failure_name[61];
extern uint32_t rf_material_failure[3];
int main(int argc, char **argv)
{
    rf_vpp level_archive, archives[16];
    rf_level level;
    rf_geometry geometry;
    rf_materials materials;
    uint32_t count, i;
    int result;
    if(argc>=5 && argc<=20 && !strcmp(argv[1],"--glare-materials")) {
        rf_glare_classes classes={0};rf_glare_materials bundle={0},empty={0};uint32_t j,k,f;
        if(rf_vpp_open(&level_archive,argv[2]))return 2;
        result=rf_glare_classes_open(&level_archive,1000000,&classes);rf_vpp_close(&level_archive);if(result)return 2;
        count=(uint32_t)argc-4;
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+4]))return 2;
        result=rf_glare_materials_open(&bundle,classes.definitions,classes.count,archives,count,(uint32_t)strtoul(argv[3],NULL,10));
        rf_glare_classes_close(&classes);for(i=0;i<count;++i)rf_vpp_close(archives+i);
        printf("%d %u %u %u\n",result,bundle.count,bundle.texture_count,bundle.resident_bytes);
        if(result){if(memcmp(&bundle,&empty,sizeof(bundle)))return 3;return 0;}
        for(i=0;i<bundle.count;++i)printf("B %u %u %u\n",bundle.bindings[i][0],bundle.bindings[i][1],bundle.bindings[i][2]);
        for(i=0;i<bundle.texture_count;++i) {
            rf_particle_animation *animation=&bundle.textures[i].animation;
            printf("T %s %u %u %u %u\n",bundle.textures[i].name,animation->count,animation->rate,animation->archive_index,animation->resident_bytes);
            for(f=0;f<animation->count;++f) {
                rf_image *image=animation->images+f;uint32_t hash=2166136261u;
                for(j=0;j<image->height;++j)for(k=0;k<image->width;++k) {
                    unsigned char *pixel=rf_image_pixel(image,k,j);uint32_t b;
                    for(b=0;b<(rf_image_is_packed_1555(image)?2u:4u);++b)hash=(hash^pixel[b])*16777619u;
                }
                printf("F %u %u %u\n",image->width,image->height,hash);
            }
        }
        rf_glare_materials_close(&bundle);rf_glare_materials_close(&bundle);
        return memcmp(&bundle,&empty,sizeof(bundle))?3:0;
    }
    if(argc>=4 && argc<=19 && (!strcmp(argv[1],"--records") || !strcmp(argv[1],"--records-overrides"))) {
        uint32_t n,j;uint8_t (*rows)[84];rf_model_materials bundle={0};
        static char replacements[4096][64];static const char *names[4096];
        int overridden=!strcmp(argv[1],"--records-overrides");
        _setmode(_fileno(stdin),_O_BINARY);
        if(fread(&n,4,1,stdin)!=1 || n>4096)return 2;
        rows=n?malloc(n*84):NULL;if(n && (!rows || fread(rows,84,n,stdin)!=n)){free(rows);return 2;}
        if(overridden) {
            if(n && fread(replacements,64,n,stdin)!=n){free(rows);return 2;}
            for(i=0;i<n;++i) {
                if(!memchr(replacements[i],0,64)){free(rows);return 2;}
                names[i]=replacements[i][0]?replacements[i]:NULL;
            }
        }
        count=(uint32_t)argc-3;
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+3])){while(i)rf_vpp_close(archives+--i);free(rows);return 1;}
        result=overridden?rf_model_materials_open_records_overrides(&bundle,rows,n,names,archives,count,(uint32_t)strtoul(argv[2],NULL,10)):
            rf_model_materials_open_records(&bundle,rows,n,archives,count,(uint32_t)strtoul(argv[2],NULL,10));
        memset(replacements,0xdd,sizeof(replacements));memset(names,0,sizeof(names));
        if(n)memset(rows,0xdd,n*84);free(rows);for(i=0;i<count;++i)rf_vpp_close(archives+i);
        if(!result) {
            printf("%u %u %u %u\n",bundle.count,bundle.textures.count,bundle.resident_bytes,bundle.peak_bytes);
            for(i=0;i<bundle.count;++i) {
                rf_model_material_instance *item=bundle.items+i;
                printf("M ");for(j=0;j<200;++j)printf("%02x",item->record.bytes[j]);printf(" %u\n",item->arrays[1][0]);
            }
        } else {if(bundle.items || bundle.textures.items || bundle.resident_bytes)return 3;printf("%d\n",result);}
        rf_model_materials_close(&bundle);rf_model_materials_close(&bundle);return result?1:0;
    }
    if(argc>=8 && argc<=23 && !strcmp(argv[1],"--entities")) {
        rf_vpp tables,meshes;rf_entity_seeds seeds={0};rf_entity_skeletons skeletons={0};
        rf_entity_render_models models={0};rf_entity_appearances appearances={0};rf_entity_materials bundle={0},guard={0};
        uint32_t j,k,peak;
        if(rf_vpp_open(&level_archive,argv[2]) || rf_vpp_open(&tables,argv[3]) || rf_vpp_open(&meshes,argv[4]) ||
            rf_level_open(&level,&level_archive,argv[5]) || rf_entity_seeds_open(&level,&tables,4*1024*1024,&seeds) ||
            rf_entity_skeletons_open(&seeds,&meshes,1024*1024,&skeletons) ||
            rf_entity_render_models_open(&skeletons,&meshes,4*1024*1024,&models) ||
            rf_entity_appearances_open(&seeds,&skeletons,&tables,1024*1024,&appearances))return 4;
        count=(uint32_t)argc-7;
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+7]))return 4;
        result=rf_entity_materials_open(&bundle,&appearances,&models,archives,count,(uint32_t)strtoul(argv[6],NULL,10));
        if(result){fprintf(stderr,"entity materials %d\n",result);return 5;}
        peak=bundle.peak_bytes;
        if(rf_entity_materials_open(&guard,&appearances,&models,archives,count,peak-1)!=RF_RANGE ||
            memcmp(&guard,&(rf_entity_materials){0},sizeof(guard)))return 6;
        if(rf_entity_materials_open(&guard,&appearances,&models,archives,count,peak) || guard.peak_bytes!=peak ||
            guard.resident_bytes!=bundle.resident_bytes || memcmp(guard.offsets,bundle.offsets,(bundle.count+1)*4))return 7;
        rf_entity_materials_close(&guard);rf_entity_materials_close(&guard);
        if(rf_entity_materials_open(&guard,&appearances,&models,NULL,0,peak)!=RF_NOT_FOUND ||
            memcmp(&guard,&(rf_entity_materials){0},sizeof(guard)))return 8;
        rf_entity_appearances_close(&appearances);rf_entity_render_models_close(&models);
        rf_entity_skeletons_close(&skeletons);rf_entity_seeds_close(&seeds);
        rf_vpp_close(&meshes);rf_vpp_close(&tables);rf_vpp_close(&level_archive);
        for(i=0;i<count;++i)rf_vpp_close(archives+i);
        printf("E %u %u %u %u %u\n",bundle.count,bundle.materials.count,bundle.materials.textures.count,bundle.resident_bytes,bundle.peak_bytes);
        for(i=0;i<bundle.count;++i)printf("A %u %u %u\n",i,bundle.offsets[i],bundle.offsets[i+1]);
        for(i=0;i<bundle.materials.count;++i) {
            rf_model_material_instance *item=bundle.materials.items+i;
            printf("M ");for(j=0;j<200;++j)printf("%02x",item->record.bytes[j]);printf(" %u\n",item->arrays[1][0]);
        }
        for(i=0;i<bundle.materials.textures.count;++i) {
            rf_image *image=&bundle.materials.textures.items[i].image;uint32_t hash=2166136261u;
            for(k=0;k<image->bytes;++k)hash=(hash^image->rgba[k])*16777619u;
            printf("T %u %u %u %u %u\n",i,image->width,image->height,image->bytes,hash);
        }
        rf_entity_materials_close(&bundle);rf_entity_materials_close(&bundle);
        return memcmp(&bundle,&(rf_entity_materials){0},sizeof(bundle))?9:0;
    }
    if(argc>=5 && argc<=20 && !strcmp(argv[1],"--particle-animation")) {
        rf_particle_definition definition={0};rf_particle_animation animation={0},empty={0};
        uint32_t budget=(uint32_t)strtoul(argv[3],NULL,10),f;
        if(strlen(argv[2])>=sizeof(definition.bitmap))return 2;strcpy(definition.bitmap,argv[2]);
        count=(uint32_t)argc-4;
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+4]))return 1;
        result=rf_particle_animation_open(&animation,&definition,archives,count,budget);
        for(i=0;i<count;++i)rf_vpp_close(archives+i);
        memset(&definition,0xdd,sizeof(definition));
        if(result) {if(memcmp(&animation,&empty,sizeof(animation)))return 3;printf("%d %u\n",result,(unsigned)sizeof(animation));return 0;}
        printf("0 %u %u %u %u %u %u",(unsigned)sizeof(animation),(unsigned)sizeof(rf_image),animation.count,
            animation.rate,animation.resident_bytes,animation.archive_index);
        for(f=0;f<animation.count;++f) {
            uint32_t hash=2166136261u;rf_image *image=animation.images+f;
            for(i=0;i<image->bytes;++i)hash=(hash^image->rgba[i])*16777619u;
            printf(" %u %u %u",image->width,image->height,hash);
        }
        printf("\n");rf_particle_animation_close(&animation);rf_particle_animation_close(&animation);
        return memcmp(&animation,&empty,sizeof(animation))?3:0;
    }
    if(argc>=4 && argc<=19 && !strcmp(argv[1],"--corpse-texture")) {
        rf_particle_bitmap bitmap={0},empty={0};uint32_t budget=(uint32_t)strtoul(argv[2],NULL,10),hash=2166136261u;
        count=(uint32_t)argc-3;
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+3]))return 1;
        result=rf_corpse_surface_texture_open(&bitmap,archives,count,budget);
        for(i=0;i<count;++i)rf_vpp_close(archives+i);
        if(result){if(memcmp(&bitmap,&empty,sizeof(bitmap)))return 3;printf("%d %u\n",result,(unsigned)sizeof(bitmap));return 0;}
        for(i=0;i<bitmap.image.bytes;++i)hash=(hash^bitmap.image.rgba[i])*16777619u;
        printf("0 %u %u %u %u %u %u %u %u %u\n",(unsigned)sizeof(bitmap),bitmap.frames,bitmap.rate,
            bitmap.image.width,bitmap.image.height,bitmap.resident_bytes,bitmap.archive_index,hash,bitmap.image.source_format);
        rf_particle_bitmap_close(&bitmap);rf_particle_bitmap_close(&bitmap);return memcmp(&bitmap,&empty,sizeof(bitmap))?3:0;
    }
    if(argc>=6 && argc<=21 && !strcmp(argv[1],"--particle")) {
        rf_particle_definition definition={0};rf_particle_bitmap bitmap={0},empty={0};
        uint32_t frame=(uint32_t)strtoul(argv[3],NULL,10),budget=(uint32_t)strtoul(argv[4],NULL,10),hash=2166136261u;
        if(strlen(argv[2])>=sizeof(definition.bitmap))return 2;strcpy(definition.bitmap,argv[2]);
        count=(uint32_t)argc-5;
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+5]))return 1;
        result=rf_particle_bitmap_open(&bitmap,&definition,archives,count,frame,budget);
        for(i=0;i<count;++i)rf_vpp_close(archives+i);
        memset(&definition,0xdd,sizeof(definition));
        if(result) {if(memcmp(&bitmap,&empty,sizeof(bitmap)))return 3;printf("%d %u\n",result,(unsigned)sizeof(bitmap));return 0;}
        for(i=0;i<bitmap.image.bytes;++i)hash=(hash^bitmap.image.rgba[i])*16777619u;
        printf("0 %u %u %u %u %u %u %u %u\n",(unsigned)sizeof(bitmap),bitmap.frames,bitmap.rate,
            bitmap.image.width,bitmap.image.height,bitmap.resident_bytes,bitmap.archive_index,hash);
        rf_particle_bitmap_close(&bitmap);rf_particle_bitmap_close(&bitmap);
        return memcmp(&bitmap,&empty,sizeof(bitmap))?3:0;
    }
    if(argc>=5 && argc<=21 && (!strcmp(argv[1],"--geometry") || !strcmp(argv[1],"--residency") || !strcmp(argv[1],"--texture-names"))) {
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
        if(!strcmp(argv[1],"--texture-names")) {
            printf("G %u\n",movers.count+1);
            for(g=0;g<=movers.count;++g)for(j=0;j<sources[g]->textures;++j) {
                char name[61];if(rf_geometry_texture_name(sources[g],j,name,sizeof(name)))return 3;
                printf("N %u %u %s\n",g,j,name);
            }
            free(sources);rf_geometry_close(&world);rf_geometry_movers_close(&movers);return 0;
        }
        count=(uint32_t)argc-5;
        for(i=0;i<count;++i)if(rf_vpp_open(archives+i,argv[i+5]))return 1;
        result=rf_geometry_materials_open(&bundle,sources,movers.count+1,archives,count,
            (uint32_t)strtoul(argv[4],NULL,10));if(result){fprintf(stderr,"material load %d: %s archive %u entry bytes %u\n",result,rf_material_failure_name,rf_material_failure[1],rf_material_failure[2]);return 1;}
        peak=bundle.peak_bytes;
        if(rf_geometry_materials_open(&check,sources,movers.count+1,archives,count,peak-1)!=RF_RANGE ||
            check.offsets || check.slots || check.textures.items || check.resident_bytes)return 3;
        if(rf_geometry_materials_open(&check,sources,movers.count+1,archives,count,peak) ||
            check.peak_bytes!=peak || check.textures.count!=bundle.textures.count ||
            memcmp(check.slots,bundle.slots,bundle.offsets[bundle.count]*sizeof(uint32_t)))return 3;
        rf_geometry_materials_close(&check);rf_geometry_materials_close(&check);
        if(strcmp(argv[1],"--residency")) {
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
        if(strcmp(argv[1],"--residency")) {
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
