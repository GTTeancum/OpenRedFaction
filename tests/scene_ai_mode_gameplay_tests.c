#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_mode_gameplay.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI mode line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body owner={0};rf_entity_ai_transition_state saved;
    rf_entity_pose pose={0};rf_level_navigation_node nodes[2]={0};
    const unsigned char route_indices[8]={0,0,0,0,1,0,0,0};
    scene_stream stream={0};rf_geometry_collision_world world={0};float eye[3]={0};uint32_t alerted;
    rf_object_registry_init(&campaign_registry);campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration));
    owner.view.linked_handle=-1;owner.damage.effects.health=100;owner.combat_alert=1;
    owner.script_move.active=1;owner.script_move.follow=2;
    CHECK(!campaign_set_ai_mode_live(NULL,owner.registration.handle,1,100));
    CHECK(!owner.combat_alert && !owner.script_move.active && owner.ai_mode.action_280==1);
    CHECK(!campaign_enemy_mode_admits(&owner,1,0) && !campaign_enemy_mode_admits(&owner,1,1));
    CHECK(!campaign_set_ai_mode_live(NULL,owner.registration.handle,2,101));
    CHECK(campaign_enemy_mode_admits(&owner,0,0));
    CHECK(!campaign_set_ai_mode_live(NULL,owner.registration.handle,11,102));
    CHECK(!campaign_enemy_mode_admits(&owner,0,0) && campaign_enemy_mode_admits(&owner,1,0));
    CHECK(!campaign_enemy_mode_admits(&owner,1,1));
    owner.combat_alert=1;CHECK(campaign_enemy_mode_admits(&owner,0,0));
    saved=owner.ai_mode;CHECK(campaign_set_ai_mode_live(NULL,owner.registration.handle,4,103)==RF_NOT_FOUND);
    CHECK(!memcmp(&saved,&owner.ai_mode,sizeof(saved)));
    CHECK(!campaign_set_ai_mode_live(NULL,owner.registration.handle,-1,104));
    CHECK(campaign_enemy_mode_admits(&owner,0,0));
    owner.ai_mode.action_280=13;CHECK(campaign_set_ai_mode_live(NULL,owner.registration.handle,2,105)==RF_NOT_FOUND);
    /* Actual scheduling gate must precede any firing or pose requirements. */
    owner.ai_mode.action_280=0;
    CHECK(!campaign_set_ai_mode_live(NULL,owner.registration.handle,1,106));
    stream.collision=&world;campaign_player_damage.state.effects.health=100;
    campaign_player_object.handle=42;
    owner.combat_alert=owner.combat_scripted=1;owner.combat_target=42;
    owner.script_move.follow=2;owner.script_move.active=1;
    CHECK(!campaign_enemy_tick(&stream,0,eye));
    CHECK(!rf_scene_enemy_combat[2] && !owner.script_move.active);
    /* A later Goto/Attack or damage alert must not bypass the inert scheduler. */
    campaign_poses.items=&pose;campaign_poses.count=1;
    owner.script_move.follow=0;owner.script_move.active=1;
    CHECK(!campaign_script_step(&stream,1.0f/60));
    CHECK(!owner.script_move.active && !rf_scene_enemy_aim[0]);
    CHECK(!campaign_enemy_hear_shot(eye,16,0,&alerted) && !alerted);
    /* Waiting restores real line-of-sight acquisition on its staggered tick. */
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_weapon_supply.names.count=8;campaign_weapon_supply.definitions[0].ammo_type=0;
    campaign_weapon_supply.definitions[0].magazine=6;
    campaign_primary[0].reload_seconds=1;campaign_primary[0].fire_seconds=1;campaign_primary[0].burst_count=1;
    owner.view.weapons[0]=0;owner.inventory.owned[0]=1;owner.inventory.loaded[0]=6;
    owner.eye_position[2]=3;owner.look.orientation[8]=-1;
    CHECK(!campaign_set_ai_mode_live(NULL,owner.registration.handle,2,107));
    CHECK(!campaign_enemy_tick(&stream,30,eye));
    CHECK(owner.combat_alert && owner.combat_due==60 && !rf_scene_enemy_combat[2]);
    /* Resume a concrete route retained from Follow_Waypoints, including cursor. */
    owner.ai_mode.action_280=2;
    campaign_navigation.nodes=nodes;campaign_navigation.count=2;
    nodes[1].candidate.position[0]=7;nodes[1].candidate.position[2]=-3;
    owner.script_move.path.indices=route_indices;owner.script_move.path.count=2;
    owner.script_move.path_index=1;owner.script_move.path_mode=2;owner.script_move.path_reverse=1;
    CHECK(!campaign_set_ai_mode_live(NULL,owner.registration.handle,1,110));
    CHECK(!campaign_set_ai_mode_live(NULL,owner.registration.handle,4,111));
    CHECK(owner.script_move.active && !owner.script_move.follow && !owner.script_move.stop);
    CHECK(owner.script_move.path_index==1 && owner.script_move.path_mode==2 && owner.script_move.path_reverse==1);
    CHECK(owner.script_move.target[0]==7 && owner.script_move.target[2]==-3);
    CHECK(!campaign_enemy_mode_admits(&owner,1,0) && !campaign_enemy_mode_admits(&owner,1,1));
    owner.combat_scripted=1;CHECK(campaign_enemy_mode_admits(&owner,1,0));
    owner.combat_scripted=0;owner.script_move.path_index=2;saved=owner.ai_mode;
    CHECK(campaign_set_ai_mode_live(NULL,owner.registration.handle,4,112)==RF_NOT_FOUND);
    CHECK(!memcmp(&saved,&owner.ai_mode,sizeof(saved)));
    puts("Live AI mode adapter: inert, wake, motion acquisition, unsupported preservation");return 0;
}
