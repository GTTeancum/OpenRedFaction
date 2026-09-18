#include <stdio.h>
#include "../src/diagnostic/scene_submarine_resources.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Sub resources line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}},tables={0};scene_submarine_resources *o=NULL;
    scene_submarine_weapon_state weapon={0};uint32_t i;char path[128];
    float pose[12];const float basis[9]={1,0,0,0,1,0,0,0,1},position[3]={0,0,0};
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(scene_submarine_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,1,&o)==RF_RANGE && !o);
    CHECK(!scene_submarine_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,8*1024*1024,&o));
    CHECK(!strcmp(o->chassis->model,"Sub_Mini01.v3m"));
    CHECK(o->chassis->physics.authored.mass==2500 && o->chassis->vitals.health==700);
    CHECK(o->chassis->physics.authored.use_kind==1 && o->chassis->physics.authored.use_radius==5);
    CHECK(o->chassis->movement.speed==6 && o->chassis->movement.acceleration==8);
    CHECK(o->chassis->rotation.maximum_velocity==4 && o->chassis->rotation.acceleration==2);
    /* Seven table overrides do not imply seven model collision spheres. */
    CHECK(o->chassis->render.sphere_count==1 && o->chassis->physics.spheres.count==7);
    CHECK(!strcmp(o->chassis->render.spheres[0].name,"csphere_1"));
    CHECK(o->chassis->tags.count==14 && o->chassis->seat==5 && o->primary[0]==6 && o->primary[1]==7);
    CHECK(o->cockpit->endpoint==40 && o->cockpit->geometry->count==19);
    {const rf_vfx_mesh *helper=o->cockpit->geometry->meshes[18];
     CHECK(helper && !strcmp(helper->prefix.name,"Sphere01"));
     CHECK(!(helper->prefix.timing.flags&1u) && !helper->materials);
     CHECK(helper->prefix.vertices==26 && helper->prefix.faces==48);
     CHECK(helper->keys.counts[0]==1 && helper->keys.counts[1]==1 && helper->keys.counts[2]==1);
     CHECK(!o->cockpit->geometry->instances[18]);
     for(i=0;i<18;i++)CHECK(o->cockpit->geometry->instances[i]);
    }
    CHECK(!strcmp(o->torpedo->model,"torpedo01.v3m"));
    CHECK(!scene_submarine_weapon_open(&tables,&o->chassis->tags,"primary_1",1024*1024,&weapon));
    CHECK(weapon.definition.damage==200 && weapon.definition.capacity==20 && weapon.reserve==0);
    CHECK(weapon.definition.speed==7 && weapon.definition.fire_seconds==3 && weapon.definition.lifetime==10);
    CHECK(o->resident_bytes>0 && o->peak_bytes>=o->resident_bytes && o->peak_bytes<=8*1024*1024);
    printf("SUB_RESOURCES resident=%u peak=%u hull=%u cockpit=%u torpedo=%u spheres=%u tags=%u\n",
        o->resident_bytes,o->peak_bytes,o->chassis->resident_bytes,o->cockpit->resident_bytes,
        o->torpedo->resident_bytes,o->chassis->render.sphere_count,o->chassis->tags.count);
    rf_vpp_close(&tables);rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    CHECK(!scene_driller_seat_pose(o->chassis,position,basis,pose));
    CHECK(!rf_static_model_tag_place(&o->chassis->tags,o->primary[1],basis,position,pose));
    scene_submarine_resources_close(&o);CHECK(!o);
    puts("Sub resources: installed hull, cockpit, launch tags and finite weapon metadata passed");return 0;
}
