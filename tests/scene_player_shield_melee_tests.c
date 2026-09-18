/* Same scene bash input/contact path, real registered NPC health arithmetic.
 * Only unrelated audio/pain/AI presentation callbacks are quiet fixtures. */
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"shield melee line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t published;
static void bash_quiet_notify(void *context,uint32_t kind,uint32_t target,float value,uint32_t source)
{(void)context;(void)kind;(void)target;(void)value;(void)source;}
static int bash_real_damage(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock_bits,const rf_damage_effect_backend *effects,float *amount)
{
    rf_damage_effect_backend quiet={campaign_damage_test_predicate,campaign_damage_test_uid,campaign_damage_test_source,
        campaign_damage_test_burn,campaign_damage_test_random,bash_quiet_notify,campaign_damage_test_playing,campaign_damage_test_play,NULL};
    (void)effects;
    if(request->amount!=10 || request->kind!=campaign_primary[11].damage_kind || request->source!=campaign_player_object.handle)return RF_FORMAT;
    ++published;return rf_scene_npc_damage(handle,request,difficulty,clock_bits,&quiet,amount);
}
#define rf_scene_npc_damage bash_real_damage
#define scene_player_shield_bash_state fixture_bash_state
#define scene_player_shield_bash_impact fixture_bash_impact
#define scene_player_shield_bash_input fixture_bash_input
#include "../src/diagnostic/scene_player_shield_melee.inc"
#undef scene_player_shield_bash_input
#undef scene_player_shield_bash_impact
#undef scene_player_shield_bash_state
#undef rf_scene_npc_damage
int main(int argc,char **argv)
{
    scene_stream scene={0};rf_geometry_collision_world world={0};static campaign_npc_body owner;
    rf_entity_seed seed={0};rf_entity_seed_class definition={0};rf_physics_sphere sphere={0};
    rf_collision_solid_view mover={0};rf_collision_face face={0};rf_vpp tables={0};
    float vertices[4][3]={{1,-2,-2},{1,2,-2},{1,2,2},{1,-2,2}};
    const float start[3]={2,0,0},forward[3]={-1,0,0};uint32_t i;
    CHECK(rf_vpp_open(&tables,argc>1?argv[1]:"Installed_Game/tables.vpp")==RF_OK);
    CHECK(rf_weapon_primary_load(&tables,"riot shield",128*1024,&campaign_primary[11])==RF_OK);rf_vpp_close(&tables);
    CHECK(campaign_primary[11].damage==10 && campaign_primary[11].ai_attack_range==2);
    scene.collision=&world;campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    campaign_seeds.items=&seed;campaign_seeds.classes=&definition;campaign_seeds.class_count=1;
    for(i=0;i<11;++i)definition.damage_factors[i]=1;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration)==RF_OK);
    owner.damage.effects.handle=owner.registration.handle;owner.damage.effects.health=owner.damage.effects.class_health=100;
    owner.view.weapons[0]=-1;owner.body.allocated_bytes=1;owner.body.spheres.items=&sphere;owner.body.spheres.count=1;sphere.radius=.5f;
    for(i=0;i<3;++i){owner.body.state.orientation[i*4]=1;owner.body.state.bounds.minimum[i]=-.5f;owner.body.state.bounds.maximum[i]=.5f;}
    campaign_player_object.handle=UINT32_MAX;campaign_player_view.handle=-1;
    campaign_shield_id=13;campaign_equipped_slot=11;campaign_player_inventory.owned[13]=1;
    campaign_player_inventory.loaded[13]=7;campaign_player_damage.state.effects.health=100;
    player_input.fire=1;
    CHECK(fixture_bash_input(&scene,0,start,forward)==RF_OK && !published && owner.damage.effects.health==100);
    player_input.fire=0; /* Release does not cancel an accepted swing. */
    for(i=1;i<12;++i)CHECK(fixture_bash_input(&scene,i,start,forward)==RF_OK && !published);
    CHECK(fixture_bash_input(&scene,12,start,forward)==RF_OK && published==1 && owner.damage.effects.health==90);
    CHECK(fixture_bash_input(&scene,13,start,forward)==RF_OK && published==1);
    CHECK(campaign_player_inventory.loaded[13]==7);
    /* A nearer physical mover blocks the next delayed impact. */
    face.vertices=vertices;face.count=4;face.plane[0]=1;face.plane[3]=-1;
    face.minimum[0]=face.maximum[0]=1;face.minimum[1]=face.minimum[2]=-2;face.maximum[1]=face.maximum[2]=2;
    mover.flat_faces=&face;mover.flat_count=1;
    for(i=0;i<3;++i){mover.input_matrix[i][i]=mover.output_matrix[i][i]=1;mover.minimum[i]=-2;mover.maximum[i]=2;}
    campaign_movers.views=&mover;campaign_movers.count=1;player_input.fire=1;
    CHECK(fixture_bash_input(&scene,30,start,forward)==RF_OK);player_input.fire=0;
    CHECK(fixture_bash_input(&scene,42,start,forward)==RF_OK && published==1 && owner.damage.effects.health==90);
    /* Out of authored reach: body lies entirely beyond the2m segment. */
    campaign_movers.count=0;owner.body.state.position[0]=-2;
    owner.body.state.bounds.minimum[0]=-2.5f;owner.body.state.bounds.maximum[0]=-1.5f;
    player_input.fire=1;CHECK(fixture_bash_input(&scene,60,start,forward)==RF_OK);player_input.fire=0;
    CHECK(fixture_bash_input(&scene,72,start,forward)==RF_OK && published==1 && owner.damage.effects.health==90);
    CHECK(campaign_player_inventory.loaded[13]==7);
    puts("player shield bash: delayed real10HP damage, release completion, cover, range and no ammo debit pass");return 0;
}
