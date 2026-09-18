#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene_jeep_gun_resources.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Jeep gun line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}};rf_model_file *file=malloc(sizeof(*file));rf_static_model_tags chassis={0};
    scene_jeep_gun_resources *owner=NULL;float zero[3]={0},identity[9]={1,0,0,0,1,0,0,0,1};
    float position[3]={10,20,30},basis[9]={0,0,-1,0,1,0,1,0,0},gun[12],muzzle[12],primary[12],rotated[12],g2[12],p2[12];
    uint32_t i;char path[128];CHECK(file);
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!rf_model_file_open(file,&meshes,"Jeep01.v3m"));CHECK(!rf_static_model_tags_open(file,65536,&chassis));
    CHECK(!scene_jeep_gun_resources_open(&meshes,maps,4,&chassis,576*1024,&owner));
    rf_static_model_tags_close(&chassis);free(file);rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    CHECK(owner->model->render.part_count && owner->model->materials.textures.count);
    CHECK(!scene_jeep_gun_attachment_pose(owner,zero,identity,gun,muzzle,primary));
    CHECK(fabsf(gun[9]-.0053349268f)<.00001f && fabsf(gun[10]-1.35058153f)<.00001f);
    CHECK(fabsf(muzzle[11]-(-.58039057f-1.40131366f))<.0001f);
    CHECK(!scene_jeep_gun_attachment_pose(owner,position,basis,g2,rotated,p2));
    CHECK(fabsf(rotated[9]-(10+muzzle[11]))<.0001f && fabsf(rotated[10]-(20+muzzle[10]))<.0001f && fabsf(rotated[11]-(30-muzzle[9]))<.0001f);
    printf("JEEP_GUN resident=%u peak=%u parts=%u textures=%u mount=%g,%g,%g muzzle=%g,%g,%g primary=%g,%g,%g\n",
        owner->resident_bytes,owner->peak_bytes,owner->model->render.part_count,owner->model->materials.textures.count,
        gun[9],gun[10],gun[11],muzzle[9],muzzle[10],muzzle[11],primary[9],primary[10],primary[11]);
    scene_jeep_gun_resources_close(&owner);puts("PASS actual Jeep gun mount/muzzle composition after source closure");return 0;
}
