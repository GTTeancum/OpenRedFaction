#include "rf/geomod.h"
#include "rf/geometry.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"placement line %d: %s\n",__LINE__,#c);return 1;}} while(0)
/* Reuses the real horizontal geometry fixture from weapon_blast_tests, but
 * tests direct internal world_ray flags; ordinary blast facade remaps CF5. */
int main(void)
{
    rf_collision_face face={0};float vertices[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
    rf_geometry_collision_room room={0};rf_collision_room_view view={0};rf_geometry_collision_world world={0};
    rf_geometry_world_hit hit;uint32_t primary=0,matched,i;float start[3]={0,1,0},delta[3]={0,-2,0};
    face.vertices=vertices;face.count=4;face.plane[1]=1;face.minimum[0]=face.minimum[2]=-2;face.maximum[0]=face.maximum[2]=2;
    CHECK(rf_collision_tree_open(&face,1,65536,&room.tree)==RF_OK);view.tree=&room.tree;
    memcpy(view.minimum,room.tree.nodes[0].minimum,12);memcpy(view.maximum,room.tree.nodes[0].maximum,12);
    world.rooms=&room;world.views=&view;world.room_count=world.primary_count=1;world.primary=&primary;
    for(i=0;i<4;i++) {
        rf_collision_face_filter *f=&room.tree.faces[0].filter;memset(f,0,sizeof(*f));
        if(i==1)f->face_flags=0x40;if(i==2)f->face_flags=0x80;
        if(i==3){f->owner_present=1;f->owner_kind=1;}
        CHECK(rf_geometry_collision_world_ray(&world,RF_GEOMOD_DEBRIS_QUERY_FLAGS,start,delta,1,&hit,&matched)==RF_OK && matched);
        CHECK(hit.hit.fraction==.5f && hit.hit.point[0]==0 && hit.hit.point[1]==0 && hit.hit.point[2]==0);
        CHECK(rf_geometry_collision_world_ray(&world,0x460,start,delta,1,&hit,&matched)==RF_OK && matched==(i==0));
    }
    room.tree.faces[0].filter.face_flags=4;
    CHECK(rf_geometry_collision_world_ray(&world,RF_GEOMOD_DEBRIS_QUERY_FLAGS,start,delta,1,&hit,&matched)==RF_OK && !matched);
    room.tree.faces[0].filter.face_flags=0;delta[0]=6;
    CHECK(rf_geometry_collision_world_ray(&world,RF_GEOMOD_DEBRIS_QUERY_FLAGS,start,delta,1,&hit,&matched)==RF_OK && !matched);
    rf_collision_tree_close(&room.tree);puts("PASS spawn CF5 face/owner policy and exact solid point");return 0;
}
