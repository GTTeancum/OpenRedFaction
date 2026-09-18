#include "rf/entity_assets.h"
#include <stdio.h>
#include "../src/diagnostic/scene_pickup_class_names.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_item_definition item;int kind;
    static const char *weapons[]={"12mm handgun","Assault Rifle","Riot Stick","Shotgun","Rocket Launcher","Grenade","Sniper Rifle","rail_gun","Remote Charge","Remote Charge Detonator","Flamethrower"};
    static const unsigned char gives[]={1,1,1,1,1,1,0,0,0,1,0};
    CHECK(pickup_class("First Aid Kit")==10 && pickup_class("SHOTGUN")==8);
    CHECK(pickup_class("not a pickup")==-1 && pickup_class(NULL)==-1);
    CHECK(scene_pickup_weapon_slot(-1)==-1 && scene_pickup_weapon_slot(SCENE_PICKUP_CLASSES)==-1);
    CHECK(rf_vpp_open(&tables,argc>1?argv[1]:"Installed_Game/tables.vpp")==RF_OK);
    for(kind=11;kind<SCENE_PICKUP_CLASSES;kind++) {
        int slot=scene_pickup_weapon_slot(kind);
        CHECK(slot>=0 && slot<11);
        CHECK(rf_item_definition_load(&tables,pickup_classes[kind],128*1024,&item)==RF_OK);
        CHECK(item.mesh_kind==1 && scene_pickup_name_equal(item.weapon,weapons[slot]));
        CHECK(item.gives_weapon==gives[kind-11] && !(item.flags&1));
    }
    rf_vpp_close(&tables);puts("All added pickup classes match installed table static models and weapon/ammo effects");return 0;
}
