#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"unsupported NPC weapon line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body owner={0};scene_stream scene={0};rf_geometry_collision_world world={0};
    rf_collision_room_liquid_view dry={0};rf_physics_sphere sphere={{0,0,0},.5f,0,0};
    rf_weapon_inventory before;float eye[3]={0,0,5};uint32_t i;
    scene.collision=&world;world.liquids=&dry;campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    rf_object_registry_init(&campaign_registry);
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_view.flags_7c=8;campaign_player_damage.state.effects.handle=campaign_player_object.handle;
    campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.class_health=100;
    for(i=0;i<11;i++)campaign_player_damage.factors[i]=1;
    scene_actor_body.allocated_bytes=1;scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;
    scene_actor_body.state.position[2]=5;
    for(i=0;i<3;i++){scene_actor_body.state.orientation[i*4]=1;scene_actor_body.state.bounds.minimum[i]=-.5f;scene_actor_body.state.bounds.maximum[i]=.5f;}
    scene_actor_body.state.bounds.minimum[2]+=5;scene_actor_body.state.bounds.maximum[2]+=5;
    owner.damage.effects.health=100;owner.view.linked_handle=-1;owner.view.weapons[0]=8;
    owner.inventory.owned[8]=1;owner.inventory.loaded[8]=32;owner.inventory.reserve[8]=7;before=owner.inventory;
    owner.look.orientation[8]=1;owner.combat_alert=1;owner.combat_target=campaign_player_object.handle;
    owner.combat_navigation_due=1000;owner.pain.animation_lock=-1;rf_scene_dev_room_enabled=1;
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_weapon_supply.names.count=9;strcpy(campaign_weapon_supply.names.names[8],"riot shield");
    campaign_weapon_supply.definitions[8].ammo_type=8;campaign_weapon_supply.definitions[8].magazine=32;
    /* Hostile, aimed, in range and due: an unsupported held shield must not
     * reach the old generic10-damage hitscan fallback. */
    CHECK(!campaign_enemy_tick(&scene,100,eye));
    CHECK(!campaign_enemy_tick(&scene,200,eye));
    CHECK(campaign_player_damage.state.effects.health==100 && !rf_scene_enemy_combat[2]);
    CHECK(!rf_scene_enemy_spread[0] && !rf_scene_enemy_damage_kinds[9]);
    CHECK(!memcmp(&before,&owner.inventory,sizeof(before)) && owner.view.weapons[0]==8);
    for(i=0;i<SCENE_AI_ROCKET_CAPACITY;i++)CHECK(!scene_ai_rockets[i].flight.active);
    for(i=0;i<SCENE_AI_GRENADE_CAPACITY;i++)CHECK(!scene_ai_grenades[i].projectile.flight.lifecycle.active);
    puts("Unsupported NPC held item emits no hitscan, projectile, damage or ammo debit");return 0;
}
