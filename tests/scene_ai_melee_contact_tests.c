#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_melee_contact.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI melee line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    scene_stream scene={0};rf_geometry_collision_world world={0};rf_physics_body target={0};
    rf_physics_sphere sphere={0};rf_collision_solid_view mover={0};rf_collision_face face={0};
    float start[3]={2,0,0},delta[3]={-2,0,0};uint32_t i,contact;
    float vertices[4][3]={{1,-2,-2},{1,2,-2},{1,2,2},{1,-2,2}};
    scene.collision=&world;target.allocated_bytes=1;target.spheres.items=&sphere;target.spheres.count=1;sphere.radius=.5f;
    for(i=0;i<3;i++){target.state.orientation[i*4]=1;target.state.bounds.minimum[i]=-.5f;target.state.bounds.maximum[i]=.5f;}
    CHECK(!campaign_enemy_melee_contact(&scene,start,delta,2.6f,&target,6,&contact) && contact);
    CHECK(!campaign_enemy_melee_contact(&scene,start,delta,1,&target,6,&contact) && !contact);
    delta[2]=3;
    CHECK(!campaign_enemy_melee_contact(&scene,start,delta,2.6f,&target,6,&contact) && !contact);delta[2]=0;
    face.vertices=vertices;face.count=4;face.plane[0]=1;face.plane[3]=-1;
    face.minimum[0]=face.maximum[0]=1;face.minimum[1]=face.minimum[2]=-2;face.maximum[1]=face.maximum[2]=2;
    mover.flat_faces=&face;mover.flat_count=1;
    for(i=0;i<3;i++){mover.input_matrix[i][i]=mover.output_matrix[i][i]=1;mover.minimum[i]=-2;mover.maximum[i]=2;}
    campaign_movers.views=&mover;campaign_movers.count=1;
    CHECK(!campaign_enemy_melee_contact(&scene,start,delta,2.6f,&target,6,&contact) && !contact);
    puts("NPC melee target contact, reach and cover admission pass");return 0;
}
