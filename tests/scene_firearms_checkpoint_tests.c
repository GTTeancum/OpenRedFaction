/* Actual tables and live scene RFPL catalog/scope; no renderer or emulator. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"firearm checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_player_checkpoint_catalog catalog;rf_player_checkpoint saved={0},loaded,bad;
    unsigned char bytes[RF_PLAYER_CHECKPOINT_BYTES];uint32_t i;int32_t ids[4];
    scene_stream scene={0};rf_physics_sphere sphere={{0,0,0},.6f,0,0};
    CHECK(argc==2 && !rf_vpp_open(&tables,argv[1]));
    CHECK(!rf_weapon_supply_load(&tables,128*1024,&campaign_weapon_supply));rf_vpp_close(&tables);
    rf_scene_weapon_supply[3]=0x484d4750;
    campaign_player_damage.state.effects.class_health=100;campaign_player_damage.state.effects.class_armor=100;
    for(i=0;i<4;i++){ids[i]=rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[13+i]);CHECK(ids[i]>=0);campaign_extra_ids[i]=ids[i];}
    CHECK(!scene_checkpoint_player_catalog(&catalog));
    CHECK(catalog.supported[ids[1]] && catalog.supported[ids[2]]);
    CHECK(!catalog.supported[ids[0]] && !catalog.supported[ids[3]]);
    saved.player.catalog_hash=catalog.hash;saved.player.health=73;saved.player.armor=27;
    saved.position[0]=2;saved.position[1]=3;saved.position[2]=4;saved.body_angles[1]=.5f;saved.eye_angles[0]=-.2f;
    for(i=1;i<=2;i++){
        int32_t id=ids[i];const rf_weapon_acquire_definition *d=catalog.weapons+id;
        CHECK(d->magazine>2 && d->capacity>7 && d->ammo_type>=0 && d->ammo_type<32);
        saved.player.inventory.owned[id]=1;saved.player.inventory.loaded[id]=d->magazine-2;
        saved.player.inventory.reserve[d->ammo_type]=d->capacity-7;
    }
    for(i=1;i<=2;i++){
        saved.player.weapon=(uint32_t)ids[i];CHECK(!rf_player_checkpoint_encode(&saved,&catalog,bytes,sizeof(bytes)));
        memset(&loaded,0xa5,sizeof(loaded));CHECK(!rf_player_checkpoint_decode(bytes,sizeof(bytes),&catalog,&loaded));
        CHECK(!memcmp(&saved,&loaded,sizeof(saved)));
    }
    /* Scope uses a synthetic settled DEV body, but the actual scene gate and
     * original weapon IDs/definitions. No terrain operation is performed. */
    rf_scene_dev_room_enabled=campaign_spawn=1;strcpy(campaign_current_level,"glass_house.rfl");
    scene.terrain=(rf_geomod_terrain *)(uintptr_t)1;scene_actor_body.allocated_bytes=sizeof(scene_actor_body);
    scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;
    campaign_player_damage.state.effects.health=73;campaign_player_view.linked_handle=-1;rf_scene_actor_landing[1]=1;
    campaign_player_inventory=saved.player.inventory;CHECK(!scene_checkpoint_player_scope(&scene,1));
    for(i=0;i<4;i+=3){
        int32_t id=ids[i];bad=saved;bad.player.inventory.owned[id]=1;bad.player.weapon=(uint32_t)id;
        CHECK(rf_player_checkpoint_encode(&bad,&catalog,bytes,sizeof(bytes))==RF_FORMAT);
        campaign_player_inventory=bad.player.inventory;CHECK(scene_checkpoint_player_scope(&scene,1)==RF_RANGE);
    }
    campaign_player_inventory=saved.player.inventory;CHECK(!scene_checkpoint_player_scope(&scene,1));
    puts("PASS actual-table HMG/precision RFPL ownership, magazines, reserves, selection; live scope accepts both, MP/Undercover reject");
    return 0;
}
