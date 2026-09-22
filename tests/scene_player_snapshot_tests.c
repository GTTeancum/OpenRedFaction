#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"player snapshot line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    scene_stream stream={0};rf_player_checkpoint player,before;rf_player_checkpoint_catalog catalog;
    unsigned char wire[RF_PLAYER_CHECKPOINT_BYTES];rf_player_checkpoint decoded;uint32_t i;
    campaign_spawn=1;scene_actor_body.allocated_bytes=1;scene_actor_body.spheres.count=1;
    campaign_player_view.linked_handle=-1;rf_scene_actor_landing[1]=1;
    campaign_player_damage.state.effects.health=75;campaign_player_damage.state.effects.armor=20;
    campaign_player_damage.state.effects.class_health=100;campaign_player_damage.state.effects.class_armor=100;
    campaign_weapon_supply.names.count=SCENE_WEAPON_SLOTS;
    for(i=0;i<SCENE_WEAPON_SLOTS;i++){
        strcpy(campaign_weapon_supply.names.names[i],campaign_weapon_names[i]);
        campaign_weapon_supply.definitions[i]=(rf_weapon_acquire_definition){0,100,12};
    }
    campaign_player_inventory.owned[0]=1;campaign_player_inventory.loaded[0]=7;campaign_player_inventory.reserve[0]=23;
    scene_actor_body.state.position[0]=4;actor_look.state.body_angles[1]=.25f;actor_look.state.eye_angles[0]=.1f;
    /* Ordinary world with NPCs/movers: player snapshot is independent of DEV
     * terrain; the whole-scene gate must still reject this old profile. */
    campaign_npc_body_count=2;campaign_movers.count=1;
    CHECK(scene_checkpoint_player_scope(&stream,1)==RF_RANGE);
    CHECK(!scene_checkpoint_player_snapshot(&stream,0,&player,&catalog));
    CHECK(player.player.health==75&&player.player.armor==20&&player.position[0]==4);
    CHECK(!rf_player_checkpoint_encode(&player,&catalog,wire,sizeof(wire)));
    CHECK(!rf_player_checkpoint_decode(wire,sizeof(wire),&catalog,&decoded));
    CHECK(decoded.player.inventory.loaded[0]==7&&decoded.player.inventory.reserve[0]==23);
    before=player;combat_trigger.held=1;
    CHECK(scene_checkpoint_player_snapshot(&stream,0,&player,&catalog)==RF_RANGE&&!memcmp(&player,&before,sizeof(player)));
    puts("PASS ordinary player snapshot vitals/ammo/pose with transient rejection and legacy gate retained");return 0;
}
