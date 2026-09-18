#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene_driller_actor_collision.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller actor collision line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_collision_body_sphere host_spheres[8]={0},actor_sphere={{0,0,0},.5f};
    scene_driller_actor_collision host={0};rf_collision_body_query q={0};rf_geometry_body_hit hit={0},saved;uint32_t i,found;
    host.spheres=host_spheres;host.count=8;host.handle=99;host.driver=UINT32_MAX;host.material=3;
    host.orientation[0]=host.orientation[4]=host.orientation[8]=1;memcpy(host.next_orientation,host.orientation,36);
    for(i=0;i<8;++i){host_spheres[i].center[0]=8+i;host_spheres[i].radius=1;}host_spheres[7].center[0]=4;
    q.spheres=&actor_sphere;q.count=1;q.radius=.5f;q.limit=1;q.end[0]=20;q.matrix[0][0]=q.matrix[1][1]=q.matrix[2][2]=1;
    CHECK(!scene_driller_actor_query(&host,17,UINT32_MAX,&q,&hit,&found) && found);
    CHECK(fabsf(hit.contact.fraction-.125f)<1e-6f && hit.contact.object_id==99 && hit.contact.material==3);
    CHECK(hit.contact.normal[0]==-1 && fabsf(hit.contact.point[0]-3)<1e-6f);
    /* Existing nearer wall wins; exact ties also retain its metadata. */
    hit.contact.fraction=.1f;hit.contact.object_id=123;saved=hit;found=1;
    CHECK(!scene_driller_actor_compose(&host,17,UINT32_MAX,&q,&hit,&found) && !memcmp(&hit,&saved,sizeof(hit)));
    hit.contact.fraction=.125f;saved=hit;
    CHECK(!scene_driller_actor_compose(&host,17,UINT32_MAX,&q,&hit,&found) && !memcmp(&hit,&saved,sizeof(hit)));
    CHECK(!scene_driller_actor_query(&host,17,99,&q,&hit,&found) && !found);
    host.driver=17;CHECK(!scene_driller_actor_query(&host,17,UINT32_MAX,&q,&hit,&found) && !found);host.driver=UINT32_MAX;
    /* Full local shape rotation: local+X becomes world+Z. */
    memset(host.orientation,0,36);host.orientation[2]=1;host.orientation[4]=1;host.orientation[6]=-1;
    memcpy(host.next_orientation,host.orientation,36);q.end[0]=0;q.end[2]=20;
    CHECK(!scene_driller_actor_query(&host,17,UINT32_MAX,&q,&hit,&found) && found && fabsf(hit.contact.fraction-.125f)<1e-6f);
    /* Stationary actor, translating host: relative sweep and contact velocity. */
    host.count=1;memset(host_spheres[0].center,0,12);host.position[0]=5;host.next_position[0]=0;host.velocity[0]=-5;
    memset(q.end,0,12);
    CHECK(!scene_driller_actor_query(&host,17,UINT32_MAX,&q,&hit,&found) && found);
    CHECK(fabsf(hit.contact.fraction-.7f)<1e-6f && hit.contact.velocity[0]==-5);
    /* Existing overlap may move away instead of trapping the player. */
    host.position[0]=host.next_position[0]=0;host.velocity[0]=0;q.start[0]=1;q.end[0]=3;
    CHECK(!scene_driller_actor_query(&host,17,UINT32_MAX,&q,&hit,&found) && !found);
    q.end[0]=0;CHECK(!scene_driller_actor_query(&host,17,UINT32_MAX,&q,&hit,&found) && found && hit.contact.fraction==0);
    puts("driller actor collision PASS: complete sphere union, rotation, moving host, ownership and nearest composition");return 0;
}
