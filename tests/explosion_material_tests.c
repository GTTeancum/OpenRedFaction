#include "rf/material.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"explosion materials line%d\n",__LINE__);return 1;}} while(0)
int main(int argc,char **argv)
{
    rf_vpp tables={0},maps[4]={{0}};rf_vclip_definition impact;
    rf_explosion_definition definition,bad;rf_explosion_materials owner={0},empty={0},failed={0},dedup={0};
    uint32_t i,j,bytes,hash=2166136261u;char path[1024];
    CHECK(argc==2 || argc==3);snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);CHECK(!rf_vpp_open(&tables,path));
    CHECK(!rf_vclip_definition_load(&tables,"rocket_impact",128*1024,&impact));
    CHECK(impact.flags==8 && !strcmp(impact.explosion,"rocket hit"));
    CHECK(!rf_explosion_definition_load(&tables,impact.explosion,256*1024,&definition));
    CHECK(definition.recipe.central_count==6 && definition.recipe.play_time==2 && definition.recipe.sparks_count==20);
    CHECK(definition.resolved==63); /* Authored optional sparks reference is missing. */
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"%s/maps%u.vpp",argv[1],i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!rf_explosion_materials_open(&owner,&definition,maps,4,128*1024));bytes=owner.resident_bytes;
    for(i=0;i<7;i++)if(definition.resolved&(1u<<i)) {
        uint32_t slot=owner.slot_texture[i];CHECK(slot<owner.count);
        printf("EXPLOSION_TEXTURE slot%u texture%u %s frames%u bytes%u\n",i,slot,definition.emitters[i].bitmap,owner.animations[slot].count,owner.animations[slot].resident_bytes);
    }
    CHECK(owner.slot_texture[6]==UINT32_MAX && owner.slot_texture[7]==UINT32_MAX && owner.slot_texture[8]==UINT32_MAX);
    CHECK(rf_explosion_materials_open(&failed,&definition,maps,4,bytes-1)==RF_RANGE && !memcmp(&failed,&empty,sizeof(empty)));
    bad=definition;strcpy(bad.emitters[5].bitmap,"missing-explosion-fixture.vbm");
    CHECK(rf_explosion_materials_open(&failed,&bad,maps,4,128*1024)==RF_NOT_FOUND && !memcmp(&failed,&empty,sizeof(empty)));
    bad=definition;bad.resolved=3;strcpy(bad.emitters[1].bitmap,bad.emitters[0].bitmap);
    for(i=0;bad.emitters[1].bitmap[i];i++)if(bad.emitters[1].bitmap[i]>='a' && bad.emitters[1].bitmap[i]<='z')bad.emitters[1].bitmap[i]-='a'-'A';
    CHECK(!rf_explosion_materials_open(&dedup,&bad,maps,4,128*1024));CHECK(dedup.count==1 && dedup.slot_texture[0]==dedup.slot_texture[1]);
    rf_explosion_materials_close(&dedup);rf_explosion_materials_close(&dedup);
    if(argc==3)for(i=0;i<owner.count;i++) {
        const rf_image *image=owner.animations[i].images;FILE *file;uint32_t x,y;
        CHECK(image->bytes==image->width*image->height*4);
        snprintf(path,sizeof(path),"%s/texture-%u.rgba",argv[2],i);file=fopen(path,"wb");CHECK(file);
        CHECK(fwrite(&image->width,4,1,file)==1 && fwrite(&image->height,4,1,file)==1);
        for(y=0;y<image->height;y++)for(x=0;x<image->width;x++)CHECK(fwrite(rf_image_pixel(image,x,y),4,1,file)==1);
        CHECK(!fclose(file));
    }
    memset(&definition,0xdd,sizeof(definition));rf_vpp_close(&tables);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    for(i=0;i<owner.count;i++)for(j=0;j<owner.animations[i].count;j++) {
        const rf_image *image=owner.animations[i].images+j;uint32_t k;
        CHECK(image->rgba && image->width && image->height && image->bytes);
        for(k=0;k<image->bytes;k++)hash=(hash^((const unsigned char*)image->rgba)[k])*16777619u;
    }
    printf("PASS: complete rocket-impact image owner bytes%u textures%u hash%u; budget, deduplication and late failure rollback\n",bytes,owner.count,hash);
    rf_explosion_materials_close(&owner);rf_explosion_materials_close(&owner);CHECK(!memcmp(&owner,&empty,sizeof(owner)));return 0;
}
