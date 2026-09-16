#include "rf/weapon.h"
#include "rf/geometry.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"blast line%d: %s\n",__LINE__,#x);return 1;}}while(0)
/* Recorded original489010 clear-cover damage words, not host formula values. */
static const uint32_t cases[][4]={{0x00000000u,0x00000000u,0x00000000u,0x43c80000u},{0x3f800000u,0x00000000u,0x00000000u,0x43a00000u},{0x40200000u,0x00000000u,0x00000000u,0x43480000u},{0x40800000u,0x00000000u,0x00000000u,0x42a00000u},{0x409fffffu,0x00000000u,0x00000000u,0x38200000u},{0x40a00000u,0x00000000u,0x00000000u,0x00000000u},{0x40a00001u,0x00000000u,0x00000000u,0x00000000u},{0x40e00000u,0x00000000u,0x00000000u,0x00000000u},{0x3f800000u,0x40000000u,0x40000000u,0x43200000u},{0xbf800000u,0xc0000000u,0xc0000000u,0x43200000u},{0x00000000u,0x00000000u,0x40800000u,0x42a00000u}};
static int cover_cases(void){
 rf_collision_face face={0};float vertices[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
 rf_geometry_collision_room room={0};rf_collision_room_view view={0};rf_geometry_collision_world world={0};
 rf_geometry_collision_movers movers={0};uint32_t primary=0,hit;float start[3]={0,1,0},end[3]={0,-1,0};
 face.vertices=vertices;face.count=4;face.plane[1]=1;face.minimum[0]=face.minimum[2]=-2;face.maximum[0]=face.maximum[2]=2;
 CHECK(!rf_collision_tree_open(&face,1,65536,&room.tree));view.tree=&room.tree;
 memcpy(view.minimum,room.tree.nodes[0].minimum,12);memcpy(view.maximum,room.tree.nodes[0].maximum,12);
 world.rooms=&room;world.views=&view;world.room_count=world.primary_count=1;world.primary=&primary;
 CHECK(!rf_geometry_collision_ray(&world,&movers,start,end,5,NULL,&hit)&&hit);
 end[0]=6;CHECK(!rf_geometry_collision_ray(&world,&movers,start,end,5,NULL,&hit)&&!hit);end[0]=0;
 room.tree.faces[0].filter.owner_present=1;room.tree.faces[0].filter.owner_kind=1;room.tree.faces[0].filter.owner_state=0;
 CHECK(!rf_geometry_collision_ray(&world,&movers,start,end,5,NULL,&hit)&&hit);
 CHECK(!rf_geometry_collision_ray(&world,&movers,start,end,0x27,NULL,&hit)&&!hit);
 room.tree.faces[0].filter.face_flags=0x80;
 CHECK(!rf_geometry_collision_ray(&world,&movers,start,end,5,NULL,&hit)&&!hit);
 rf_collision_tree_close(&room.tree);return 0;
}
int main(void){unsigned i;float origin[3]={0},p[3],out;uint32_t bits;
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  memcpy(p,cases[i],12);out=-1;CHECK(!rf_weapon_blast_amount(origin,p,400,5,&out));memcpy(&bits,&out,4);CHECK(bits==cases[i][3]);
 }
 p[0]=p[1]=p[2]=0;out=9;CHECK(!rf_weapon_blast_amount(origin,p,400,.1f,&out)&&out==0);
 CHECK(!rf_weapon_blast_amount(origin,p,400,nextafterf(.1f,1),&out)&&out==400);
 CHECK(!rf_weapon_blast_amount(origin,p,0,5,&out)&&out==0);
 out=9;p[0]=NAN;CHECK(rf_weapon_blast_amount(origin,p,400,5,&out)==RF_RANGE&&out==9);
 p[0]=FLT_MAX;origin[0]=-FLT_MAX;CHECK(rf_weapon_blast_amount(origin,p,400,5,&out)==RF_RANGE&&out==9);
 CHECK(!cover_cases());puts("PASS original ordinary blast vectors, real cover filtering and API boundaries");return 0;
}
