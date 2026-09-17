#include "rf/checkpoint_placement.h"
#include "rf/geomod_notify.h"
#include "rf/geomod_piece_bank.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct staging {rf_geomod_piece_batch *batch[8];rf_random_state random;uint32_t count,reject;} staging;
static void clear(staging *s)
{for(uint32_t i=0;i<8;i++)rf_geomod_piece_batch_close(s->batch+i);memset(s,0,sizeof(*s));}
static int emit(const rf_geomod_mesh_view *mesh,const uint32_t *map,const rf_collision_face_filter *filters,
    uint32_t source_count,uint32_t prefix,uint32_t ordinal,void *opaque)
{
    staging *s=opaque;rf_collision_face_filter mapped[32],generated={0};int status;
    (void)ordinal;if(s->reject && prefix==s->reject)return RF_IO;
    if(mesh->face_count>32 || s->count==8)return RF_RANGE;
    for(uint32_t i=0;i<mesh->face_count;i++){if(map[i]>=source_count)return RF_FORMAT;mapped[i]=filters[map[i]];}
    status=rf_geomod_piece_batch_open(mesh,mapped,&generated,7,2.5f,.5f,.25f,&s->random,2097152,s->batch+s->count);
    if(!status)s->count++;return status;
}
static double volume(const rf_geomod_mesh_view *m)
{
    double sum=0;
    for(uint32_t f=0;f<m->face_count;f++) {
        const rf_geomod_face *face=m->faces+f;const float *a=m->vertices[face->first].position;
        for(uint32_t i=1;i+1<face->count;i++) {
            const float *b=m->vertices[face->first+i].position,*c=m->vertices[face->first+i+1].position;
            sum+=((double)a[0]*(b[1]*c[2]-b[2]*c[1])+(double)a[1]*(b[2]*c[0]-b[0]*c[2])+(double)a[2]*(b[0]*c[1]-b[1]*c[0]))/6;
        }
    }
    return sum;
}
static int cut(rf_geomod_terrain *t,const float center[3],const float extent[3],uint32_t star)
{
    rf_geomod_vertex v[36]={0};rf_geomod_face f[12];rf_geomod_mesh_view mesh={v,f,36,12,0};
    const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};const uint32_t corners[2][3]={{0,1,2},{0,2,3}};
    if(!star)return rf_geomod_terrain_cut_box(t,center,extent,7);
    for(uint32_t axis=0;axis<3;axis++)for(uint32_t side=0;side<2;side++)for(uint32_t tri=0;tri<2;tri++) {
        uint32_t face=(axis*2+side)*2+tri,a=(axis+1)%3,b=(axis+2)%3;
        f[face]=(rf_geomod_face){face*3,3,7,UINT32_MAX};
        for(uint32_t j=0;j<3;j++) {
            uint32_t k=corners[tri][j];if(!side)k=3-k;
            v[face*3+j].position[axis]=center[axis]+(side?extent[axis]:-extent[axis]);
            v[face*3+j].position[a]=center[a]+u[k]*extent[a];
            v[face*3+j].position[b]=center[b]+w[k]*extent[b];
        }
    }
    return rf_geomod_terrain_cut_star(t,&mesh,center);
}
static int reject_candidate(const rf_geomod_terrain_view *view,void *context)
{(void)view;(void)context;return RF_IO;}
static void registry_lifetime(const rf_geomod_mesh_view *mesh,const rf_collision_face_filter *filters,
    const rf_collision_face_filter *generated)
{
    rf_geomod_terrain *t=NULL;rf_geomod_piece_registry *r=NULL;rf_geomod_piece_batch *first,*again;
    rf_geomod_owned_piece piece;rf_physics_body *body;float moved;uint32_t bytes;
    rf_geomod_changed_box initial_boxes[32],boxes[32];uint32_t box_count;
    const float center[4][3]={{0,0,0},{5,0,0},{-1,0,0},{-6,0,0}},extent[4][3]={{1,12,12},{2,2,2},{2,2,2},{1,12,12}};
    CHECK(!rf_geomod_terrain_open(mesh,filters,generated,0,4096,800,1179648,&t));
    CHECK(!rf_geomod_piece_registry_open(generated,7,2.5f,.5f,.25f,0,2097152,&r));
    CHECK(!rf_geomod_terrain_set_extraction(t,rf_geomod_piece_registry_emit,r));
    CHECK(!rf_geomod_piece_registry_begin(r,0));
    CHECK(!rf_geomod_terrain_cut_box(t,center[0],extent[0],7));
    CHECK(!rf_geomod_piece_registry_count(r));rf_geomod_piece_registry_commit(r);
    CHECK(rf_geomod_piece_registry_count(r)==1);CHECK(!rf_geomod_piece_registry_get(r,0,&first));
    CHECK(!rf_geomod_piece_registry_changed_boxes(r,0,initial_boxes,&box_count) && box_count==1);
    CHECK(!rf_geomod_piece_registry_changed_boxes(r,1,boxes,&box_count) && !box_count);
    box_count=999;CHECK(rf_geomod_piece_registry_changed_boxes(r,2,boxes,&box_count)==RF_RANGE && box_count==999);
    CHECK(!rf_geomod_piece_batch_get(first,0,&piece,&body));body->state.position[0]+=17;moved=body->state.position[0];
    for(uint32_t i=1;i<3;i++) {
        CHECK(!rf_geomod_piece_registry_begin(r,0));
        CHECK(!rf_geomod_terrain_cut_box(t,center[i],extent[i],7));
        rf_geomod_piece_registry_commit(r);
        CHECK(rf_geomod_piece_registry_count(r)==1);CHECK(!rf_geomod_piece_registry_get(r,0,&again));
        CHECK(first==again && body->state.position[0]==moved);
        CHECK(!rf_geomod_piece_registry_changed_boxes(r,0,boxes,&box_count) && box_count==1);
        CHECK(!memcmp(boxes,initial_boxes,sizeof(*boxes)));
    }
    {
        rf_collision_body_query query={0};rf_collision_body_sphere spheres[2]={0};
        rf_geomod_registry_body_hit hit,before;const rf_collision_face *face=piece.collision;
        float center[3]={0};uint32_t k,n,matched;
        for(n=0;n<face->count;n++)for(k=0;k<3;k++)center[k]+=face->vertices[n][k]/face->count;
        for(k=0;k<3;k++) {
            query.matrix[k][k]=1;query.start[k]=body->state.position[k]+center[k]+2*face->plane[k];
            query.end[k]=query.start[k]-4*face->plane[k];spheres[1].center[k]=-face->plane[k];
        }
        spheres[0].radius=spheres[1].radius=.1f;query.radius=2;query.spheres=spheres;query.count=2;query.limit=1;
        body->state.velocity[2]=3;
        CHECK(!rf_geomod_piece_registry_body_sweep(r,&query,1,&hit,&matched));
        CHECK(matched && hit.batch==0 && hit.piece==0 && hit.face==0 && hit.sphere==1);
        CHECK(fabsf(hit.contact.fraction-.225f)<.0001f && hit.contact.material==1 && hit.contact.velocity[2]==3);
        CHECK(hit.contact.object_id==UINT32_MAX && hit.contact.face_token==UINT32_MAX);
        before=hit;
        CHECK(!rf_geomod_piece_registry_body_sweep_excluding(r,0,0,&query,1,&hit,&matched));
        CHECK(!matched && !memcmp(&hit,&before,sizeof(hit)));
        matched=999;
        CHECK(rf_geomod_piece_registry_body_sweep_excluding(r,0,UINT32_MAX,&query,1,&hit,&matched)==RF_RANGE);
        CHECK(matched==999 && !memcmp(&hit,&before,sizeof(hit)));
        CHECK(!rf_geomod_piece_registry_body_sweep_excluding(r,UINT32_MAX,UINT32_MAX,&query,1,&hit,&matched));
        CHECK(matched && !memcmp(&hit,&before,sizeof(hit)));
        memset(query.matrix,0,sizeof(query.matrix));query.matrix[0][1]=1;query.matrix[1][0]=-1;query.matrix[2][2]=1;
        spheres[1].center[0]=-face->plane[1];spheres[1].center[1]=face->plane[0];spheres[1].center[2]=-face->plane[2];
        CHECK(!rf_geomod_piece_registry_body_sweep(r,&query,1,&hit,&matched));
        CHECK(matched && !memcmp(&hit,&before,sizeof(hit)));
        CHECK(!rf_geomod_piece_registry_body_sweep(NULL,&query,1,&hit,&matched));
        CHECK(!matched && !memcmp(&hit,&before,sizeof(hit)));

        before=hit;matched=99;query.limit=.1f;
        CHECK(!rf_geomod_piece_registry_body_sweep(r,&query,1,&hit,&matched));
        CHECK(!matched && !memcmp(&hit,&before,sizeof(hit)));
        query.limit=1;query.matrix[0][0]=NAN;matched=99;
        CHECK(rf_geomod_piece_registry_body_sweep(r,&query,1,&hit,&matched)!=RF_OK);
        CHECK(matched==99 && !memcmp(&hit,&before,sizeof(hit)));body->state.velocity[2]=0;
    }
    {
        const rf_collision_face *face=piece.collision;float center[3]={0},test_center[3];
        rf_physics_sphere sphere={0};rf_checkpoint_placement placement={0};uint32_t n,k;
        for(n=0;n<face->count;n++)for(k=0;k<3;k++)center[k]+=face->vertices[n][k]/face->count;
        for(n=0;n<4;n++) {
            const float distance[]={1,.1f,.04f,-.3f};int expected=n<2?RF_OK:RF_NOT_FOUND;
            for(k=0;k<3;k++)test_center[k]=center[k]+distance[n]*face->plane[k];
            CHECK(rf_checkpoint_solid_sphere_check(piece.collision,piece.mesh.face_count,0,test_center,.1f,NULL)==expected);
        }
        sphere.radius=.1f;placement.spheres=&sphere;placement.count=1;
        placement.basis[0]=placement.basis[4]=placement.basis[8]=1;
        for(k=0;k<3;k++)placement.position[k]=body->state.position[k]+center[k]-.3f*face->plane[k];
        CHECK(rf_geomod_piece_registry_placement_check(r,&placement)==RF_NOT_FOUND);
        for(k=0;k<3;k++)placement.position[k]=body->state.position[k]+center[k]+face->plane[k];
        CHECK(!rf_geomod_piece_registry_placement_check(r,&placement));
        /* Tilt the solid and rotate an offset player sphere independently.
         * The same local clear/overlap cases must survive both transforms. */
        {
            float saved_basis[9];uint32_t j;
            const float tilted[9]={.8f,.6f,0,-.6f,.8f,0,0,0,1};
            memcpy(saved_basis,body->state.orientation,sizeof(saved_basis));
            memcpy(body->state.orientation,tilted,sizeof(tilted));
            memset(placement.basis,0,sizeof(placement.basis));
            placement.basis[1]=1;placement.basis[3]=-1;placement.basis[8]=1;
            sphere.center[0]=.25f;sphere.center[1]=-.5f;sphere.center[2]=.125f;
            for(n=0;n<4;n++) {
                const float distance[]={1,.1f,.04f,-.3f};
                for(k=0;k<3;k++)test_center[k]=center[k]+distance[n]*face->plane[k];
                for(k=0;k<3;k++) {
                    placement.position[k]=body->state.position[k];
                    for(j=0;j<3;j++)placement.position[k]+=test_center[j]*tilted[j*3+k]-sphere.center[j]*placement.basis[j*3+k];
                }
                CHECK(rf_geomod_piece_registry_placement_check(r,&placement)==(n<2?RF_OK:RF_NOT_FOUND));
            }
            memcpy(body->state.orientation,saved_basis,sizeof(saved_basis));
        }
        {rf_checkpoint_placement_result value={123,456,789},saved=value;
         test_center[0]=NAN;
         CHECK(rf_checkpoint_solid_sphere_check(piece.collision,piece.mesh.face_count,0,test_center,.1f,&value)!=RF_OK);
         CHECK(!memcmp(&value,&saved,sizeof(value)));}
    }
    {
        rf_physics_ground_probe probe={0};rf_checkpoint_support_hit hit,kept;uint32_t matched,k,n;
        rf_physics_body_state saved=body->state;const rf_collision_face *face=piece.collision;float center[3]={0};
        for(n=0;n<face->count;n++)for(k=0;k<3;k++)center[k]+=face->vertices[n][k]/face->count;
        for(k=0;k<3;k++){probe.start[k]=body->state.position[k]+center[k]+2*face->plane[k];probe.end[k]=probe.start[k]-4*face->plane[k];}
        probe.sphere.radius=.1f;
        CHECK(!rf_geomod_piece_registry_support(r,&probe,1,&hit,&matched));CHECK(matched && !hit.stable);
        body->state.flags&=~0x80000000u;
        CHECK(!rf_geomod_piece_registry_support(r,&probe,1,&hit,&matched));CHECK(matched && hit.stable);
        body->state.velocity[0]=1;
        CHECK(!rf_geomod_piece_registry_support(r,&probe,1,&hit,&matched));CHECK(matched && !hit.stable);
        body->state.velocity[0]=0;body->state.vector_c8[0]=1;
        CHECK(!rf_geomod_piece_registry_support(r,&probe,1,&hit,&matched));CHECK(matched && !hit.stable);
        kept=hit;CHECK(!rf_geomod_piece_registry_support(NULL,&probe,1,&hit,&matched));CHECK(!matched && !memcmp(&hit,&kept,sizeof(hit)));
        body->state=saved;
    }
    {
        rf_geomod_notify_change change={0};rf_physics_body_state saved=body->state;uint32_t woke=999;
        float center[3];memcpy(center,body->state.position,12);change.radius=1;
        body->state.flags&=~0x80000000u;center[0]+=1000;
        CHECK(!rf_geomod_piece_registry_notify(r,&change,center,&woke) && !woke && !(body->state.flags&0x80000000u));
        center[0]=NAN;woke=999;CHECK(rf_geomod_piece_registry_notify(r,&change,center,&woke)==RF_FORMAT && woke==999 && !(body->state.flags&0x80000000u));
        memcpy(center,body->state.position,12);
        CHECK(!rf_geomod_piece_registry_notify(r,&change,center,&woke) && woke==1 && (body->state.flags&0x80000000u));
        CHECK(!memcmp(body->state.velocity,saved.velocity,12) && !memcmp(body->state.position,saved.position,12));
        CHECK(!rf_geomod_piece_registry_notify(r,&change,center,&woke) && !woke);
        body->state=saved;
    }
    bytes=rf_geomod_piece_registry_bytes(r);
    CHECK(!rf_geomod_piece_registry_begin(r,0));
    CHECK(rf_geomod_terrain_cut_box_checked(t,center[3],extent[3],7,reject_candidate,NULL)==RF_IO);
    CHECK(rf_geomod_piece_registry_bytes(r)>bytes && rf_geomod_piece_registry_count(r)==1);
    rf_geomod_piece_registry_abort(r);CHECK(rf_geomod_piece_registry_bytes(r)==bytes);
    CHECK(!rf_geomod_piece_registry_changed_boxes(r,0,boxes,&box_count) && box_count==1);
    CHECK(!memcmp(boxes,initial_boxes,sizeof(*boxes)));
    CHECK(body->state.position[0]==moved);
    CHECK(!rf_geomod_piece_registry_begin(r,0));
    CHECK(!rf_geomod_terrain_cut_box(t,center[3],extent[3],7));rf_geomod_piece_registry_commit(r);
    CHECK(rf_geomod_piece_registry_count(r)==2 && body->state.position[0]==moved);
    CHECK(!rf_geomod_piece_registry_changed_boxes(r,1,boxes,&box_count) && box_count==1);
    CHECK(memcmp(boxes,initial_boxes,sizeof(*boxes)));
    /* Excluding the source must still find a distinct chunk and its velocity. */
    {
        rf_geomod_piece_batch *second;rf_geomod_owned_piece target;rf_physics_body *other;
        rf_collision_body_query query={0};rf_collision_body_sphere sphere={0};
        rf_geomod_registry_body_hit hit,kept;uint32_t k,n,matched;float saved_velocity[3];
        CHECK(!rf_geomod_piece_registry_get(r,1,&second));
        CHECK(!rf_geomod_piece_batch_get(second,0,&target,&other));
        memcpy(saved_velocity,other->state.velocity,12);other->state.velocity[2]=3;
        for(k=0;k<3;k++) {
            float center=0;const rf_collision_face *face=target.collision;
            for(n=0;n<face->count;n++)center+=face->vertices[n][k]/face->count;
            query.start[k]=other->state.position[k]+center+2*face->plane[k];
            query.end[k]=query.start[k]-4*face->plane[k];query.matrix[k][k]=1;
        }
        sphere.radius=.1f;query.spheres=&sphere;query.count=1;query.radius=2;query.limit=1;
        CHECK(!rf_geomod_piece_registry_body_sweep_excluding(r,0,0,&query,1,&hit,&matched));
        CHECK(matched && hit.batch==1 && hit.piece==0 && hit.contact.velocity[2]==3);
        kept=hit;
        CHECK(!rf_geomod_piece_registry_body_sweep_excluding(r,1,0,&query,1,&hit,&matched));
        CHECK(!matched && !memcmp(&hit,&kept,sizeof(hit)));
        memcpy(other->state.velocity,saved_velocity,12);
    }
    /* A later invalid body must not wake an earlier valid sleeping body. */
    {
        rf_geomod_piece_batch *second;rf_geomod_owned_piece view;rf_physics_body *later;
        rf_geomod_notify_change change={0};rf_physics_body_state saved=body->state,later_saved;
        float wake_center[3];uint32_t woke=999;
        CHECK(!rf_geomod_piece_registry_get(r,1,&second));
        CHECK(!rf_geomod_piece_batch_get(second,0,&view,&later));later_saved=later->state;
        memcpy(wake_center,body->state.position,12);change.radius=1000;
        body->state.flags&=~0x80000000u;later->state.position[0]=NAN;
        CHECK(rf_geomod_piece_registry_notify(r,&change,wake_center,&woke)==RF_FORMAT);
        CHECK(woke==999 && !(body->state.flags&0x80000000u));
        body->state=saved;later->state=later_saved;
    }
    /* Clone decode + mutate invokes separate traversals; rewind must preserve
     * live bodies and never construct duplicates for either traversal. */
    CHECK(!rf_geomod_piece_registry_begin(r,0));
    {
        unsigned char encoded[RF_GEOMOD_HISTORY_MAX_BYTES];uint32_t size;
        CHECK(!rf_geomod_terrain_history_size(t,&size));CHECK(!rf_geomod_terrain_history_encode(t,encoded,size));
        CHECK(!rf_geomod_terrain_history_decode(t,encoded,size));
        CHECK(!rf_geomod_piece_registry_rewind(r));
        CHECK(!rf_geomod_terrain_history_decode(t,encoded,size));
    }
    rf_geomod_piece_registry_commit(r);CHECK(rf_geomod_piece_registry_count(r)==2 && body->state.position[0]==moved);
    {
        unsigned char *snapshot,*bad,*check;uint32_t size;rf_physics_body_state saved_body;
        body->state.velocity[1]=-2;body->state.mass_vector_d4[0]=.3f;body->state.vector_c8[0]=.4f;
        body->state.vector_e0[2]=.7f;body->state.vector_ec[1]=.2f;body->state.coefficients[0]=.125f;
        saved_body=body->state;
        CHECK(rf_geomod_piece_batch_alive(first,0));
        CHECK(!rf_geomod_piece_registry_damage(r,0,0,100000));
        CHECK(!rf_geomod_piece_batch_alive(first,0));
        {
            rf_geomod_piece_hit hit;float start[3]={0},delta[3];uint32_t matched,k,n;
            const rf_collision_face *face=piece.collision;
            for(k=0;k<3;k++) {
                for(n=0;n<face->count;n++)start[k]+=face->vertices[n][k]/face->count;
                start[k]+=body->state.position[k]+face->plane[k]*2;delta[k]=-4*face->plane[k];
            }
            CHECK(!rf_geomod_piece_batch_sweep(first,0,start,delta,.1f,1,&hit,&matched));CHECK(!matched);
        }
        CHECK(!rf_geomod_piece_registry_state_size(r,&size));CHECK(size>336);
        snapshot=malloc(size);bad=malloc(size);check=malloc(size);CHECK(snapshot && bad && check);
        CHECK(!rf_geomod_piece_registry_state_encode(r,snapshot,size));
        {
            rf_geomod_terrain *restored=NULL;rf_geomod_piece_registry *restored_pieces=NULL;
            unsigned char *history;uint32_t history_bytes;
            CHECK(!rf_geomod_terrain_history_size(t,&history_bytes));history=malloc(history_bytes);CHECK(history);
            CHECK(!rf_geomod_terrain_history_encode(t,history,history_bytes));
            CHECK(!rf_geomod_terrain_open(mesh,filters,generated,0,4096,800,1179648,&restored));
            CHECK(!rf_geomod_piece_registry_open(generated,7,2.5f,.5f,.25f,0,2097152,&restored_pieces));
            CHECK(!rf_geomod_terrain_set_extraction(restored,rf_geomod_piece_registry_emit,restored_pieces));
            CHECK(!rf_geomod_piece_registry_begin(restored_pieces,1));
            CHECK(!rf_geomod_terrain_history_decode(restored,history,history_bytes));
            rf_geomod_piece_registry_commit(restored_pieces);
            CHECK(!rf_geomod_piece_registry_state_decode(restored_pieces,snapshot,size));
            CHECK(!rf_geomod_piece_registry_state_encode(restored_pieces,check,size));
            CHECK(!memcmp(snapshot,check,size));
            {rf_geomod_piece_batch *rb;CHECK(!rf_geomod_piece_registry_get(restored_pieces,0,&rb));CHECK(!rf_geomod_piece_batch_alive(rb,0));}
            rf_geomod_terrain_close(&restored);rf_geomod_piece_registry_close(&restored_pieces);free(history);
        }
        body->state.position[1]+=7;body->state.velocity[2]=3;
        CHECK(!rf_geomod_piece_registry_state_decode(r,snapshot,size));
        CHECK(!memcmp(&saved_body,&body->state,sizeof(saved_body)));
        CHECK(!rf_geomod_piece_registry_state_encode(r,check,size));CHECK(!memcmp(snapshot,check,size));
        for(uint32_t fault=0;fault<8;fault++) {
            memcpy(bad,snapshot,size);body->state.position[1]=123;
            if(fault==0)bad[0]='X';
            if(fault==1)bad[size-328]^=1; /* Later identity, after earlier valid bodies. */
            if(fault==2){bad[16+12+12]=0;bad[16+12+13]=0;bad[16+12+14]=128;bad[16+12+15]=63;} /* Wrong immutable mass. */
            if(fault==3){bad[size-328+12]=0;bad[size-328+13]=0;bad[size-328+14]=192;bad[size-328+15]=127;} /* NaN. */
            if(fault==4)memset(bad+16+12+28*4,0,36); /* Degenerate orientation. */
            if(fault==5){uint32_t nan=0x7fc00000;memcpy(bad+16+320,&nan,4);}
            if(fault==6)memset(bad+16+324,0,4); /* Dead health cannot become live. */
            if(fault==7){uint32_t unknown=0x400000;memcpy(bad+size-4,&unknown,4);}
            CHECK(rf_geomod_piece_registry_state_decode(r,bad,size)!=RF_OK);
            CHECK(body->state.position[1]==123);
        }
        CHECK(rf_geomod_piece_registry_state_decode(r,snapshot,size-1)!=RF_OK);
        CHECK(!rf_geomod_piece_registry_state_decode(r,snapshot,size));
        {
            uint32_t n=(size-16)/328,old_size=16+n*320,version=1;
            memcpy(bad,snapshot,16);memcpy(bad+4,&version,4);memcpy(bad+8,&old_size,4);
            for(uint32_t j=0;j<n;j++)memcpy(bad+16+j*320,snapshot+16+j*328,320);
            CHECK(!rf_geomod_piece_registry_state_decode(r,bad,old_size));
            CHECK(rf_geomod_piece_batch_alive(first,0)); /* Legacy means birth health. */
            CHECK(!rf_geomod_piece_registry_state_decode(r,snapshot,size));CHECK(!rf_geomod_piece_batch_alive(first,0));
        }
        CHECK(!rf_geomod_piece_registry_begin(r,0));
        CHECK(rf_geomod_piece_registry_state_decode(r,snapshot,size)==RF_RANGE);
        rf_geomod_piece_registry_abort(r);free(snapshot);free(bad);free(check);
    }
    CHECK(!rf_geomod_piece_registry_begin(r,1));CHECK(!rf_geomod_terrain_reset(t));rf_geomod_piece_registry_commit(r);
    CHECK(!rf_geomod_piece_registry_count(r));
    rf_geomod_piece_registry_close(&r);rf_geomod_piece_registry_close(&r);rf_geomod_terrain_close(&t);
    puts("PASS registry deduplication, moved-body retention, rejected-edit cleanup, replay rewind and reset");
}
static int run(uint32_t star)
{
    rf_geomod_vertex vertices[24]={0},saved[4096];rf_geomod_face faces[6],saved_faces[800];
    rf_collision_face_filter filters[6]={{0}},generated={0};rf_geomod_terrain *t=NULL,*reload=NULL;
    rf_geomod_mesh_view mesh={vertices,faces,24,6,0};rf_geomod_terrain_view view,after;
    const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};
    const float centers[4][3]={{0,0,0},{5,0,0},{-1,0,0},{-6,0,0}},extents[4][3]={{1,12,12},{2,2,2},{2,2,2},{1,12,12}};
    const double volumes[4]={3600,3600,3568,1568};staging stage={0},decoded={0};uint32_t bytes;
    unsigned char encoded[RF_GEOMOD_HISTORY_MAX_BYTES];
    for(uint32_t axis=0;axis<3;axis++)for(uint32_t side=0;side<2;side++) {
        uint32_t f=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;faces[f]=(rf_geomod_face){f*4,4,5,f};
        for(uint32_t j=0;j<4;j++) {uint32_t k=side?j:3-j;vertices[f*4+j].position[axis]=side?10:-10;
            vertices[f*4+j].position[a]=u[k]*10;vertices[f*4+j].position[b]=w[k]*10;}
    }
    if(!star)registry_lifetime(&mesh,filters,&generated);
    CHECK(!rf_geomod_terrain_open(&mesh,filters,&generated,0,4096,800,1179648,&t));
    CHECK(!rf_geomod_terrain_set_extraction(t,emit,&stage));
    for(uint32_t c=0;c<4;c++) {
        clear(&stage);
        CHECK(!rf_geomod_terrain_get(t,&view));
        memcpy(saved,view.mesh.vertices,view.mesh.vertex_count*sizeof(*saved));
        memcpy(saved_faces,view.mesh.faces,view.mesh.face_count*sizeof(*saved_faces));
        stage.reject=c==3?4:1;
        CHECK(cut(t,centers[c],extents[c],star)==RF_IO);
        CHECK(stage.count==(c==3?1u:0u));
        CHECK(!rf_geomod_terrain_get(t,&after));
        CHECK(after.cuts==c && after.mesh.vertex_count==view.mesh.vertex_count && after.mesh.face_count==view.mesh.face_count);
        CHECK(!memcmp(saved,after.mesh.vertices,view.mesh.vertex_count*sizeof(*saved)));
        CHECK(!memcmp(saved_faces,after.mesh.faces,view.mesh.face_count*sizeof(*saved_faces)));
        clear(&stage);
        CHECK(!cut(t,centers[c],extents[c],star));
        CHECK(!rf_geomod_terrain_get(t,&view));CHECK(fabs(volume(&view.mesh)-volumes[c])<.0001);
        CHECK(stage.count==(c==3?2u:1u));
        printf("PASS production extraction star%u cut%u volume%g batches%u peak%u\n",star,c+1,volume(&view.mesh),stage.count,view.peak_bytes);
    }
    CHECK(rf_geomod_terrain_set_extraction(t,NULL,NULL)==RF_RANGE);
    CHECK(!rf_geomod_terrain_history_size(t,&bytes));CHECK(!rf_geomod_terrain_history_encode(t,encoded,bytes));
    CHECK(!rf_geomod_terrain_open(&mesh,filters,&generated,0,4096,800,1179648,&reload));
    CHECK(!rf_geomod_terrain_set_extraction(reload,emit,&decoded));
    CHECK(!rf_geomod_terrain_history_decode(reload,encoded,bytes));CHECK(!rf_geomod_terrain_get(reload,&after));
    CHECK(after.mesh.vertex_count==view.mesh.vertex_count && after.mesh.face_count==view.mesh.face_count);
    CHECK(!memcmp(after.mesh.vertices,view.mesh.vertices,view.mesh.vertex_count*sizeof(*saved)));
    CHECK(!memcmp(after.mesh.faces,view.mesh.faces,view.mesh.face_count*sizeof(*saved_faces)));
    CHECK(decoded.count==stage.count && decoded.random.value==stage.random.value);
    for(uint32_t b=0;b<stage.count;b++)CHECK(rf_geomod_piece_batch_count(stage.batch[b])==rf_geomod_piece_batch_count(decoded.batch[b]));
    puts("PASS production extraction rejection rollback and checkpoint replay");
    clear(&stage);
    {const float center[3]={0,0,0},extent[3]={100,100,100};
     CHECK(!rf_geomod_terrain_cut_box(t,center,extent,7));
     CHECK(!rf_geomod_terrain_get(t,&after));
     CHECK(!after.mesh.vertex_count && !after.mesh.face_count);
     puts("PASS extraction accepts completely consumed retained terrain");}
    clear(&decoded);clear(&stage);rf_geomod_terrain_close(&reload);rf_geomod_terrain_close(&t);return 0;
}

