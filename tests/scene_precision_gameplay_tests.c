/* Real scene shot routing, registry/body geometry and mover cover. Damage
 * callback records publication; full NPC damage/pain resources are not mocked
 * as verified here and remain covered by their own gameplay integration. */
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"precision scene line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t precision_published[32],precision_count;
static int precision_record_damage(uint32_t handle,const rf_damage_request *request,
    float difficulty,uint32_t clock_bits,const rf_damage_effect_backend *effects,float *amount)
{
    (void)difficulty;(void)clock_bits;(void)effects;
    if(precision_count>=32 || request->amount!=25 || request->source!=campaign_player_object.handle)return RF_FORMAT;
    precision_published[precision_count++]=handle;*amount=0;return RF_OK;
}
/* Compile the same adapter against one explicit publication seam. */
#define scene_precision_contact fixture_precision_contact
#define scene_precision_insert fixture_precision_insert
#define scene_precision_fire fixture_precision_fire
#define rf_scene_npc_damage precision_record_damage
#include "../src/diagnostic/scene_precision_gameplay.inc"
#undef rf_scene_npc_damage
#undef scene_precision_fire
#undef scene_precision_insert
#undef scene_precision_contact
int main(void)
{
    scene_stream scene={0};rf_geometry_collision_world world={0};
    campaign_npc_body actors[3]={0};rf_physics_sphere spheres[3]={0};
    rf_collision_solid_view mover={0};rf_collision_face face={0};
    float vertices[4][3]={{1,-2,-2},{1,2,-2},{1,2,2},{1,-2,2}};
    float start[3]={3,0,0},forward[3]={-1,0,0};uint32_t i,k;
    scene.collision=&world;campaign_npc_bodies=actors;campaign_npc_body_count=3;
    rf_object_registry_init(&campaign_registry);campaign_player_object.handle=9999;
    campaign_pistol.damage=25;campaign_pistol.damage_kind=2;
    /* Enumeration deliberately far, near, middle. */
    for(i=0;i<3;i++){
        actors[i].body.allocated_bytes=1;actors[i].body.spheres.items=spheres+i;actors[i].body.spheres.count=1;
        spheres[i].radius=.25f;actors[i].body.state.position[0]=i==0?-4:i==1?0:-2;
        for(k=0;k<3;k++)actors[i].body.state.orientation[k*4]=1;
        actors[i].registration.view=&actors[i].view;actors[i].damage.effects.health=100;
        CHECK(!rf_object_registry_insert(&campaign_registry,&actors[i].registration,&actors[i].registration.handle));
    }
    CHECK(!fixture_precision_fire(&scene,1,start,forward,0));
    CHECK(precision_count==1 && precision_published[0]==actors[1].registration.handle);
    precision_count=0;
    face.vertices=vertices;face.count=4;face.plane[0]=1;face.plane[3]=-1;
    face.minimum[0]=face.maximum[0]=1;face.minimum[1]=face.minimum[2]=-2;face.maximum[1]=face.maximum[2]=2;
    mover.flat_faces=&face;mover.flat_count=1;
    for(k=0;k<3;k++){mover.input_matrix[k][k]=mover.output_matrix[k][k]=1;mover.minimum[k]=-2;mover.maximum[k]=2;}
    campaign_movers.views=&mover;campaign_movers.count=1;
    CHECK(!fixture_precision_fire(&scene,2,start,forward,0));CHECK(!precision_count);
    CHECK(!fixture_precision_fire(&scene,3,start,forward,1));
    CHECK(precision_count==3 && precision_published[0]==actors[1].registration.handle &&
        precision_published[1]==actors[2].registration.handle && precision_published[2]==actors[0].registration.handle);
    precision_count=0;actors[1].object_flags=2;
    CHECK(!rf_object_registry_remove(&campaign_registry,actors[2].registration.handle));
    CHECK(!fixture_precision_fire(&scene,4,start,forward,1));
    CHECK(precision_count==1 && precision_published[0]==actors[0].registration.handle);
    puts("precision scene nearest sniper, mover cover, rail piercing/order and stale/hidden filters passed");return 0;
}
