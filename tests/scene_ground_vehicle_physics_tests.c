#include <stdio.h>
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_ground_vehicle_physics.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"ground physics line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp tables={0},meshes={0};rf_vpp_entry entry;char *text;
    const char *names[3]={"APC","Jeep01","Driller01"};uint32_t i,j;
    float position[3]={0,10,0},basis[9]={1,0,0,0,1,0,0,0,1};
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_vpp_find(&tables,"entity.tbl",&entry));text=malloc(entry.size);CHECK(text);
    CHECK(!rf_vpp_read(&tables,&entry,0,text,entry.size));
    for(i=0;i<3;i++){
        scene_driller_resources resource={0};scene_driller_physics physics,kept,legacy;
        rf_model_file *file=malloc(sizeof(*file));CHECK(file);
        CHECK(!rf_entity_assets_load("Installed_Game/tables.vpp",names[i],"",&resource.assets,2*1024*1024));
        CHECK(!rf_model_compiled_filename(resource.assets.model,resource.model,".v3m"));
        CHECK(!rf_entity_physics_config_load(&tables,names[i],2*1024*1024,&resource.physics));
        CHECK(!rf_entity_movement_load(&tables,names[i],2*1024*1024,&resource.movement));
        CHECK(!rf_entity_rotation_values_read(text,entry.size,names[i],&resource.rotation));
        CHECK(!rf_model_file_open(file,&meshes,resource.model));
        CHECK(!rf_static_render_resource_open(file,4*1024*1024,&resource.render));
        CHECK(!scene_ground_vehicle_physics_initialize(names[i],&resource.physics,&resource.movement,&resource.rotation,
            &resource.render,position,basis,9.8f,&physics));
        CHECK(physics.spring_count==(i==2?6u:4u));CHECK(physics.parameters.mass==resource.physics.authored.mass);
        CHECK(physics.parameters.maximum_speed==resource.movement.speed && physics.parameters.acceleration==resource.movement.acceleration);
        CHECK(physics.parameters.maximum_turn==resource.rotation.maximum_velocity && physics.parameters.turn_acceleration==resource.rotation.acceleration);
        for(j=0;j<physics.spring_count;j++){
            CHECK(physics.springs[j].length==.25f);
            if(i==0)CHECK(physics.springs[j].constant==2.5f);
            if(i==1)CHECK(physics.springs[j].constant==1.65f || physics.springs[j].constant==3.3f);
        }
        if(i==2){CHECK(!scene_driller_physics_initialize(&resource,position,basis,9.8f,&legacy));CHECK(!memcmp(&legacy,&physics,sizeof(physics)));}
        kept=physics;CHECK(scene_ground_vehicle_physics_initialize("Fighter01",&resource.physics,&resource.movement,
            &resource.rotation,&resource.render,position,basis,9.8f,&physics)==RF_FORMAT);CHECK(!memcmp(&physics,&kept,sizeof(physics)));
        printf("GROUND_VEHICLE %s movement=%d spheres=%u springs=%u mass=%g speed=%g turn=%g owner_bytes=%u\n",
            names[i],resource.physics.authored.movement_index,physics.sphere_count,physics.spring_count,
            physics.parameters.mass,physics.parameters.maximum_speed,physics.parameters.maximum_turn,(unsigned)sizeof(physics));
        rf_static_render_resource_close(&resource.render);free(file);
    }
    free(text);rf_vpp_close(&tables);rf_vpp_close(&meshes);puts("PASS authored ground vehicle physics and unchanged Driller math");return 0;
}
