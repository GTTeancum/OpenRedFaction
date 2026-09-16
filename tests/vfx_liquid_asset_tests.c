#include "rf/effect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);return 1;}}while(0)
static void word(unsigned char *p,uint32_t n){memcpy(p,&n,4);}
/* Private stream; never rewrite the installed archive. */
static int corrupt_open(const unsigned char *payload,uint32_t bytes,rf_vfx_geometry_asset **out)
{
    unsigned char directory[4096]={0},zero[2048]={0};rf_vpp archive={0};int status;
    uint32_t padded=(bytes+2047u)&~2047u;
    memcpy(directory+2048,"ripple.vfx",11);word(directory+2048+60,bytes);
    archive.stream=tmpfile();if(!archive.stream)return RF_IO;
    archive.length=4096+padded;archive.count=1;archive.payload_offset=4096;
    if(fwrite(directory,1,sizeof(directory),archive.stream)!=sizeof(directory) ||
       fwrite(payload,1,bytes,archive.stream)!=bytes ||
       fwrite(zero,1,padded-bytes,archive.stream)!=padded-bytes){fclose(archive.stream);return RF_IO;}
    fflush(archive.stream);status=rf_vfx_geometry_asset_open(&archive,"ripple.vfx",1048576,out);
    rf_vpp_close(&archive);return status;
}
int main(int argc,char **argv)
{
    rf_vpp archive;rf_vpp_entry entry;rf_vfx_geometry_asset *asset=NULL,*other=NULL;
    uint32_t i,peak,material_offset;float scalar;unsigned char *payload;uint32_t saved;
    const char *path=argc>1?argv[1]:"Installed_Game/meshes.vpp";
    CHECK(!rf_vpp_open(&archive,path));
    CHECK(!rf_vfx_geometry_asset_open(&archive,"WaterRipple01.vfx",1048576,&asset));
    CHECK(asset->version==0x40006 && asset->count==4 && asset->material_bank && asset->material_bank->count==4);
    peak=asset->peak_bytes;CHECK(peak>asset->resident_bytes);
    CHECK(rf_vfx_geometry_asset_open(&archive,"WaterRipple01.vfx",peak-1,&other)==RF_RANGE && !other);
    CHECK(!rf_vfx_geometry_asset_open(&archive,"WaterRipple01.vfx",peak,&other));rf_vfx_geometry_asset_close(&other);
    CHECK(!rf_vpp_find(&archive,"WaterRipple01.vfx",&entry));payload=malloc(entry.size);CHECK(payload);
    CHECK(!rf_vpp_read(&archive,&entry,0,payload,entry.size));
    /* All material IDs are resolved by mesh_open, even before texture loading. */
    material_offset=136+asset->meshes[0]->material_offset;
    memcpy(&saved,payload+material_offset,4);word(payload+material_offset,asset->material_bank->count);
    CHECK(corrupt_open(payload,entry.size,&other)==RF_FORMAT && !other);word(payload+material_offset,saved);
    memcpy(&saved,payload+16,4);word(payload+16,3);
    CHECK(corrupt_open(payload,entry.size,&other)==RF_FORMAT && !other);word(payload+16,saved);
    memcpy(&saved,payload+128,4);memcpy(payload+128,"BAD!",4);
    CHECK(corrupt_open(payload,entry.size,&other)==RF_FORMAT && !other);word(payload+128,saved);
    CHECK(corrupt_open(payload,entry.size-1,&other)==RF_FORMAT && !other);
    free(payload);
    /* Existing legacy owner still accepts the rocket's embedded materials. */
    CHECK(!rf_vfx_geometry_asset_open(&archive,"DrillMissile01.vfx",131072,&other));
    CHECK(other->version<0x40000 && !other->material_bank);rf_vfx_geometry_asset_close(&other);
    rf_vpp_close(&archive);
    /* The bank/meshes/instances survive archive closure. */
    for(i=0;i<asset->count;i++) {
        rf_vfx_mesh *mesh=asset->meshes[i];CHECK(mesh && asset->instances[i]);
        CHECK(!rf_vfx_instance_update(asset->instances[i],2) && asset->instances[i]->active);
        CHECK(!rf_vfx_mesh_material_sample(mesh,asset->material_bank,0,1,2,&scalar) && scalar==0);
        CHECK(!rf_vfx_mesh_material_sample(mesh,asset->material_bank,0,2,2,&scalar) && scalar>=0 && scalar<=1);
        CHECK(!rf_vfx_instance_update(asset->instances[i],10000) && !asset->instances[i]->active);
    }
    rf_vfx_geometry_asset_close(&asset);rf_vfx_geometry_asset_close(&asset);CHECK(!asset);
    puts("PASS version4 ripple ownership, global materials, budgets, malformed references and legacy loading");return 0;
}