static void life_damage(void)
{
    const float amounts[]={0,1,99.999f,100,100.00001f,200,400};
    const float expected[]={250,250,250,250,150,50,-150};
    rf_geomod_piece_life life,saved;uint32_t i;
    for(i=0;i<7;i++) {
        CHECK(!rf_geomod_piece_life_init(5,&life));
        CHECK(!rf_geomod_piece_life_damage(&life,amounts[i]));
        CHECK(life.health==expected[i]);CHECK(!!(life.flags&2)==(i==6));
        CHECK(!!(life.flags&0x200000)==(i!=0));
    }
    saved=life;CHECK(!rf_geomod_piece_life_damage(&life,400));CHECK(!memcmp(&life,&saved,sizeof(life)));
    CHECK(!rf_geomod_piece_life_init(10,&life));
    CHECK(!rf_geomod_piece_life_damage(&life,200));CHECK(life.health==300 && !(life.flags&2));
    CHECK(!rf_geomod_piece_life_damage(&life,300));CHECK(life.health==0 && (life.flags&2));
    saved=life;CHECK(rf_geomod_piece_life_damage(&life,NAN)==RF_FORMAT);CHECK(!memcmp(&life,&saved,sizeof(life)));
    CHECK(rf_geomod_piece_life_init(INFINITY,&life)==RF_FORMAT);CHECK(!memcmp(&life,&saved,sizeof(life)));
    CHECK(!rf_geomod_piece_life_init(5,&life));life.flags=4;
    CHECK(!rf_geomod_piece_life_damage(&life,400));CHECK(life.health==250 && life.flags==(4|0x200000));
}
int main(void){life_damage();CHECK(!run(0));CHECK(!run(1));return 0;}
