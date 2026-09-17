/* Compile the actual scene selector with its consumed field contract. This
 * exercises synthetic room transitions; it is not a live traversal replay. */
#include "rf/debris_visibility.h"
#include "rf/geomod.h"
#include <stdio.h>
#include <string.h>
typedef struct scene_debris_chunk {uint32_t active,room;float position[3],age;} scene_debris_chunk;
typedef struct scene_debris_pool {scene_debris_chunk chunks[80];} scene_debris_pool;
typedef struct scene_stream {
    scene_debris_pool *debris;const rf_geometry *geometry;rf_level_visibility visibility;
    rf_visibility_camera particle_camera;rf_level rocket_camera;
} scene_stream;
static uint32_t rf_scene_debris_visibility[8];
static uint32_t npc_hash_bytes(uint32_t h,const void *data,uint32_t n)
{const unsigned char *p=data;while(n--)h=(h^*p++)*16777619u;return h;}
#include "../src/diagnostic/scene_debris_visibility.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    unsigned char data[80]={0};uint32_t offsets[2]={0,40},rooms[2]={0,1},order[80],n;
    float bounds[6]={-1,-1,4,1,1,6},other[6]={2,-1,4,3,1,6};
    rf_geometry g={0};rf_room_visibility visible[2]={0};scene_debris_pool pool={0};scene_stream s={0};
    rf_geomod_debris_lifecycle life;uint32_t i;
    memcpy(data+4,bounds,24);memcpy(data+44,other,24);g.data=data;g.rooms=2;g.room_offsets=offsets;
    s.debris=&pool;s.geometry=&g;s.visibility.storage=data;s.visibility.state.rooms=visible;
    s.visibility.state.order=rooms;s.visibility.state.count=2;s.visibility.state.visible_count=2;
    s.particle_camera.view.perspective=1;s.particle_camera.view.scale[0]=s.particle_camera.view.scale[2]=1;
    s.particle_camera.view.scale[1]=4.f/3;
    s.particle_camera.view.basis[0]=s.particle_camera.view.basis[4]=s.particle_camera.view.basis[8]=1;
    s.rocket_camera.player_orientation[2][2]=1;
    CHECK(!rf_visibility_frustum_build(&s.particle_camera.view,&s.particle_camera.frustum));
    for(i=0;i<2;i++){visible[i].visible=1;visible[i].rectangle[2]=640;visible[i].rectangle[3]=480;}
    pool.chunks[0]=(scene_debris_chunk){1,0,{0,0,5},.5f};
    pool.chunks[1]=(scene_debris_chunk){1,1,{2,0,5},.75f};
    /* Position outside both bounds must still use retained room0. */
    pool.chunks[2]=(scene_debris_chunk){1,0,{100,0,8},1.f};
    CHECK(!scene_debris_visible_order(&s,order,&n) && n==3);
    CHECK(order[0]==1 && order[1]==2 && order[2]==0); /* reverse room order, depth within room */
    visible[1].rectangle[2]=320; /* right-side room clipped by left rectangle */
    CHECK(!scene_debris_visible_order(&s,order,&n) && n==2 && order[0]==2 && order[1]==0);
    CHECK(rf_scene_debris_visibility[3]==1 && rf_scene_debris_visibility[7]>0);
    for(i=0;i<n;i++){scene_debris_chunk *c=pool.chunks+order[i];CHECK(!rf_geomod_debris_age(c->age,2,1.f/60,0,&life));c->age=life.age;}
    CHECK(pool.chunks[1].age==.75f && pool.chunks[0].age>.5f);
    s.visibility.state.visible_count=0;
    CHECK(!scene_debris_visible_order(&s,order,&n) && n==0 && rf_scene_debris_visibility[3]==3);
    CHECK(pool.chunks[1].age==.75f);
    s.visibility.state.visible_count=2;visible[1].rectangle[2]=640;
    CHECK(!scene_debris_visible_order(&s,order,&n) && n==3 && order[0]==1);
    CHECK(!rf_geomod_debris_age(pool.chunks[1].age,2,1.f/60,0,&life) && life.age>.75f);
    puts("PASS actual scene selector: retained room, reverse order, rectangle rejection, hidden retention and return");return 0;
}
