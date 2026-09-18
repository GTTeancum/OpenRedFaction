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
static int batch_validation_test(void)
{
    rf_collision_face faces[17];float vertices[17][4][3];rf_collision_node nodes[17];
    rf_collision_tree trees[17];rf_collision_room_view rooms[17];uint32_t stack[17],primary[17],a,b,i,j;
    rf_collision_sweep_batch batch={0};rf_collision_sweep_room_hit expected,actual,saved;
    float start[3]={0,2,0},delta[3]={0,-4,0};
    memset(nodes,0,sizeof(nodes));memset(trees,0,sizeof(trees));memset(rooms,0,sizeof(rooms));
    for(i=0;i<17;i++) {
        face_make(faces+i,vertices[i],0,0);primary[i]=i;
        for(j=0;j<3;j++){nodes[i].minimum[j]=rooms[i].minimum[j]=-10;nodes[i].maximum[j]=rooms[i].maximum[j]=10;}
        nodes[i].face_count=1;nodes[i].left=nodes[i].right=UINT32_MAX;
        trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;
        trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=stack+i;rooms[i].tree=trees+i;
    }
    /* Repeated, different query inputs; equal-time ordering, miss, first hit,
     * and cache overflow must all match the fully checked route. */
    for(i=0;i<48;i++) {
        float radius=(i%3)*.1f,limit=(i%4)*.25f;uint32_t flags=i&1;
        memset(&expected,0,sizeof(expected));memset(&actual,0,sizeof(actual));a=b=99;
        CHECK(!rf_collision_sweep_rooms(rooms,17,primary,17,NULL,0,flags,start,delta,radius,limit,&expected,&a));
        CHECK(!rf_collision_sweep_rooms_batch(rooms,17,primary,17,NULL,0,flags,start,delta,radius,limit,&batch,&actual,&b));
        CHECK(a==b && !memcmp(&expected,&actual,sizeof(expected)));
    }
    CHECK(batch.tree_count==16);saved=actual;
    /* Mutable query inputs are never cached. */
    CHECK(rf_collision_sweep_rooms_batch(rooms,17,primary,17,NULL,0,0,start,delta,-1,1,&batch,&actual,&b)==RF_FORMAT);
    CHECK(!memcmp(&saved,&actual,sizeof(actual)));
    /* The uncached seventeenth tree remains checked on every encounter. */
    nodes[16].first_face=2;
    CHECK(rf_collision_sweep_rooms_batch(rooms,17,primary,17,NULL,0,0,start,delta,0,1,&batch,&actual,&b)==RF_RANGE);
    nodes[16].first_face=0;
    /* A new batch cannot retain validation from geometry before an edit. */
    memset(&batch,0,sizeof(batch));nodes[0].first_face=2;
    CHECK(rf_collision_sweep_rooms_batch(rooms,17,primary,17,NULL,0,0,start,delta,0,1,&batch,&actual,&b)==RF_RANGE);
    nodes[0].first_face=0;memset(&batch,0,sizeof(batch));
    rooms[16].minimum[0]=11;
    CHECK(rf_collision_sweep_rooms_batch(rooms,17,primary,17,NULL,0,0,start,delta,0,1,&batch,&actual,&b)==RF_FORMAT);
    rooms[16].minimum[0]=-10;memset(&batch,0,sizeof(batch));
    CHECK(!rf_collision_sweep_rooms_batch(rooms,17,primary,17,NULL,0,0,start,delta,0,1,&batch,&actual,&b));
    /* Changing list identity invalidates validation without caller reset. */
    {uint32_t invalid=17;
     CHECK(rf_collision_sweep_rooms_batch(rooms,17,&invalid,1,NULL,0,0,start,delta,0,1,&batch,&actual,&b)==RF_RANGE);}
    return 0;
}
static int block_pruning_test(void)
{
    rf_collision_face faces[64];float vertices[64][4][3];rf_collision_node node={0};
    rf_collision_tree tree={0};rf_collision_room_view room={0};rf_collision_sweep_batch batch={0};
    rf_collision_sweep_room_hit expected,actual;uint32_t stack,primary=0,i,j,a,b;
    float start[3]={0,2,0},delta[3]={0,-4,0};
    for(i=0;i<64;i++) {
        face_make(faces+i,vertices[i],0,0);
        if(i<32){faces[i].minimum[0]+=100;faces[i].maximum[0]+=100;for(j=0;j<4;j++)vertices[i][j][0]+=100;}
    }
    for(j=0;j<3;j++){node.minimum[j]=room.minimum[j]=-10;node.maximum[j]=room.maximum[j]=110;}
    node.face_count=64;node.left=node.right=UINT32_MAX;
    tree.nodes=&node;tree.node_count=tree.node_capacity=1;tree.faces=faces;tree.face_count=64;tree.stack=&stack;room.tree=&tree;
    for(i=0;i<32;i++) {
        float radius=(i%2)*.25f,limit=(i%4)*.25f;uint32_t flags=(i/4)&1;
        memset(&expected,0xa5,sizeof(expected));actual=expected;a=b=99;
        CHECK(!rf_collision_sweep_rooms(&room,1,&primary,1,NULL,0,flags,start,delta,radius,limit,&expected,&a));
        CHECK(!rf_collision_sweep_rooms_batch(&room,1,&primary,1,NULL,0,flags,start,delta,radius,limit,&batch,&actual,&b));
        CHECK(a==b && !memcmp(&expected,&actual,sizeof(actual)));
    }
    /* Distant malformed groups cannot hide the existing error. */
    memset(&batch,0,sizeof(batch));faces[0].filter.owner_present=2;
    CHECK(rf_collision_sweep_rooms_batch(&room,1,&primary,1,NULL,0,0,start,delta,0,1,&batch,&actual,&b)==RF_RANGE);
    faces[0].filter.owner_present=0;faces[0].minimum[0]=faces[0].maximum[0]+1;memset(&batch,0,sizeof(batch));
    CHECK(rf_collision_sweep_rooms_batch(&room,1,&primary,1,NULL,0,0,start,delta,0,1,&batch,&actual,&b)==RF_FORMAT);
    /* Preparation must not report a later error before an earlier first hit. */
    face_make(faces,vertices[0],0,0);faces[15].filter.owner_present=2;memset(&batch,0,sizeof(batch));
    memset(&expected,0,sizeof(expected));actual=expected;
    CHECK(!rf_collision_sweep_rooms(&room,1,&primary,1,NULL,0,1,start,delta,0,1,&expected,&a));
    CHECK(!rf_collision_sweep_rooms_batch(&room,1,&primary,1,NULL,0,1,start,delta,0,1,&batch,&actual,&b));
    CHECK(a==b && a && !memcmp(&expected,&actual,sizeof(actual)));
    return 0;
}
int main(void)
{
    CHECK(!batch_validation_test());
    CHECK(!block_pruning_test());
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
