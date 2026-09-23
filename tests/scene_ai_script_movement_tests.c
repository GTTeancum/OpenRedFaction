#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"script movement scheduler line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    campaign_npc_body owner={0};rf_level_owned_entity record={0};scene_stream scene={0};
    rf_geometry_collision_world world={0};rf_collision_room_liquid_view dry={0};
    rf_level_navigation_node nodes[2]={0};rf_level_event event={0},attack={0};
    unsigned char route[]={1,0,0,0,6,0,'p','a','t','r','o','l',2,0,0,0,0,0,0,0,1,0,0,0};
    unsigned char saved[sizeof(owner.script_move)];const char *orders[3]={"Goto","Goto_Player","Follow_Waypoints"};
    float far_eye[3]={0,0,30},near_eye[3]={0,0,5};uint32_t i;
    scene.collision=&world;world.liquids=&dry;
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;campaign_seeds.records.items=&record;campaign_seeds.records.count=1;record.record.uid=201;
    rf_object_registry_init(&campaign_registry);
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    owner.damage.effects.health=campaign_player_damage.state.effects.health=100;
    owner.view.linked_handle=-1;owner.view.weapons[0]=4;owner.inventory.owned[4]=1;
    owner.look.orientation[8]=1;owner.pain.animation_lock=-1;
    campaign_pistol_id=0;campaign_rifle_id=1;campaign_riot_id=2;campaign_shotgun_id=3;
    campaign_rocket_id=4;campaign_grenade_id=5;campaign_sniper_id=6;campaign_rail_id=7;
    campaign_weapon_supply.names.count=8;campaign_weapon_supply.definitions[4]=(rf_weapon_acquire_definition){4,6,6};
    campaign_primary[4].fire_seconds=1;campaign_primary[4].reload_seconds=1;campaign_primary[4].burst_count=1;
    campaign_primary[4].ai_attack_range=8;campaign_primary[4].damage=100;campaign_primary[4].ai_damage_scale[0]=1;
    campaign_rocket.speed=20;campaign_rocket.lifetime=10;campaign_rocket.collision_radius=.1f;
    rf_scene_dev_room_enabled=0;scene_rocket_resources=1;
    campaign_navigation.nodes=nodes;campaign_navigation.count=2;nodes[0].candidate.position[0]=10;nodes[1].candidate.position[0]=20;
    campaign_waypoints=route;campaign_waypoint_bytes=sizeof(route);
    for(i=0;i<3;i++){
        memset(&event,0,sizeof(event));event.uid=900+i;strcpy(event.type,orders[i]);event.position[0]=12;
        strcpy(event.texts[0],"patrol");strcpy(event.texts[1],"One way");
        CHECK(!campaign_script_move(NULL,owner.registration.handle,&event,1));
        CHECK(owner.script_move.active&&owner.script_move.follow==(i==1));
        CHECK(owner.ai_mode.action_280==0); /* Direct route events do not set mode4. */
        owner.combat_alert=1;owner.combat_target=campaign_player_object.handle;owner.combat_due=0;owner.combat_navigation_due=0;
        owner.inventory.loaded[4]=3;memcpy(saved,&owner.script_move,sizeof(saved));scene_ai_rocket_reset();
        memcpy(scene_actor_body.state.position,far_eye,12);
        CHECK(!campaign_enemy_tick(&scene,100,far_eye));
        CHECK(!memcmp(saved,&owner.script_move,sizeof(saved))&&owner.inventory.loaded[4]==3&&!scene_ai_rocket_pending());
        /* In-range firing remains allowed while authored locomotion keeps its target. */
        owner.combat_due=0;memcpy(scene_actor_body.state.position,near_eye,12);
        CHECK(!campaign_enemy_tick(&scene,101,near_eye));
        CHECK(!memcmp(saved,&owner.script_move,sizeof(saved))&&owner.inventory.loaded[4]==2&&rf_scene_ai_rockets[0]==1);
    }
    /* Actual damage notification alerts without replacing authored locomotion. */
    CHECK(!campaign_script_move(NULL,owner.registration.handle,&event,1));
    memcpy(saved,&owner.script_move,sizeof(saved));combat_frame=150;
    combat_notify(NULL,RF_DAMAGE_AI_REACTION,owner.registration.handle,10,campaign_player_object.handle);
    CHECK(owner.combat_alert&&owner.combat_target==campaign_player_object.handle&&owner.combat_due==180);
    CHECK(!owner.combat_scripted&&!memcmp(saved,&owner.script_move,sizeof(saved)));
    CHECK(rf_scene_enemy_retaliation[0]==1&&rf_scene_enemy_retaliation[1]==owner.registration.handle);
    /* An already-sounding alarm still wakes linked actors; this isolates its
     * real gameplay callback without loading or fabricating an audio backend. */
    CHECK(!campaign_script_move(NULL,owner.registration.handle,&event,1));
    memcpy(saved,&owner.script_move,sizeof(saved));
    {rf_level_event alarm={0};rf_level_link_target linked={0};
     alarm.uid=1001;alarm.link_count=1;linked.kind=1;linked.value=owner.registration.handle;
     rf_scene_alarm[4]=1;campaign_alarm_deadline=17000;
     CHECK(!campaign_alarm(NULL,&alarm,&linked,2500,1));
     CHECK(rf_scene_alarm[6]==1&&rf_scene_alarm[7]==1&&campaign_alarm_deadline==17000);
     CHECK(owner.combat_alert&&owner.combat_target==campaign_player_object.handle&&!owner.combat_scripted);
     CHECK(!memcmp(saved,&owner.script_move,sizeof(saved)));}
    /* Explicit Attack cancels the authored order and may publish pursuit. */
    attack.uid=999;attack.words[0]=201;strcpy(attack.name,"player");
    CHECK(!campaign_script_attack(NULL,&attack,NULL,1));
    CHECK(owner.combat_scripted==1&&!owner.script_move.active);
    owner.combat_due=UINT32_MAX;memcpy(scene_actor_body.state.position,far_eye,12);
    CHECK(!campaign_enemy_tick(&scene,200,far_eye));
    CHECK(owner.script_move.active&&owner.script_move.follow==2&&!memcmp(owner.script_move.target,far_eye,12));
    /* A live Enable_Navpoint change must retire a cached route through that
     * node; an unrelated change leaves the route intact. Reopening a node
     * also releases an active scripted actor's failed-route retry delay. */
    {float radius=2.f;
     for(i=0;i<2;i++){nodes[i].candidate.radius=radius;memcpy(&nodes[i].candidate.retained_018,&radius,4);}
     owner.script_move.active=1;owner.script_move.route_index=1;owner.script_move.retry=13;
     owner.navigation.retained.count=3;
     owner.navigation.retained.nodes[0]=&owner.navigation.start;
     owner.navigation.retained.nodes[1]=&nodes[0].candidate;
     owner.navigation.retained.nodes[2]=&owner.navigation.goal;
     CHECK(!campaign_navpoint_set(&campaign_navigation,1,0));
     CHECK(owner.navigation.retained.count==3&&owner.script_move.retry==13);
     CHECK(!campaign_navpoint_set(&campaign_navigation,0,0));
     CHECK(owner.navigation.retained.count==0&&owner.script_move.route_index==1&&owner.script_move.retry==0);
     owner.script_move.retry=13;
     CHECK(!campaign_navpoint_set(&campaign_navigation,0,1));
     CHECK(owner.script_move.retry==0&&nodes[0].candidate.radius==radius);}
    puts("PASS authored Goto/Follow targets survive ordinary pursuit, firing remains active, explicit Attack supersedes");return 0;
}
