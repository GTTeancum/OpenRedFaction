/* V4 cockpit local IDs must resolve into one retained global material bank. */
#include "rf/material.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"APC cockpit materials line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int inspect(const char *name,uint32_t apc)
{
    rf_vpp meshes={0},maps[4]={{0}};rf_vfx_geometry_asset *asset=NULL;rf_vfx_asset_materials *materials=NULL;
    const uint32_t budget=8u*1024u*1024u;uint32_t i,j,k,faces=0,reused=0,mapped=0,images=0,pixels=0,geometry_bytes,peak;
    uint32_t seen[64]={0};char path[128];
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!rf_vfx_geometry_asset_open(&meshes,name,budget,&asset));geometry_bytes=asset->resident_bytes;peak=asset->peak_bytes;
    CHECK(geometry_bytes<budget);CHECK(!rf_vfx_asset_materials_open(asset,maps,4,budget-geometry_bytes,&materials));
    CHECK(geometry_bytes+materials->resident_bytes<=budget && peak<=budget);
    if(apc)CHECK(asset->count==86 && asset->material_bank && asset->material_bank->count==32 && materials->count==32);
    else CHECK(asset->count==6);
    /* Every remaining access must use retained geometry, material and pixels. */
    rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    for(i=0;i<asset->count;i++){
        const rf_vfx_mesh *mesh=asset->meshes[i];CHECK(mesh && mesh->data);
        CHECK(!rf_vfx_instance_update(asset->instances[i],0));
        for(j=0;j<mesh->prefix.faces;j++){
            rf_vfx_face face;uint32_t index=UINT32_MAX,expected,offset=mesh->prefix.face_offset+j*(mesh->version<0x3000d?120u:96u);
            CHECK(!rf_vfx_face_read(mesh->data+offset,mesh->bytes-offset,mesh->version,&face));
            CHECK(!rf_vfx_asset_material_index(asset,materials,i,face.material,&index));CHECK(index<materials->count);
            if(mesh->version>=0x40000){
                CHECK(face.material<mesh->materials);expected=word(mesh->data+mesh->material_offset+face.material*4);
                CHECK(index==expected && expected<asset->material_bank->count);
            }else{expected=materials->first[i]+face.material;CHECK(index==expected);}
            if(seen[index]++)reused++;mapped|=1u;faces++;
        }
        {uint32_t unchanged=0xfeed1234u;CHECK(rf_vfx_asset_material_index(asset,materials,i,mesh->materials,&unchanged)!=RF_OK);CHECK(unchanged==0xfeed1234u);}
    }
    CHECK(mapped && reused);
    for(i=0;i<materials->textures.texture_count;i++){
        const rf_particle_animation *animation=&materials->textures.textures[i].animation;
        CHECK(animation->count && animation->images);
        for(k=0;k<animation->count;k++){
            const rf_image *image=animation->images+k;CHECK(image->rgba && image->bytes && image->width && image->height);
            images++;pixels+=image->bytes;
        }
    }
    CHECK(images && pixels);if(!apc)CHECK(faces==226);
    printf("COCKPIT_MATERIALS %s meshes=%u materials=%u faces=%u reused=%u images=%u pixels=%u resident=%u geometry_peak=%u budget=%u\n",
        name,asset->count,materials->count,faces,reused,images,pixels,geometry_bytes+materials->resident_bytes,peak,budget);
    rf_vfx_asset_materials_close(&materials);rf_vfx_geometry_asset_close(&asset);return 0;
}
int main(void)
{
    CHECK(!inspect("APC.vfx",1));CHECK(!inspect("driller01.vfx",0));
    puts("PASS APC global material mapping and legacy Driller after archive close");return 0;
}
