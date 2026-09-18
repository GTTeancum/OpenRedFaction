#include "../src/diagnostic/scene_submarine_projectile_collision.inc"
#include <stdio.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);return 1;}} while(0)
static void face_make(rf_collision_face *face,float vertices[4][3],float y,uint32_t flags)
{
    static const float xz[4][2]={{-10,-10},{-10,10},{10,10},{10,-10}};
    uint32_t i;memset(face,0,sizeof(*face));face->plane[1]=1;face->plane[3]=-y;
    face->count=4;face->vertices=(const float(*)[3])vertices;face->filter.face_flags=flags;
    face->minimum[0]=face->minimum[2]=-10;face->maximum[0]=face->maximum[2]=10;
    face->minimum[1]=face->maximum[1]=y;
    for(i=0;i<4;i++){vertices[i][0]=xz[i][0];vertices[i][1]=y;vertices[i][2]=xz[i][1];}
}
static scene_submarine_weapon_round round_make(float y)
{
    scene_submarine_weapon_round round={0};round.flight.position[1]=y;
    round.flight.velocity[1]=-100;round.flight.radius=.15f;round.flight.remaining=10;round.flight.active=1;
    return round;
}
static int composed(void *context,const float start[3],const float delta[3],float radius,
    uint32_t flags,rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{
    uint32_t i;int status=*(int*)context;(void)radius;(void)flags;
    if(status)return status;
    memset(out,0,sizeof(*out));out->hit.fraction=.1f;out->hit.normal[1]=1;
    for(i=0;i<3;i++)out->hit.point[i]=start[i]+delta[i]*.1f;
    out->object=123;*liquid=0;*matched=1;return RF_OK;
}
int main(void)
{
    rf_geometry_collision_room room={0};rf_collision_room_view view={0};
    rf_collision_room_liquid_view liquid_view={0};rf_geometry_collision_world world={0};
    rf_collision_face faces[2];float vertices[2][4][3];uint32_t root=0,i;uint8_t marker=1;
    scene_submarine_projectile_query query={0};scene_submarine_weapon_round round,saved;
    rf_weapon_flight_liquid_state liquid={0x1004},saved_liquid;
    rf_weapon_flight_liquid_event event,sentinel;int status=0;
    face_make(faces,vertices[0],-2,0);face_make(faces+1,vertices[1],0,4);
    CHECK(!rf_collision_tree_open(faces,2,65536,&room.tree));view.tree=&room.tree;
    for(i=0;i<3;i++){
        room.minimum[i]=view.minimum[i]=room.tree.nodes[0].minimum[i];
        room.maximum[i]=view.maximum[i]=room.tree.nodes[0].maximum[i];
    }
    liquid_view.faces=room.tree.faces;liquid_view.face_count=2;liquid_view.contains_liquid=1;
    world.rooms=&room;world.views=&view;world.liquids=&liquid_view;world.contains_liquid=&marker;
    world.room_count=world.primary_count=1;world.primary=&root;
    world.minimum[0]=world.minimum[2]=-10;world.minimum[1]=-2;
    world.maximum[0]=world.maximum[2]=10;world.maximum[1]=2;query.world=&world;
    /* Real liquid surface is encountered before the solid floor: torpedo policy
     * expires without returning the solid-impact event used for blast/GeoMod. */
    round=round_make(2);
    CHECK(!scene_submarine_projectile_step(&round,.05f,&query,&liquid,0,-1,-1,&event));
    CHECK(event.has_liquid && event.terminal.kind==2 && !round.flight.active);
    CHECK(round.flight.remaining==0 && !(liquid.query_flags&0x1000));
    /* Starting below that surface still produces a real floor impact. */
    round=round_make(-1);liquid.query_flags=0x1004;
    CHECK(!scene_submarine_projectile_step(&round,.05f,&query,&liquid,0,-1,-1,&event));
    CHECK(!event.has_liquid && event.terminal.kind==1 && !round.flight.active);
    CHECK(event.terminal.contact.object==UINT32_MAX && event.terminal.contact.face==0);
    /* Live complete composition can select an actor before any boundary. */
    query.composed=composed;query.context=&status;round=round_make(2);liquid.query_flags=0x1004;
    CHECK(!scene_submarine_projectile_step(&round,.05f,&query,&liquid,0,-1,-1,&event));
    CHECK(!event.has_liquid && event.terminal.kind==1 && event.terminal.contact.object==123);
    round=round_make(2);saved=round;saved_liquid=liquid;status=RF_RANGE;
    memset(&sentinel,0xa5,sizeof(sentinel));event=sentinel;
    CHECK(scene_submarine_projectile_step(&round,.05f,&query,&liquid,0,-1,-1,&event)==RF_RANGE);
    CHECK(!memcmp(&round,&saved,sizeof(round)) && !memcmp(&liquid,&saved_liquid,sizeof(liquid)));
    CHECK(!memcmp(&event,&sentinel,sizeof(event)));
    rf_collision_tree_close(&room.tree);puts("submarine projectile solid/liquid composition passed");return 0;
}
