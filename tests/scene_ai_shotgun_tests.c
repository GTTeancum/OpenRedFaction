#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"shotgun line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void quiet_notify(void *context,uint32_t kind,uint32_t target,float value,uint32_t source)
{(void)context;(void)kind;(void)target;(void)value;(void)source;}
int main(void)
{
    scene_stream scene={0};rf_geometry_collision_world world={0};campaign_npc_body owner={0};
    rf_weapon_primary_definition definition={0};rf_physics_sphere sphere={0};
    combat_feedback feedback={0};float delta[3]={-3,0,0},point_delta[3]={-5,0,0},player_eye[3]={0},amount=0,player_amount=0;uint32_t i;
    rf_damage_effect_backend effects={campaign_damage_test_predicate,campaign_damage_test_uid,campaign_damage_test_source,
        campaign_damage_test_burn,campaign_damage_test_random,quiet_notify,campaign_damage_test_playing,campaign_damage_test_play,NULL};
    rf_collision_solid_view mover={0};rf_collision_face face={0};
    float vertices[4][3]={{1,-2,-2},{1,2,-2},{1,2,2},{1,-2,2}};
    scene.collision=&world;owner.eye_position[0]=3;owner.registration.handle=UINT32_MAX;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    campaign_player_view.flags_7c=8;
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_damage.state.effects.handle=campaign_player_object.handle;
    campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.class_health=100;
    for(i=0;i<11;i++)campaign_player_damage.factors[i]=1;
    scene_actor_body.allocated_bytes=1;scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;sphere.radius=.5f;
    for(i=0;i<3;i++){scene_actor_body.state.orientation[i*4]=1;scene_actor_body.state.bounds.minimum[i]=-.5f;scene_actor_body.state.bounds.maximum[i]=.5f;}
    definition.projectiles=4;definition.damage=10;definition.ai_damage_scale[0]=1;definition.damage_kind=2;
    /* Actual body intersection, cover query and player damage for every pellet. */
    CHECK(!campaign_enemy_shotgun_fire(&scene,&owner,NULL,UINT32_MAX,&definition,delta,player_eye,0,10,0,&effects,&feedback,&amount,&player_amount));
    CHECK(amount==40 && player_amount==40 && campaign_player_damage.state.effects.health==60);
    CHECK(rf_scene_enemy_spread[0]==4 && rf_scene_enemy_spread[2]==4);
    /* Ammunition is a caller contract: the helper cannot debit per pellet. */
    owner.inventory.loaded[3]=7;
    CHECK(!campaign_enemy_shotgun_fire(&scene,&owner,NULL,UINT32_MAX,&definition,point_delta,player_eye,1,10,0,&effects,&feedback,&amount,&player_amount));
    CHECK(owner.inventory.loaded[3]==7 && amount==40 && player_amount==40);
    /* Real finite mover face between muzzle and player blocks every pellet. */
    face.vertices=vertices;face.count=4;face.plane[0]=1;face.plane[3]=-1;
    face.minimum[0]=face.maximum[0]=1;face.minimum[1]=face.minimum[2]=-2;face.maximum[1]=face.maximum[2]=2;
    mover.flat_faces=&face;mover.flat_count=1;
    for(i=0;i<3;i++){mover.input_matrix[i][i]=mover.output_matrix[i][i]=1;mover.minimum[i]=-2;mover.maximum[i]=2;}
    campaign_movers.views=&mover;campaign_movers.count=1;
    CHECK(!campaign_enemy_shotgun_fire(&scene,&owner,NULL,UINT32_MAX,&definition,delta,player_eye,0,10,0,&effects,&feedback,&amount,&player_amount));
    CHECK(amount==0 && campaign_player_damage.state.effects.health==20 && rf_scene_enemy_spread[4]==4);
    campaign_movers.count=0;
    campaign_player_damage.state.effects.health=5;memset(rf_scene_enemy_spread,0,sizeof(rf_scene_enemy_spread));
    CHECK(!campaign_enemy_shotgun_fire(&scene,&owner,NULL,UINT32_MAX,&definition,delta,player_eye,0,10,0,&effects,&feedback,&amount,&player_amount));
    CHECK(amount==10 && campaign_player_damage.state.effects.health<=0 && rf_scene_enemy_spread[0]==1);
    puts("NPC shotgun pellets, finite cover, shell accounting and death cutoff pass");return 0;
}
