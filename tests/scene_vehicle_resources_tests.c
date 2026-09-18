#include <stdio.h>
#include "../src/diagnostic/scene_vehicle_resources.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Vehicle resources line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    static const struct {const char *name,*model;uint32_t movement;float health;} cases[]={
        {"Driller01","Driller01.v3m",5,900},{"APC","APC.v3m",5,5000},
        {"Jeep01","Jeep01.v3m",5,400},{"sub","Sub_Mini01.v3m",7,700},{"Fighter01","Fighter01.v3m",9,900}};
    const float basis[9]={1,0,0,0,1,0,0,0,1},position[3]={10,2,-3};
    rf_vpp meshes={0},maps[4]={{0}};scene_driller_resources *owner=NULL,*legacy=NULL;
    uint32_t i,j;char path[128];float pose[12],reference[12];int status;
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
        CHECK(!scene_vehicle_resources_open("Installed_Game/tables.vpp",cases[i].name,"interface_1",&meshes,maps,4,8*1024*1024,&owner));
        CHECK(!strcmp(owner->model,cases[i].model));
        CHECK(owner->physics.authored.use_kind==1 && owner->physics.authored.movement_index==cases[i].movement);
        CHECK(owner->vitals.health==cases[i].health && owner->seat>=0);
        CHECK(!strcmp(owner->tags.items[owner->seat].name,"interface_1"));
        CHECK(owner->resident_bytes>0 && owner->peak_bytes>=owner->resident_bytes && owner->peak_bytes<=8*1024*1024);
        CHECK(!scene_driller_seat_pose(owner,position,basis,pose));
        if(!i){
            CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,8*1024*1024,&legacy));
            CHECK(!scene_driller_seat_pose(legacy,position,basis,reference));CHECK(!memcmp(pose,reference,sizeof(pose)));
            CHECK(owner->resident_bytes==legacy->resident_bytes && owner->peak_bytes==legacy->peak_bytes);
            CHECK(!memcmp(&owner->physics,&legacy->physics,sizeof(owner->physics)));
            scene_driller_resources_close(&legacy);
        }
        printf("VEHICLE_RESOURCE %s model=%s movement=%u seat=%d health=%g resident=%u peak=%u\n",
            cases[i].name,owner->model,cases[i].movement,owner->seat,owner->vitals.health,owner->resident_bytes,owner->peak_bytes);
        scene_driller_resources_close(&owner);CHECK(!owner);
    }
    status=scene_vehicle_resources_open("Installed_Game/tables.vpp","APC","missing_driver_seat",&meshes,maps,4,8*1024*1024,&owner);
    CHECK(status==RF_NOT_FOUND && !owner);
    CHECK(scene_vehicle_resources_open("Installed_Game/tables.vpp","APC","interface_1",&meshes,maps,4,1,&owner)==RF_RANGE && !owner);
    /* One retained owner remains valid after source archive lifetime ends. */
    CHECK(!scene_vehicle_resources_open("Installed_Game/tables.vpp","APC","interface_1",&meshes,maps,4,8*1024*1024,&owner));
    rf_vpp_close(&meshes);for(j=0;j<4;j++)rf_vpp_close(maps+j);
    CHECK(!scene_driller_seat_pose(owner,position,basis,pose));scene_driller_resources_close(&owner);
    puts("Vehicle resources: actual classes, exact seats, Driller parity, budgets and retained lifetime passed");return 0;
}
