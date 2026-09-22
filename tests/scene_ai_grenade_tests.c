#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_grenade.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI grenade line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int enemy_scheduler_launch(void)
{
    campaign_npc_body owner={0};scene_stream scene={0};rf_geometry_collision_world world={0};
    float player_eye[3]={0,0,5};uint32_t shooter,live=0,i;
    rf_scene_dev_room_enabled=0;scene_grenade_resources=0;
    scene.collision=&world;campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration));
    shooter=owner.registration.handle;
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.class_health=100;
    owner.damage.effects.health=100;owner.view.linked_handle=-1;owner.view.weapons[0]=5;
    owner.inventory.owned[5]=1;owner.inventory.reserve[5]=2;owner.look.orientation[8]=1;
    owner.combat_alert=1;owner.combat_target=campaign_player_object.handle;owner.combat_navigation_due=1000;
    owner.pain.animation_lock=-1;
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_weapon_supply.names.count=8;
    campaign_primary[5].fire_seconds=3;campaign_primary[5].burst_count=1;
    campaign_primary[5].ai_attack_range=8;
    /* Optional presentation owner absent: scheduler must still emit real flight. */
    campaign_poses.items=NULL;campaign_poses.count=0;
    scene_ai_grenade_reset();
    CHECK(!campaign_enemy_tick(&scene,100,player_eye));
    CHECK(owner.inventory.reserve[5]==2 && !scene_ai_grenade_pending());
    CHECK(!campaign_player_inventory.owned[5] && !scene.player_weapon[5]);
    scene_grenade_resources=1;
    CHECK(!campaign_enemy_tick(&scene,100,player_eye));
    CHECK(owner.inventory.reserve[5]==1 && owner.inventory.loaded[5]==0);
    CHECK(!campaign_player_inventory.owned[5] && !scene.player_weapon[5]);
    for(i=0;i<SCENE_AI_GRENADE_CAPACITY;i++)if(scene_ai_grenades[i].projectile.flight.lifecycle.active){
        ++live;CHECK(scene_ai_grenades[i].source==shooter);
        CHECK(scene_ai_grenades[i].projectile.flight.velocity[1]>0);
    }
    CHECK(live==1 && campaign_player_damage.state.effects.health==100);
    CHECK(owner.combat_due>100);
    CHECK(!campaign_enemy_tick(&scene,101,player_eye));
    CHECK(owner.inventory.reserve[5]==1 && campaign_player_damage.state.effects.health==100);
    /* Actual shared blast must attribute a lethal NPC grenade to its shooter,
     * never the player-source wrapper. NPC body is absent, so only player is
     * geometrically eligible in this deliberately narrow attribution fixture. */
    campaign_player_view.flags_7c=8;
    campaign_player_damage.state.effects.handle=campaign_player_object.handle;
    campaign_player_damage.state.effects.armor=0;
    campaign_player_damage.state.responsible_handle=UINT32_MAX;
    for(i=0;i<11;i++)campaign_player_damage.factors[i]=1;
    memset(scene_actor_body.state.position,0,sizeof(scene_actor_body.state.position));
    {const float blast_origin[3]={0};
     CHECK(!scene_explosion_blast_source(&scene,102,blast_origin,10000,8,shooter,3));}
    CHECK(campaign_player_damage.state.effects.health<=0);
    CHECK(campaign_player_damage.state.responsible_handle==shooter);
    CHECK(campaign_player_damage.state.responsible_handle!=campaign_player_object.handle);
    return 0;
}
int main(void)
{
    campaign_npc_body owner={0};float target[3]={0,0,5},direction[3];uint32_t i;
    CHECK(!scene_ai_grenade_aim(target,10,9.8f,direction));
    {float time=5/(10*direction[2]);CHECK(fabsf(10*direction[1]*time-.5f*9.8f*time*time)<.001f);}
    target[2]=100;CHECK(scene_ai_grenade_aim(target,10,9.8f,direction)==RF_NOT_FOUND);target[2]=5;
    campaign_grenade_id=5;campaign_weapon_supply.definitions[5].ammo_type=5;
    campaign_grenade.speed=10;campaign_grenade.collision_radius=.15f;campaign_grenade.lifetime=5;
    campaign_primary[5].damage=150;campaign_primary[5].ai_damage_scale[0]=1;scene_gravity.vector[1]=-9.8f;
    owner.registration.handle=123;owner.inventory.owned[5]=1;owner.inventory.reserve[5]=9;
    scene_ai_grenade_reset();CHECK(!scene_ai_grenade_launch(&owner,target));
    CHECK(owner.inventory.reserve[5]==8 && scene_ai_grenades[0].source==123 && scene_ai_grenades[0].damage==150);
    CHECK(scene_ai_grenades[0].projectile.flight.velocity[1]>0 && scene_ai_grenades[0].projectile.flight.lifecycle.active);
    for(i=1;i<SCENE_AI_GRENADE_CAPACITY;i++)CHECK(!scene_ai_grenade_launch(&owner,target));
    CHECK(scene_ai_grenade_launch(&owner,target)==RF_NOT_FOUND && owner.inventory.reserve[5]==1);
    scene_ai_grenade_reset();owner.inventory.reserve[5]=0;
    CHECK(scene_ai_grenade_launch(&owner,target)==RF_NOT_FOUND);
    CHECK(!enemy_scheduler_launch());
    puts("NPC grenade ballistic launch, finite reserve and fixed-pool admission");return 0;
}
