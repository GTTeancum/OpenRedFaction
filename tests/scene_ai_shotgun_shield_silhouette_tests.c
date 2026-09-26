/* Actual shotgun/selector source, body spheres, finite mover cover and shield
 * damage arithmetic. Fixture substitutes skeletal resource/hand-pose ownership
 * with finite synthetic posed triangles; it does NOT validate installed NPC
 * attachment meshes/animations. No hardcoded triangle-hit or cover answers. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"shotgun shield line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_clutter_gameplay_definition fixture_definition;
static rf_clutter_damage_state fixture_durability;
static uint32_t fixture_commits;
static uint32_t fixture_triangle(const float start[3],const float end[3],float extent,float z,float limit,rf_collision_model_response_hit *hit)
{
    rf_collision_model_triangle triangle={0};float delta[3];uint32_t i;
    triangle.vertices[0][0]=-extent;triangle.vertices[0][1]=-extent;
    triangle.vertices[1][0]=extent;triangle.vertices[1][1]=-extent;triangle.vertices[2][1]=extent;
    for(i=0;i<3;i++){triangle.vertices[i][2]=z;delta[i]=end[i]-start[i];}
    triangle.plane[2]=1;triangle.plane[3]=-z;hit->time=limit;
    return rf_collision_model_ray_triangle(&triangle,start,delta,0x20,hit);
}
static int fixture_shield_query(uint32_t slot,const float *start,const float *end,float limit,scene_npc_shield_candidate *out,uint32_t *accepted)
{
    scene_npc_shield_candidate value={0};value.slot=slot;value.handle=42;value.limit=limit;
    *accepted=fixture_triangle(start,end,2,0,limit,&value.hit);if(*accepted)*out=value;return RF_OK;
}
static int fixture_body_before(const scene_npc_shield_candidate *candidate,const float *start,const float *end,uint32_t *accepted)
{
    rf_collision_model_response_hit body={0};
    *accepted=fixture_triangle(start,end,.2f,.1f,candidate->hit.time,&body);return RF_OK;
}
static int fixture_shield_commit(const scene_npc_shield_candidate *candidate,const float *start,const float *end,float damage,int32_t kind,uint32_t *accepted,uint32_t *broken)
{
    scene_riot_shield_result result;rf_collision_model_response_hit hit={0};float forward[3]={0,0,1},travel[3];uint32_t i,contact;int status;
    contact=fixture_triangle(start,end,2,0,candidate->limit,&hit);
    for(i=0;i<3;i++)travel[i]=end[i]-start[i];
    status=scene_riot_shield_intercept(&fixture_definition,&fixture_durability,1,1,1,contact,forward,travel,damage,kind,&result);if(status)return status;
    fixture_durability=result.state;*accepted=result.intercepted;*broken=result.broken;fixture_commits+=result.intercepted;return RF_OK;
}
/* Reinclude unchanged production adapters with only the physical-resource seams
 * redirected; the ordinary scene copy remains intact for all other services. */
#undef RF_SCENE_AI_SHIELD_RAY_INC
#define scene_npc_shield_query fixture_shield_query
#define scene_npc_shield_body_before fixture_body_before
#define scene_npc_shield_commit fixture_shield_commit
#define SCENE_AI_SHIELD_RAY_MISS FIXTURE_RAY_MISS
#define SCENE_AI_SHIELD_RAY_BODY FIXTURE_RAY_BODY
#define SCENE_AI_SHIELD_RAY_SHIELD FIXTURE_RAY_SHIELD
#define scene_ai_shield_ray_selection fixture_ray_selection
#define scene_ai_shield_ray_select fixture_ray_select
#define scene_ai_shield_ray_commit fixture_ray_commit
#include "../src/diagnostic/scene_ai_shield_ray.inc"
#define campaign_enemy_shotgun_fire fixture_shotgun_fire
#include "../src/diagnostic/scene_ai_shotgun.inc"
int main(void)
{
    scene_stream scene={0};rf_geometry_collision_world world={0};campaign_npc_body shooter={0},victim={0};
    rf_weapon_primary_definition definition={0};rf_physics_sphere sphere={0};scene_riot_shield_owner shield={0};
    combat_feedback feedback={0};rf_damage_effect_backend effects={0};float delta[3]={0,0,-3},player_eye[3]={0},amount,player_amount;uint32_t i;
    rf_collision_solid_view mover={0};rf_collision_face face={0};
    float vertices[4][3]={{-2,-2,1},{2,-2,1},{2,2,1},{-2,2,1}};
    scene.collision=&world;shooter.eye_position[0]=.75f;shooter.eye_position[2]=3;shooter.registration.handle=123;
    victim.damage.effects.health=100;victim.body.allocated_bytes=1;victim.body.spheres.items=&sphere;victim.body.spheres.count=1;sphere.radius=.2f;
    for(i=0;i<3;i++)victim.body.state.orientation[i*4]=1;
    definition.projectiles=4;definition.damage=10;definition.ai_damage_scale[0]=1;definition.damage_kind=2;
    fixture_definition.life=fixture_durability.health=100;fixture_definition.damage_factors[2]=1;
    scene_npc_shields.owners=&shield;scene_npc_shields.count=1;
    /* x.75 lies outside the radius.2 body, inside actual shield triangle. */
    CHECK(!fixture_shotgun_fire(&scene,&shooter,&victim,0,&definition,delta,player_eye,0,10,0,&effects,&feedback,&amount,&player_amount));
    CHECK(fixture_commits==4 && fixture_durability.health==60 && amount==0 && victim.damage.effects.health==100);
    face.vertices=vertices;face.count=4;face.plane[2]=1;face.plane[3]=-1;
    face.minimum[2]=face.maximum[2]=1;face.minimum[0]=face.minimum[1]=-2;face.maximum[0]=face.maximum[1]=2;
    mover.flat_faces=&face;mover.flat_count=1;
    for(i=0;i<3;i++){mover.input_matrix[i][i]=mover.output_matrix[i][i]=1;mover.minimum[i]=-2;mover.maximum[i]=2;}
    campaign_movers.views=&mover;campaign_movers.count=1;
    CHECK(!fixture_shotgun_fire(&scene,&shooter,&victim,0,&definition,delta,player_eye,0,10,0,&effects,&feedback,&amount,&player_amount));
    CHECK(fixture_commits==4 && fixture_durability.health==60 && amount==0);
    /* Same finite cover behind shield must not suppress the shield impact. */
    for(i=0;i<4;i++)vertices[i][2]=-1;face.plane[3]=1;face.minimum[2]=face.maximum[2]=-1;
    CHECK(!fixture_shotgun_fire(&scene,&shooter,&victim,0,&definition,delta,player_eye,0,10,0,&effects,&feedback,&amount,&player_amount));
    CHECK(fixture_commits==8 && fixture_durability.health==20 && amount==0 && victim.damage.effects.health==100);
    campaign_movers.count=0;scene_npc_shields.owners=NULL;
    puts("actual shotgun shield-only silhouette, front cover and behind-shield cover passed");return 0;
}
