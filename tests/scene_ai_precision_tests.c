#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI precision line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    scene_stream scene={0};rf_geometry_collision_world world={0};campaign_npc_body owner={0};
    rf_weapon_acquire_definition ammo[8]={0};rf_physics_sphere sphere={0};rf_vpp tables={0};
    rf_collision_solid_view mover={0};rf_collision_face face={0};
    float vertices[4][3]={{1,-2,-2},{1,2,-2},{1,2,2},{1,-2,2}},eye[3]={0,0,0},before;uint32_t i;
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!rf_weapon_primary_load(&tables,"Sniper Rifle",128*1024,campaign_primary+6));
    CHECK(!rf_weapon_primary_load(&tables,"rail_gun",128*1024,campaign_primary+7));rf_vpp_close(&tables);
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_weapon_supply.names.count=8;
    for(i=6;i<8;i++){ammo[i].ammo_type=i;ammo[i].magazine=(int32_t)campaign_primary[i].magazine;owner.inventory.owned[i]=1;owner.inventory.loaded[i]=ammo[i].magazine;}
    memcpy(campaign_weapon_supply.definitions,ammo,sizeof(ammo));
    scene.collision=&world;campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    owner.registration.view=&owner.view;owner.registration.handle=123;owner.damage.effects.health=100;
    owner.eye_position[0]=3;owner.look.orientation[6]=-1;owner.combat_alert=1;owner.combat_navigation_due=10000;owner.pain.animation_lock=-1;
    rf_object_registry_init(&campaign_registry);campaign_player_view.flags_7c=8;
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_damage.state.effects.handle=campaign_player_object.handle;
    campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.class_health=10000;
    for(i=0;i<11;i++)campaign_player_damage.factors[i]=1;
    scene_actor_body.allocated_bytes=1;scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;sphere.radius=.5f;
    for(i=0;i<3;i++){scene_actor_body.state.orientation[i*4]=1;scene_actor_body.state.bounds.minimum[i]=-.5f;scene_actor_body.state.bounds.maximum[i]=.5f;}
    face.vertices=vertices;face.count=4;face.plane[0]=1;face.plane[3]=-1;
    face.minimum[0]=face.maximum[0]=1;face.minimum[1]=face.minimum[2]=-2;face.maximum[1]=face.maximum[2]=2;
    mover.flat_faces=&face;mover.flat_count=1;
    for(i=0;i<3;i++){mover.input_matrix[i][i]=mover.output_matrix[i][i]=1;mover.minimum[i]=-2;mover.maximum[i]=2;}
    campaign_movers.views=&mover;campaign_movers.count=1;
    owner.view.weapons[0]=6;
    CHECK(!campaign_enemy_tick(&scene,100,eye));
    CHECK(owner.inventory.loaded[6]==ammo[6].magazine && campaign_player_damage.state.effects.health==10000);
    owner.view.weapons[0]=7;before=campaign_player_damage.state.effects.health;
    CHECK(!campaign_enemy_tick(&scene,110,eye));
    CHECK(owner.inventory.loaded[7]==ammo[7].magazine-1);
    CHECK(campaign_player_damage.state.effects.health<before && rf_scene_enemy_combat[2]==1);
    CHECK(fabsf((before-campaign_player_damage.state.effects.health)-combat_enemy_primary_damage(campaign_primary+7))<.01f);
    campaign_movers.count=0;owner.view.weapons[0]=6;owner.combat_due=0;before=campaign_player_damage.state.effects.health;
    CHECK(!campaign_enemy_tick(&scene,200,eye));
    CHECK(owner.inventory.loaded[6]==ammo[6].magazine-1);
    CHECK(campaign_player_damage.state.effects.health<before && rf_scene_enemy_combat[2]==2);
    CHECK(fabsf((before-campaign_player_damage.state.effects.health)-combat_enemy_primary_damage(campaign_primary+6))<.01f);
    puts("Scene NPC precision: sniper cover, rail traversal, authored damage and ammo debit pass");return 0;
}
