#include "rf/collision.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);return 1;}} while(0)
static void face_make(rf_collision_face *f,float v[4][3],float height,uint32_t flags)
{
    static const float xz[4][2]={{-10,-10},{-10,10},{10,10},{10,-10}};
    unsigned i;memset(f,0,sizeof(*f));f->plane[1]=1;f->plane[3]=-height;
    f->minimum[0]=f->minimum[2]=-10;f->maximum[0]=f->maximum[2]=10;
    f->minimum[1]=f->maximum[1]=height;f->count=4;f->vertices=(const float(*)[3])v;
    f->filter.face_flags=flags;
    for(i=0;i<4;i++){v[i][0]=xz[i][0];v[i][1]=height;v[i][2]=xz[i][1];}
}
static int fail_sample(void *context,uint32_t index,const rf_collision_face *face,
    int32_t bitmap,const float point[3],uint32_t *color)
{(void)context;(void)index;(void)face;(void)bitmap;(void)point;(void)color;return RF_IO;}
int main(void)
{
    rf_collision_face faces[3],detail;float vertices[4][4][3];
    rf_collision_node nodes[2];rf_collision_tree trees[2];rf_collision_room_view rooms[2];
    rf_collision_room_liquid_view water[2];rf_collision_sweep_liquid_room_hit result,saved;
    rf_collision_sweep_room_hit dry;uint32_t stack[2],primary=0,child=1,matched;
    float start[3]={0,2,0},delta[3]={0,-4,0};unsigned i,j;
    int32_t bitmaps[3]={-1,1,1};rf_collision_indexed_texture_backend texture={bitmaps,fail_sample,NULL};
    memset(nodes,0,sizeof(nodes));memset(trees,0,sizeof(trees));memset(rooms,0,sizeof(rooms));memset(water,0,sizeof(water));
    face_make(faces,vertices[0],-1,0);face_make(faces+1,vertices[1],0,4);face_make(faces+2,vertices[2],1,4);
    face_make(&detail,vertices[3],-.5f,0);
    for(i=0;i<2;i++) {
        for(j=0;j<3;j++){nodes[i].minimum[j]=rooms[i].minimum[j]=-10;nodes[i].maximum[j]=rooms[i].maximum[j]=10;}
        nodes[i].face_count=i?1:3;nodes[i].left=nodes[i].right=UINT32_MAX;
        trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;
        trees[i].faces=i?&detail:faces;trees[i].face_count=i?1:3;trees[i].stack=stack+i;
        rooms[i].tree=trees+i;
    }
    rooms[0].child_count=1;water[0].faces=faces;water[0].face_count=3;water[0].contains_liquid=1;
#define RUN(flags) rf_collision_sweep_rooms_liquid(rooms,2,&primary,1,&child,1,flags,start,delta,0,1,water,NULL,&result,&matched)
    CHECK(RUN(0x1000)==RF_OK && matched && result.is_liquid && result.room.room==0 && result.room.tree.face_index==2 && result.room.tree.hit.fraction==.25f && result.room.tree.hits==4);
    CHECK(RUN(0x1001)==RF_OK && matched && !result.is_liquid && result.room.tree.hit.fraction==.75f && result.room.tree.hits==1);
    rooms[0].skip=1;
    CHECK(RUN(0x1000)==RF_OK && matched && result.is_liquid && result.room.tree.hits==2);
    CHECK(RUN(0x1001)==RF_OK && matched && result.room.tree.face_index==1 && result.room.tree.hit.fraction==.5f);
    CHECK(RUN(0x1008)==RF_OK && matched && result.room.tree.hits==4);
    rooms[0].skip=0;water[0].contains_liquid=0;
    CHECK(RUN(0x1000)==RF_OK && matched && !result.is_liquid && result.room.room==1 && result.room.tree.hit.fraction==.625f);
    water[0].contains_liquid=1;
    CHECK(RUN(0)==RF_OK && matched && !result.is_liquid && result.room.room==1);
    CHECK(rf_collision_sweep_rooms(rooms,2,&primary,1,&child,1,0,start,delta,0,1,&dry,&matched)==RF_OK);
    CHECK(memcmp(&dry,&result.room,sizeof(dry))==0);
    CHECK(rf_collision_sweep_rooms(rooms,2,&primary,1,&child,1,0x1000,start,delta,0,1,&dry,&matched)==RF_NOT_FOUND);
    for(i=0;i<3;i++)face_make(faces+i,vertices[i],0,i?4:0);
    face_make(&detail,vertices[3],0,0);
    CHECK(RUN(0x1000)==RF_OK && matched && result.is_liquid && result.room.tree.face_index==2 && result.room.tree.hits==4);
    /* A later liquid failure must not publish the earlier solid result. */
    memset(&result,0x5a,sizeof(result));saved=result;matched=99;
    water[0].faces=NULL;
    CHECK(RUN(0x1000)==RF_RANGE && matched==99 && !memcmp(&result,&saved,sizeof(result)));
    water[0].faces=faces;
    /* Explicit texture failure after solid traversal, with optional solid tables. */
    {
        rf_collision_indexed_texture_backend solid_textures[2]={{bitmaps,fail_sample,NULL},{bitmaps,fail_sample,NULL}};
        faces[1].filter.face_flags|=0x40;water[0].textures=&texture;
        CHECK(rf_collision_sweep_rooms_liquid(rooms,2,&primary,1,&child,1,0x1080,start,delta,0,1,water,solid_textures,&result,&matched)==RF_IO);
        CHECK(matched==99 && !memcmp(&result,&saved,sizeof(result)));
        faces[1].filter.face_flags=4;water[0].textures=NULL;
    }
    start[0]=30;CHECK(RUN(0x1000)==RF_OK && !matched && !memcmp(&result,&saved,sizeof(result)));
    start[0]=0;start[1]=-2;delta[1]=4;CHECK(RUN(0x1000)==RF_OK && !matched);
    start[1]=2;delta[1]=0;CHECK(RUN(0x1000)==RF_OK && !matched);
    delta[1]=-2;CHECK(RUN(0x1000)==RF_OK && matched && result.is_liquid && result.room.tree.hit.fraction==1);
    puts("PASS liquid room ordering, filters, sky routing, duplicates, errors and endpoints");return 0;
}
