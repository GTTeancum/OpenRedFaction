#include "rf/level.h"
#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"Geo region line %d\n",__LINE__);return 1;}} while(0)
typedef struct names {char values[256][61];uint32_t count;} names;
static int collect(const rf_vpp_entry *entry,void *context)
{
    names *n=context;size_t len=strlen(entry->name);
    if(len>=4 && !strcmp(entry->name+len-4,".rfl")) {
        if(n->count==256)return RF_RANGE;
        strcpy(n->values[n->count++],entry->name);
    }
    return RF_OK;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level={0};const rf_level_section *section;
    unsigned char data[128],bad[128];rf_geo_region out,sentinel;uint32_t count,i;
    char path[1024];CHECK(argc==2);
    snprintf(path,sizeof(path),"%s/levelsm.vpp",argv[1]);
    CHECK(!rf_vpp_open(&archive,path));CHECK(!rf_level_open(&level,&archive,"glass_house.rfl"));
    {
        rf_level_geomod_settings settings,saved;unsigned char prefix[69];uint32_t size;
        CHECK(!rf_level_geomod_settings_read(&level,&settings));
        printf("Glass House GeoMod texture=%s hardness=%u\n",settings.texture,settings.hardness);
        CHECK(!strcmp(settings.texture,"rock02.tga") && settings.hardness==0);
        saved=settings;size=(uint32_t)strlen(settings.texture)+6;
        CHECK(!rf_level_read(&level,rf_level_find(&level,0x900),0,prefix,size));
        for(i=0;i<size;i++)CHECK(rf_level_geomod_settings_decode(prefix,i,&settings)!=RF_OK && !memcmp(&settings,&saved,sizeof(saved)));
        CHECK(!rf_level_geomod_settings_decode(prefix,size,&settings) && !memcmp(&settings,&saved,sizeof(saved)));
        prefix[0]=64;prefix[1]=0;CHECK(rf_level_geomod_settings_decode(prefix,sizeof(prefix),&settings)==RF_RANGE && !memcmp(&settings,&saved,sizeof(saved)));
    }
    section=rf_level_find(&level,0x200);CHECK(section && section->size==72);
    CHECK(!rf_level_read(&level,section,0,data,72));rf_vpp_close(&archive);
    CHECK(!rf_level_geo_regions_decode(data,72,&out,1,&count));
    CHECK(count==1 && out.uid==128 && out.flags==4 && out.hardness==25);
    CHECK(out.position[0]==0 && out.position[1]==0 && out.position[2]==0);
    CHECK(out.dimensions[0]==56 && out.dimensions[1]==48 && out.dimensions[2]==60);
    CHECK(out.file_basis[2]==1 && out.file_basis[3]==1 && out.file_basis[7]==1);
    {
        rf_geomod_hardness_result value,saved;float point[3]={0,0,0};rf_geo_region region=out;
        CHECK(!rf_geomod_hardness(&region,1,0,point,4,&value));
        CHECK(value.hardness==25 && value.matches==1 && value.allowed && value.scale==3);
        saved=value;region.flags|=32;
        CHECK(rf_geomod_hardness(&region,1,0,point,4,&value)==RF_NOT_FOUND && !memcmp(&value,&saved,sizeof(value)));
        {
            rf_geomod_region_result prepared,kept;rf_geo_region pair[2]={out,out};
            pair[0].flags|=32;pair[0].shallow_depth=2;
            CHECK(!rf_geomod_regions_prepare(pair,1,0,point,4,&prepared));
            CHECK(prepared.hardness.allowed && prepared.limit_count==1 && prepared.limits[0].depth==2);
            CHECK(prepared.limits[0].normal[1]==-1 && prepared.hardness.scale==3);
            pair[0].shallow_depth=-2;
            CHECK(!rf_geomod_regions_prepare(pair,1,0,point,4,&prepared));
            CHECK(prepared.limit_count==1 && prepared.limits[0].depth==-2 && prepared.limits[0].normal[1]==-1);
            pair[0].shallow_depth=2;
            kept=prepared;pair[1]=pair[0];pair[1].shallow_depth=NAN;
            CHECK(rf_geomod_regions_prepare(pair,2,0,point,4,&prepared)==RF_FORMAT && !memcmp(&prepared,&kept,sizeof(kept)));
            pair[1]=pair[0];pair[1].file_basis[7]=-1;
            CHECK(!rf_geomod_regions_prepare(pair,2,0,point,4,&prepared) && !prepared.hardness.allowed);
            pair[1]=pair[0];pair[1].hardness=75;
            CHECK(!rf_geomod_regions_prepare(pair,2,0,point,4,&prepared));
            CHECK(prepared.hardness.allowed && prepared.limit_count==1 && prepared.hardness.hardness==75 && fabsf(prepared.hardness.scale-1)<.000001f);
        }
        region=out;region.position[0]=NAN;
        CHECK(rf_geomod_hardness(&region,1,0,point,4,&value)==RF_FORMAT && !memcmp(&value,&saved,sizeof(value)));
        CHECK(rf_geomod_hardness(NULL,1,0,point,4,&value)==RF_RANGE && !memcmp(&value,&saved,sizeof(value)));
        region=out;region.hardness=100;
        CHECK(!rf_geomod_hardness(&region,1,0,point,4,&value) && !value.allowed && value.scale==4);
        CHECK(!rf_geomod_hardness(NULL,0,0,point,4,&value) && value.hardness==55 && value.matches==0);
    }
    memset(&sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<72;i++) {
        out=sentinel;count=77;
        CHECK(rf_level_geo_regions_decode(data,i,&out,1,&count)!=RF_OK);
        CHECK(count==77 && !memcmp(&out,&sentinel,sizeof(out)));
    }
    CHECK(rf_level_geo_regions_decode(data,72,&out,0,&count)==RF_RANGE);
    memcpy(bad,data,72);bad[10]=101; /* hardness at payload offset10 */
    CHECK(rf_level_geo_regions_decode(bad,72,&out,1,&count)==RF_FORMAT);
    memcpy(bad,data,72);bad[8]=6;
    CHECK(rf_level_geo_regions_decode(bad,72,&out,1,&count)==RF_FORMAT);
    memcpy(bad,data,72);memset(bad+12,0xff,4);
    CHECK(rf_level_geo_regions_decode(bad,72,&out,1,&count)==RF_FORMAT);
    memset(bad,0,sizeof(bad));bad[0]=1;bad[8]=34;bad[10]=50;
    {float depth=.5f,radius=3;memcpy(bad+12,&depth,4);memcpy(bad+64,&radius,4);}
    CHECK(!rf_level_geo_regions_decode(bad,68,&out,1,&count));
    CHECK(out.flags==34 && out.shallow_depth==.5f && out.radius==3);
    CHECK(rf_level_geo_regions_decode(bad,69,&out,1,&count)==RF_FORMAT);
    memset(bad,0,sizeof(bad));bad[0]=1;bad[8]=2;bad[10]=25;
    {float radius=2;memcpy(bad+24,&radius,4);}
    CHECK(!rf_level_geo_regions_decode(bad,28,&out,1,&count) && out.radius==2);
    {
        const char *archives[]={"levels1.vpp","levels2.vpp","levels3.vpp","levelsm.vpp"};
        uint32_t a,j,total=0,sections=0;
        for(a=0;a<4;a++) {
            names list={0};snprintf(path,sizeof(path),"%s/%s",argv[1],archives[a]);
            CHECK(!rf_vpp_open(&archive,path));CHECK(!rf_vpp_visit(&archive,collect,&list));
            for(j=0;j<list.count;j++) {
                unsigned char *payload;
                CHECK(!rf_level_open(&level,&archive,list.values[j]));section=rf_level_find(&level,0x200);
                {rf_level_geomod_settings settings;CHECK(!rf_level_geomod_settings_read(&level,&settings));}
                if(!section)continue;
                payload=malloc(section->size);CHECK(payload);
                CHECK(!rf_level_read(&level,section,0,payload,section->size));
                if(rf_level_geo_regions_decode(payload,section->size,NULL,0,&count)) {
                    fprintf(stderr,"Rejected %s/%s\n",archives[a],list.values[j]);free(payload);return 1;
                }
                total+=count;sections++;free(payload);
            }
            rf_vpp_close(&archive);
        }
        printf("PASS: %u installed sections, %u regions; corruption and synthetic cases\n",sections,total);
    }
    return 0;
}
