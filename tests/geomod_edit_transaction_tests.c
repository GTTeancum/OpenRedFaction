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
typedef struct group_context {
    rf_geomod_terrain **live;rf_geomod_terrain *old[2];uint32_t calls,aborts,fail,reset;
} group_context;
static int group_publish(void *opaque,rf_geomod_terrain *const *candidates,uint32_t count,uint32_t serial) {
    group_context *g=opaque;uint32_t i;rf_geomod_terrain_view view;
    CHECK(count==2 && serial>0);g->calls++;
    for(i=0;i<2;i++) {
        CHECK(g->live[i]==g->old[i] && candidates[i]!=g->old[i]);
        CHECK(!rf_geomod_terrain_get(candidates[i],&view));CHECK(g->reset?!view.cuts:view.cuts>0);
    }
    return g->fail?RF_IO:RF_OK;
}
static void group_abort(void *opaque){((group_context *)opaque)->aborts++;}
static void grouped_transaction(void) {
    edit_context c[2]={{0}};rf_geomod_terrain *live[2]={0};rf_geomod_piece_registry *pieces[2]={0};
    rf_geomod_piece_batch *batches[2];rf_geomod_owned_piece piece;rf_physics_body *bodies[2];
    scene_terrain_edit_callbacks source_ops={edit_create,edit_mutate,edit_publish,edit_abort};
    scene_terrain_edit_group_callbacks ops={group_publish,group_abort};
    scene_terrain_edit_source sources[2];scene_terrain_edit_result result[2],sentinel[2];
    scene_terrain_edit_limits limits={16*1024*1024,4*1024*1024,1179648};
    group_context group={0};uint32_t serial=0,bytes[2],i,case_id;
    group.live=live;memset(sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<2;i++) {
        make_source(&c[i].source);c[i].detached=1;
        CHECK(!edit_create(c+i,limits.clone_budget,live+i));group.old[i]=live[i];
        CHECK(!rf_geomod_piece_registry_open(&generated,3,2.5f,.5f,.25f,i,2097152,pieces+i));
        bytes[i]=rf_geomod_piece_registry_bytes(pieces[i]);
        sources[i]=(scene_terrain_edit_source){live+i,&source_ops,c+i,pieces[i],0};
    }
    for(case_id=0;case_id<3;case_id++) {
        c[1].fail_create=case_id==0;c[1].fail_mutate=case_id==1;group.fail=case_id==2;
        memcpy(result,sentinel,sizeof(result));
        CHECK(scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result)!=RF_OK);
        CHECK(!serial && !memcmp(result,sentinel,sizeof(result)));
        for(i=0;i<2;i++) {
            rf_geomod_terrain_view view;CHECK(live[i]==group.old[i]);
            CHECK(!rf_geomod_terrain_get(live[i],&view) && !view.cuts);
            CHECK(!rf_geomod_piece_registry_count(pieces[i]) && rf_geomod_piece_registry_bytes(pieces[i])==bytes[i]);
        }
    }
    CHECK(group.calls==1 && group.aborts==3);
    c[1].fail_create=c[1].fail_mutate=group.fail=0;
    sources[1].pieces=pieces[0];
    CHECK(scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result)==RF_RANGE);
    sources[1].pieces=pieces[1];sources[1].live=live;
    CHECK(scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result)==RF_RANGE);
    sources[1].live=live+1;limits.total_budget=1;
    CHECK(scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result)==RF_RANGE);
    limits.total_budget=16*1024*1024;
    CHECK(!scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result));
    CHECK(serial==1 && group.calls==2);
    for(i=0;i<2;i++) {
        CHECK(result[i].serial==1 && result[i].reserved_peak_bytes<=limits.total_budget);
        CHECK(live[i]!=group.old[i]);group.old[i]=live[i];
        CHECK(rf_geomod_piece_registry_count(pieces[i])==1);
        CHECK(!rf_geomod_piece_registry_get(pieces[i],0,batches+i));
        CHECK(!rf_geomod_piece_batch_get(batches[i],0,&piece,bodies+i));
        bodies[i]->state.position[0]=123.f+i;
        c[i].detached=0;c[i].center[0]=5;
    }
    CHECK(!scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result));
    CHECK(serial==2 && group.calls==3);
    for(i=0;i<2;i++) {
        rf_geomod_piece_batch *same;
        CHECK(!rf_geomod_piece_registry_get(pieces[i],0,&same));
        CHECK(same==batches[i] && bodies[i]->state.position[0]==123.f+i);
        group.old[i]=live[i];c[i].reset=1;
    }
    sources[0].fresh=1;
    CHECK(scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result)==RF_RANGE);
    group.reset=1;
    for(i=0;i<2;i++)sources[i].fresh=sources[i].replace_pieces=1;
    for(case_id=0;case_id<3;case_id++) {
        c[1].fail_create=case_id==0;c[1].fail_mutate=case_id==1;group.fail=case_id==2;
        memcpy(result,sentinel,sizeof(result));
        CHECK(scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result)!=RF_OK);
        CHECK(serial==2 && !memcmp(result,sentinel,sizeof(result)));
        for(i=0;i<2;i++) {
            rf_geomod_piece_batch *same;rf_geomod_terrain_view view;
            CHECK(live[i]==group.old[i]);CHECK(!rf_geomod_terrain_get(live[i],&view) && view.cuts==2);
            CHECK(!rf_geomod_piece_registry_get(pieces[i],0,&same) && same==batches[i]);
            CHECK(bodies[i]->state.position[0]==123.f+i);
        }
    }
    c[1].fail_create=c[1].fail_mutate=group.fail=0;
    CHECK(!scene_terrain_edit_transaction_group(sources,2,&serial,&limits,&ops,&group,result));CHECK(serial==3);
    for(i=0;i<2;i++) {
        rf_geomod_terrain_view view;CHECK(!rf_geomod_terrain_get(live[i],&view) && !view.cuts);
        CHECK(!result[i].history_bytes && !rf_geomod_piece_registry_count(pieces[i]));
        CHECK(rf_geomod_piece_registry_bytes(pieces[i])==bytes[i]);
        rf_geomod_piece_registry_close(pieces+i);rf_geomod_terrain_close(live+i);
    }
    puts("PASS fresh grouped reset: failure retains both histories/moved bodies; success clears both without history replay");
    puts("PASS grouped transaction: second-source and publication rejection roll back both cores/registries; one room commit; independent moved bodies survive replay");
}
int main(void)
{
    detached_transaction();
    grouped_transaction();
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
