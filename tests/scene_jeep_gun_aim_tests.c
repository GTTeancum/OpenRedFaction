#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene_jeep_gun_aim.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Jeep gun aim line%d: %s\n",__LINE__,#x);return 1;}}while(0)
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
        {
        const float directions[4][3]={{0,0,1},{1,0,0},{0,1,0},{0,-1,0}};
        float aimed[12],tip[12],saved[12];uint32_t d,j,k;
        for(d=0;d<4;d++){
            CHECK(!scene_jeep_gun_aim_pose(owner,position,basis,directions[d],aimed,tip));
            for(j=0;j<3;j++){
                CHECK(fabsf(aimed[9+j]-g2[9+j])<.0001f);
                CHECK(fabsf(tip[6+j]-directions[d][j])<.0001f);
                for(k=0;k<3;k++){
                    float dot=aimed[j*3]*aimed[k*3]+aimed[j*3+1]*aimed[k*3+1]+aimed[j*3+2]*aimed[k*3+2];
                    CHECK(fabsf(dot-(j==k?1.f:0.f))<.0001f);
                }
            }
        }
        memcpy(saved,aimed,sizeof(saved));
        CHECK(scene_jeep_gun_aim_pose(owner,position,basis,(const float[3]){0,0,0},aimed,tip)==RF_FORMAT);
        CHECK(!memcmp(saved,aimed,sizeof(saved)));
        /* Alter muzzle-local rotation to prove the adapter rotates relative to
         * the actual muzzle frame, rather than simply assigning gun-forward. */
        owner->model->tags.items[owner->muzzle].rotation[0]=0;
        owner->model->tags.items[owner->muzzle].rotation[1]=.70710678f;
        owner->model->tags.items[owner->muzzle].rotation[2]=0;
        owner->model->tags.items[owner->muzzle].rotation[3]=.70710678f;
        CHECK(!scene_jeep_gun_aim_pose(owner,position,basis,directions[0],aimed,tip));
        CHECK(fabsf(tip[8]-1)<.0001f && fabsf(aimed[8])<.0001f);
    }
    scene_jeep_gun_resources_close(&owner);puts("PASS actual Jeep aimed muzzle, fixed mount, vertical policy, rotated muzzle frame");return 0;
}
