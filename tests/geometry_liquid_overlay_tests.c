#include "rf/geometry.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);return 1;}} while(0)

static void make_face(rf_collision_face *face,float vertices[4][3],float x,float y,uint32_t flags)
{
    static const float xz[4][2]={{-10,-10},{-10,10},{10,10},{10,-10}};
    uint32_t i;memset(face,0,sizeof(*face));
    face->plane[1]=1;face->plane[3]=-y;face->count=4;face->vertices=(const float(*)[3])vertices;
    face->minimum[0]=x-10;face->maximum[0]=x+10;
    face->minimum[1]=face->maximum[1]=y;face->minimum[2]=-10;face->maximum[2]=10;
    face->filter.face_flags=flags;
    for(i=0;i<4;i++){vertices[i][0]=x+xz[i][0];vertices[i][1]=y;vertices[i][2]=xz[i][1];}
}
static int query(const rf_geometry_collision_world *world,float x,float fraction,uint32_t face,uint32_t room)
{
    float start[3]={x,2,0},delta[3]={0,-4,0};rf_geometry_world_sweep_hit hit;
    uint32_t matched=0,liquid=0;
    CHECK(rf_geometry_collision_world_sweep_liquid(world,0x1000,start,delta,0,1,&hit,&matched,&liquid)==RF_OK);
    CHECK(matched && liquid && hit.room==room && hit.face==face && fabsf(hit.hit.fraction-fraction)<1e-6f);
    return 0;
}
int main(void)
{
    rf_geometry_collision_room rooms[2]={{0}};rf_collision_room_view views[2]={{0}};
    rf_collision_room_liquid_view liquids[2]={{0}};rf_geometry_collision_world base={0};
    rf_geometry_collision_overlay overlay={0};rf_collision_face originals[4],edited[2];
    float vertices[6][4][3];uint32_t primary[2]={0,1},ids[2],original_ids[2]={0,1};
    uint8_t markers[2]={1,1};rf_collision_tree active={0};uint32_t i,j,cycle;
    make_face(originals,vertices[0],0,-2,0);make_face(originals+1,vertices[1],0,0,4);
    make_face(originals+2,vertices[2],40,-2,0);make_face(originals+3,vertices[3],40,0,4);
    for(i=0;i<2;i++) {
        CHECK(rf_collision_tree_open(originals+i*2,2,65536,&rooms[i].tree)==RF_OK);
        rooms[i].room=i;views[i].tree=&rooms[i].tree;
        for(j=0;j<3;j++) {
            rooms[i].minimum[j]=views[i].minimum[j]=rooms[i].tree.nodes[0].minimum[j];
            rooms[i].maximum[j]=views[i].maximum[j]=rooms[i].tree.nodes[0].maximum[j];
        }
        for(j=0;j<2;j++)rooms[i].tree.source_indices[j]+=i*2;
        liquids[i].faces=rooms[i].tree.faces;liquids[i].face_count=2;liquids[i].contains_liquid=1;
    }
    base.rooms=rooms;base.views=views;base.liquids=liquids;base.contains_liquid=markers;
    base.room_count=base.primary_count=2;base.primary=primary;
    base.minimum[0]=base.minimum[2]=-10;base.minimum[1]=-2;
    base.maximum[0]=50;base.maximum[1]=2;base.maximum[2]=10;
    CHECK(rf_geometry_collision_overlay_open(&base,0,2,65536,&overlay)==RF_OK);
    CHECK(overlay.world.views[0].tree==&overlay.world.rooms[0].tree);
    CHECK(overlay.world.views[1].tree==&overlay.world.rooms[1].tree);
    CHECK(!query(&base,0,.5f,1,0) && !query(&overlay.world,0,.5f,1,0));
    for(cycle=0;cycle<8;cycle++) {
        rf_collision_tree next={0},invalid;uint32_t saved_indices[2],bad_ids[2];
        const float height=cycle%2?1.0f:.5f;
        make_face(edited,vertices[4],0,-2,0);make_face(edited+1,vertices[5],0,height,4);
        CHECK(rf_collision_tree_open(edited,2,65536,&next)==RF_OK);
        ids[0]=100+cycle*2;ids[1]=101+cycle*2;
        CHECK(rf_geometry_collision_overlay_bind(&overlay,&next,ids,2)==RF_OK);
        /* Free the previous published tree before querying the replacement. */
        rf_collision_tree_close(&active);active=next;
        CHECK(overlay.world.liquids[0].faces==active.faces && overlay.world.liquids[0].face_count==2);
        CHECK(overlay.world.liquids[0].contains_liquid==1);
        CHECK(overlay.world.rooms[0].tree.faces==active.faces);
        for(j=0;j<2;j++)CHECK(overlay.source_indices[j]==ids[active.source_indices[j]]);
        CHECK(overlay.world.liquids[1].faces==base.liquids[1].faces);
        CHECK(overlay.world.rooms[1].tree.faces==base.rooms[1].tree.faces);
        CHECK(!query(&overlay.world,0,(2-height)/4,ids[1],0));
        CHECK(!query(&overlay.world,40,.5f,3,1) && !query(&base,0,.5f,1,0));
        memcpy(saved_indices,overlay.source_indices,sizeof(saved_indices));
        memcpy(bad_ids,ids,sizeof(ids));bad_ids[0]=UINT32_MAX;
        CHECK(rf_geometry_collision_overlay_bind(&overlay,&active,bad_ids,2)==RF_FORMAT);
        CHECK(rf_geometry_collision_overlay_bind(&overlay,&active,ids,1)==RF_RANGE);
        invalid=active;invalid.faces=NULL;
        CHECK(rf_geometry_collision_overlay_bind(&overlay,&invalid,ids,2)==RF_RANGE);
        CHECK(!memcmp(saved_indices,overlay.source_indices,sizeof(saved_indices)));
        CHECK(overlay.world.liquids[0].faces==active.faces);
        CHECK(!query(&overlay.world,0,(2-height)/4,ids[1],0));
        /* Every other edit restores; intervening edits replace a still-live cut tree. */
        if(cycle&1u) {
        CHECK(rf_geometry_collision_overlay_bind(&overlay,&rooms[0].tree,original_ids,2)==RF_OK);
        rf_collision_tree_close(&active);
        CHECK(overlay.world.liquids[0].faces==rooms[0].tree.faces && overlay.world.liquids[0].face_count==2);
        CHECK(!query(&overlay.world,0,.5f,1,0) && !query(&overlay.world,40,.5f,3,1));
        }
    }
    rf_geometry_collision_overlay_close(&overlay);rf_geometry_collision_overlay_close(&overlay);
    CHECK(!query(&base,0,.5f,1,0));
    for(i=0;i<2;i++)rf_collision_tree_close(&rooms[i].tree);
    puts("PASS liquid overlay eight replacements/restores, source mapping, borrowed rooms and rejected-bind preservation");
    return 0;
}
