/* Reuse the retained real solid/collision equivalence fixture. */
#define main retained_history_check_main
#include "geomod_history_check_tests.c"
#undef main
#include "../src/diagnostic/scene_terrain_edit_transaction.inc"
typedef struct edit_context {
    rf_geomod_mesh_view source;
    float center[3];int fail_create,fail_mutate,fail_publish,reset;
    uint32_t creates,mutations,publications,aborts,seen_serial;
    rf_geomod_terrain *published;uint32_t detached;
} edit_context;
static int edit_create(void *p,uint32_t budget,rf_geomod_terrain **out)
{
    edit_context *c=p;int status;c->creates++;
    if(c->fail_create)return RF_IO;
    status=rf_geomod_terrain_open(&c->source,filters,&generated,0,4096,800,budget,out);
    if(!status)status=rf_geomod_terrain_set_mapping(*out,64,64);return status;
}
static int edit_mutate(void *p,rf_geomod_terrain *t)
{
    edit_context *c=p;float extent[3]={2,2,2};int status;c->mutations++;
    if(c->detached){extent[0]=1;extent[1]=extent[2]=12;}
    status=c->reset?rf_geomod_terrain_reset(t):rf_geomod_terrain_cut_box(t,c->center,extent,3);
    return status?status:(c->fail_mutate?RF_FORMAT:RF_OK);
}
static int edit_publish(void *p,rf_geomod_terrain *t,uint32_t serial)
{
    edit_context *c=p;c->publications++;if(c->fail_publish)return RF_IO;
    c->published=t;c->seen_serial=serial;return RF_OK;
}
static void edit_abort(void *p){((edit_context *)p)->aborts++;}
static void save_live(rf_geomod_terrain *live,rf_geomod_terrain_view *before,uint32_t *bytes)
{
    CHECK(!rf_geomod_terrain_get(live,before));saved_tree=*before->tree;before->tree=&saved_tree;
    memcpy(saved_vertices,before->mesh.vertices,before->mesh.vertex_count*sizeof(*saved_vertices));
    memcpy(saved_faces,before->mesh.faces,before->mesh.face_count*sizeof(*saved_faces));
    CHECK(!rf_geomod_terrain_history_size(live,bytes));CHECK(!rf_geomod_terrain_history_encode(live,kept,*bytes));
}
static void detached_transaction(void)
{
    edit_context c={0};rf_geomod_piece_registry *pieces=NULL;rf_geomod_piece_batch *batch,*same;
    rf_geomod_owned_piece piece;rf_physics_body *body;rf_geomod_terrain *live=NULL,*old;
    scene_terrain_edit_limits limits={8388608,2097152,1179648};
    scene_terrain_edit_callbacks ops={edit_create,edit_mutate,edit_publish,edit_abort};
    scene_terrain_edit_result result;uint32_t serial=0,bytes;
    make_source(&c.source);c.detached=1;
    CHECK(!edit_create(&c,1179648,&live));
    CHECK(!rf_geomod_piece_registry_open(&generated,3,2.5f,.5f,.25f,0,2097152,&pieces));
    bytes=rf_geomod_piece_registry_bytes(pieces);old=live;c.fail_publish=1;
    CHECK(scene_terrain_edit_transaction_pieces(&live,&serial,&limits,&ops,&c,&result,pieces,0)==RF_IO);
    CHECK(live==old && !serial && !rf_geomod_piece_registry_count(pieces) && rf_geomod_piece_registry_bytes(pieces)==bytes);
    c.fail_publish=0;
    CHECK(!scene_terrain_edit_transaction_pieces(&live,&serial,&limits,&ops,&c,&result,pieces,0));
    CHECK(rf_geomod_piece_registry_count(pieces)==1);CHECK(!rf_geomod_piece_registry_get(pieces,0,&batch));
    CHECK(!rf_geomod_piece_batch_get(batch,0,&piece,&body));body->state.position[0]=123;
    c.detached=0;c.center[0]=5;
    CHECK(!scene_terrain_edit_transaction_pieces(&live,&serial,&limits,&ops,&c,&result,pieces,0));
    CHECK(rf_geomod_piece_registry_count(pieces)==1);CHECK(!rf_geomod_piece_registry_get(pieces,0,&same));
    CHECK(same==batch && body->state.position[0]==123);
    c.reset=1;
    CHECK(!scene_terrain_edit_transaction_pieces(&live,&serial,&limits,&ops,&c,&result,pieces,1));
    CHECK(!rf_geomod_piece_registry_count(pieces) && rf_geomod_piece_registry_bytes(pieces)==bytes);
    rf_geomod_piece_registry_close(&pieces);rf_geomod_terrain_close(&live);
    puts("PASS scene transaction stages bodies, rejects atomically, preserves moved body across clone/mutation and resets");
}
int main(void)
{
    detached_transaction();
    edit_context c={0};rf_geomod_terrain *live,*control,*original;
    rf_geomod_terrain_view before,a,b;uint32_t serial=17,bytes,i,creates;
    const float extent[3]={2,2,2};const float centers[3][3]={{-9,0,0},{9,0,0},{0,9,0}};
    scene_terrain_edit_limits limits={4*1024*1024,65536,1024*1024};
    scene_terrain_edit_callbacks ops={edit_create,edit_mutate,edit_publish,edit_abort};
    scene_terrain_edit_result result,sentinel;
    make_source(&c.source);live=open_terrain(&c.source,1024*1024,800);control=open_terrain(&c.source,1024*1024,800);
    CHECK(!rf_geomod_terrain_cut_box(live,centers[0],extent,3));CHECK(!rf_geomod_terrain_cut_box(control,centers[0],extent,3));
    memcpy(c.center,centers[1],12);original=live;save_live(live,&before,&bytes);
    CHECK(!rf_geomod_terrain_get(control,&unchanged_control));memset(&sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<3;i++){
        c.fail_create=i==0;c.fail_mutate=i==1;c.fail_publish=i==2;result=sentinel;
        CHECK(scene_terrain_edit_transaction(&live,&serial,&limits,&ops,&c,&result)!=RF_OK);
        CHECK(live==original && serial==17 && !memcmp(&result,&sentinel,sizeof(result)));
        CHECK(!c.published);preserved(live,&before,bytes);
    }
    CHECK(c.aborts==3);c.fail_create=c.fail_mutate=c.fail_publish=0;creates=c.creates;
    limits.total_budget=1;CHECK(scene_terrain_edit_transaction(&live,&serial,&limits,&ops,&c,&result)==RF_RANGE);
    CHECK(c.creates==creates && live==original && serial==17);preserved(live,&before,bytes);limits.total_budget=4*1024*1024;
    for(i=1;i<3;i++){
        memcpy(c.center,centers[i],12);original=live;
        CHECK(!scene_terrain_edit_transaction(&live,&serial,&limits,&ops,&c,&result));
        CHECK(live!=original && c.published==live && serial==17+i && c.seen_serial==serial && result.serial==serial);
        CHECK(result.reserved_peak_bytes<=limits.total_budget);
        CHECK(!rf_geomod_terrain_cut_box(control,centers[i],extent,3));
        CHECK(!rf_geomod_terrain_get(live,&a) && !rf_geomod_terrain_get(control,&b));compare(&a,&b);
    }
    c.reset=1;CHECK(!scene_terrain_edit_transaction(&live,&serial,&limits,&ops,&c,&result));
    CHECK(!rf_geomod_terrain_reset(control));CHECK(!rf_geomod_terrain_get(live,&a) && !rf_geomod_terrain_get(control,&b));compare(&a,&b);
    c.reset=0;memcpy(c.center,centers[0],12);CHECK(!scene_terrain_edit_transaction(&live,&serial,&limits,&ops,&c,&result));
    CHECK(!rf_geomod_terrain_cut_box(control,centers[0],extent,3));CHECK(!rf_geomod_terrain_get(live,&a) && !rf_geomod_terrain_get(control,&b));compare(&a,&b);
    original=live;serial=UINT32_MAX;CHECK(scene_terrain_edit_transaction(&live,&serial,&limits,&ops,&c,&result)==RF_RANGE);CHECK(live==original);
    rf_geomod_terrain_close(&control);rf_geomod_terrain_close(&live);
    puts("PASS clone transaction rejection identity/generation/history, reservation, repeated edits/reset and collision equivalence");return 0;
}
