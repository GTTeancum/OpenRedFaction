#include "rf/material.h"
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
    rf_vpp level_archive, archives[16];
    rf_level level;
    rf_geometry geometry;
    rf_materials materials;
    uint32_t count, i;
    int result;
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
                printf("%s %d %u %u %u %u\n",names[i],materials.items[i].status,materials.items[i].archive_index,image->width,image->height,hash);
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
