#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_driller_bit_animation.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller bit animation line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}};scene_driller_resources *chassis=NULL;scene_driller_bit_animation *bits=NULL;
    float origin[3]={30,5.4f,-167},basis[9]={1,0,0,0,1,0,0,0,1},tag[12],start[12],quarter[12],full[12];
    char path[128];uint32_t i,j;
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;++i){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&chassis));
    CHECK(!scene_driller_bit_animation_open(&chassis->tags,&meshes,maps,4,1024*1024,&bits));
    CHECK(chassis->render.part_count==1 && bits->model.render.part_count==1 && bits->model.render.lod_count>0);
    CHECK(!strcmp(bits->model.model,"Driller01_Drillbit.v3m") && bits->resident_bytes<=1024*1024);
    rf_vpp_close(&meshes);for(i=0;i<4;++i)rf_vpp_close(maps+i);
    /* Owned resources and tag data remain usable after all source archives close. */
    for(i=0;i<2;++i){
        CHECK(!rf_static_model_tag_place(&chassis->tags,bits->tags[i],basis,origin,tag));
        CHECK(!scene_driller_bit_animation_pose(bits,&chassis->tags,origin,basis,i,0,start));
        CHECK(!scene_driller_bit_animation_pose(bits,&chassis->tags,origin,basis,i,1.57079632679f,quarter));
        CHECK(!scene_driller_bit_animation_pose(bits,&chassis->tags,origin,basis,i,6.28318530718f,full));
        CHECK(!memcmp(start+9,tag+9,12) && !memcmp(quarter+9,start+9,12));
        for(j=0;j<9;++j)CHECK(fabsf(full[j]-start[j])<1e-5f);
        for(j=0;j<3;++j){CHECK(fabsf(quarter[j]-start[3+j])<1e-5f);CHECK(fabsf(quarter[6+j]-start[6+j])<1e-5f);}
    }
    printf("PASS actual drillbit asset and two authored pivots,phase rotation,closed archives; resident%u peak%u\n",bits->resident_bytes,bits->peak_bytes);
    scene_driller_bit_animation_close(&bits);scene_driller_resources_close(&chassis);return 0;
}
