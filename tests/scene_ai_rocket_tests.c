#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_rocket.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI rocket line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int scheduler_launch(void)
{
    campaign_npc_body owner={0};scene_stream scene={0};rf_geometry_collision_world world={0};
    rf_collision_room_liquid_view dry={0};float eye[3]={0,0,5};uint32_t i,live=0,source;
    scene.collision=&world;world.liquids=&dry;campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    memset(&campaign_player_object,0,sizeof(campaign_player_object));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration));source=owner.registration.handle;
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_damage.state.effects.health=100;owner.damage.effects.health=100;
    owner.view.linked_handle=-1;owner.view.weapons[0]=4;owner.inventory.owned[4]=1;owner.inventory.loaded[4]=2;
    owner.look.orientation[8]=1;owner.combat_alert=1;owner.combat_target=campaign_player_object.handle;
    owner.combat_navigation_due=1000;owner.pain.animation_lock=-1;rf_scene_dev_room_enabled=1;
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_weapon_supply.names.count=8;campaign_weapon_supply.definitions[4].ammo_type=4;
    campaign_weapon_supply.definitions[4].magazine=6;
    campaign_primary[4].fire_seconds=1;campaign_primary[4].reload_seconds=1;
    campaign_primary[4].burst_count=1;campaign_primary[4].ai_attack_range=20;
    campaign_poses.items=NULL;campaign_poses.count=0;scene_ai_rocket_reset();
    CHECK(!campaign_enemy_tick(&scene,100,eye));
    CHECK(owner.inventory.loaded[4]==1 && campaign_player_damage.state.effects.health==100);
    for(i=0;i<SCENE_AI_ROCKET_CAPACITY;i++)if(scene_ai_rockets[i].flight.active){
        ++live;CHECK(scene_ai_rockets[i].source==source && scene_ai_rockets[i].flight.velocity[2]>0);}
    CHECK(live==1 && owner.combat_due>100);
    CHECK(!campaign_enemy_tick(&scene,101,eye));
    CHECK(owner.inventory.loaded[4]==1 && campaign_player_damage.state.effects.health==100);
    return 0;
}
int main(void)
{
    campaign_npc_body owner={0};scene_stream scene={0};rf_geometry_collision_world world={0};
    rf_collision_room_liquid_view dry_liquid={0};
    rf_physics_sphere source_sphere={{0,0,0},.5f,0,0},player_sphere={{0,0,0},.5f,0,0};
    float target[3]={0,0,5},start[3]={0},delta[3]={0,0,10};uint32_t i,matched,liquid;
    rf_weapon_flight_contact contact;scene_ai_projectile_query query={&scene,123};
    campaign_rocket_id=4;campaign_rocket.speed=20;campaign_rocket.lifetime=10;campaign_rocket.collision_radius=.1f;
    campaign_primary[4].damage=100;campaign_primary[4].ai_damage_scale[0]=.5f;
    owner.registration.handle=123;owner.registration.view=&owner.view;owner.damage.effects.health=100;
    owner.inventory.owned[4]=1;owner.inventory.loaded[4]=9;
    owner.body.allocated_bytes=1;owner.body.spheres.items=&source_sphere;owner.body.spheres.count=1;
    owner.body.state.orientation[0]=owner.body.state.orientation[4]=owner.body.state.orientation[8]=1;
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;scene.collision=&world;world.liquids=&dry_liquid;
    campaign_player_object.view=&campaign_player_view;campaign_player_object.handle=321;campaign_player_damage.state.effects.health=100;
    scene_actor_body.spheres.items=&player_sphere;scene_actor_body.spheres.count=1;
    scene_actor_body.state.position[2]=5;scene_actor_body.state.orientation[0]=scene_actor_body.state.orientation[4]=scene_actor_body.state.orientation[8]=1;
    scene_ai_rocket_reset();CHECK(!scene_ai_rocket_launch(&owner,target));
    CHECK(owner.inventory.loaded[4]==8 && scene_ai_rockets[0].source==123 && scene_ai_rockets[0].damage==50);
    CHECK(scene_ai_rocket_pending() && scene_ai_rockets[0].flight.velocity[2]==20 && scene_ai_rockets[0].basis[8]==1);
    CHECK(!scene_ai_projectile_sweep(&query,start,delta,.1f,4,&contact,&liquid,&matched));
    CHECK(matched && !liquid && contact.object==SCENE_AI_PROJECTILE_PLAYER && contact.hit.fraction>.4f);
    CHECK(!scene_ai_projectile_solid_sweep(&query,start,delta,.1f,&contact,&matched));
    CHECK(matched && contact.object==SCENE_AI_PROJECTILE_PLAYER && contact.hit.fraction>.4f);
    /* The fired rocket travels before contact and survives the shooter's death. */
    owner.damage.effects.health=0;
    {rf_weapon_flight_liquid_event event;rf_weapon_flight_liquid_policy policy={0,0,-1,-1};
     CHECK(!rf_weapon_flight_step_liquid(&scene_ai_rockets[0].flight,1.f/60,&scene_ai_rockets[0].liquid,&policy,scene_ai_projectile_sweep,&query,&event));
     CHECK(!event.terminal.kind && scene_ai_rockets[0].flight.position[2]>0 && scene_ai_rockets[0].flight.position[2]<1);
     for(i=0;i<30 && scene_ai_rockets[0].flight.active;i++)
        CHECK(!rf_weapon_flight_step_liquid(&scene_ai_rockets[0].flight,1.f/60,&scene_ai_rockets[0].liquid,&policy,scene_ai_projectile_sweep,&query,&event));
     CHECK(event.terminal.kind==1 && event.terminal.contact.object==SCENE_AI_PROJECTILE_PLAYER && scene_ai_rockets[0].source==123);}
    scene_ai_rocket_reset();owner.damage.effects.health=100;owner.inventory.loaded[4]=9;
    for(i=0;i<SCENE_AI_ROCKET_CAPACITY;i++)CHECK(!scene_ai_rocket_launch(&owner,target));
    CHECK(scene_ai_rocket_launch(&owner,target)==RF_NOT_FOUND && owner.inventory.loaded[4]==1);
    scene_ai_rocket_reset();owner.inventory.loaded[4]=0;CHECK(scene_ai_rocket_launch(&owner,target)==RF_NOT_FOUND);
    CHECK(!scheduler_launch());
    puts("NPC rockets finite ammo, bounded pool, source exclusion, player collision and real flight passed");return 0;
}
