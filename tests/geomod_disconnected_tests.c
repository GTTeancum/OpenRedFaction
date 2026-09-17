/* Compose real terrain cuts with component classification and placement.
 * This does not yet publish detached bodies into the live scene. */
#define main legacy_history_tests
#include "geomod_history_check_tests.c"
#undef main
#include <math.h>
#include <float.h>
#include "rf/geomod_piece_bank.h"
#include "rf/geomod_solid_clip.h"

static void cap_area(const rf_collision_face *solid,uint32_t faces,uint32_t axis,float at,double expected)
{
    static rf_geomod_vertex vertices[2][2048];static rf_geomod_fragment fragments[2][512];
    static rf_geomod_vertex saved_vertices[2048];static rf_geomod_fragment saved_fragments[512];
    static uint16_t edges[2][2048];uint16_t seed_edges[4]={60000,60001,60002,60003},planes[256];
    rf_geomod_solid_clip_work work={{vertices[0],vertices[1]},{fragments[0],fragments[1]},2048,512,{edges[0],edges[1]}};
    rf_geomod_solid_clip_result result,kept;rf_geomod_vertex polygon[4]={0};
    const int x[4]={-1,1,1,-1},y[4]={-1,-1,1,1};uint32_t i,j,k,mode,a=(axis+1)%3,b=(axis+2)%3;
    for(i=0;i<4;i++){polygon[i].position[axis]=at;polygon[i].position[a]=(float)x[i]*15;polygon[i].position[b]=(float)y[i]*15;polygon[i].uv[0]=polygon[i].position[a];polygon[i].uv[1]=polygon[i].position[b];}
    CHECK(faces<=256);for(i=0;i<faces;i++)planes[i]=(uint16_t)(100+i);
    for(mode=0;mode<2;mode++) {
        double area=0;
        CHECK(!rf_geomod_polygon_clip_solid(polygon,4,solid,faces,mode,&work,&result));
        CHECK(!result.edges);kept=result;
        memcpy(saved_vertices,result.vertices,result.vertex_count*sizeof(*saved_vertices));
        memcpy(saved_fragments,result.fragments,result.fragment_count*sizeof(*saved_fragments));
        CHECK(!rf_geomod_polygon_clip_solid_tracked(polygon,4,solid,faces,mode,seed_edges,planes,&work,&result));
        CHECK(result.vertex_count==kept.vertex_count && result.fragment_count==kept.fragment_count);
        CHECK(!memcmp(saved_vertices,result.vertices,result.vertex_count*sizeof(*saved_vertices)));
        CHECK(!memcmp(saved_fragments,result.fragments,result.fragment_count*sizeof(*saved_fragments)));
        for(i=0;i<result.fragment_count;i++) {
            const rf_geomod_fragment *f=result.fragments+i;const rf_geomod_vertex *v=result.vertices+f->first;
            for(j=1;j+1<f->count;j++)area+=fabs(((double)v[j].position[a]-v[0].position[a])*(v[j+1].position[b]-v[0].position[b])-((double)v[j].position[b]-v[0].position[b])*(v[j+1].position[a]-v[0].position[a]))*.5;
            for(j=0;j<f->count;j++)for(k=0;k<2;k++)CHECK(fabs(v[j].uv[k]-v[j].position[k?b:a])<0.00001);
            for(j=0;j<f->count;j++) {
                uint16_t id=result.edges[f->first+j];uint32_t end;
                for(end=0;end<2;end++) {
                    const float *p=v[(j+end)%f->count].position;double distance;
                    if(id>=60000) {
                        CHECK(id<=60003);distance=(id==60000?p[b]+15:id==60001?p[a]-15:id==60002?p[b]-15:p[a]+15);
                    } else {
                        CHECK(id>=100 && (uint32_t)(id-100)<faces);distance=solid[id-100].plane[3];
                        for(k=0;k<3;k++)distance+=(double)solid[id-100].plane[k]*p[k];
                    }
                    CHECK(fabs(distance)<0.0001);
                }
            }
        }
        CHECK(fabs(area-(mode?expected:900-expected))<0.0001);
    }
    kept=result;work.vertex_capacity=4;
    CHECK(rf_geomod_polygon_clip_solid(polygon,4,solid,faces,1,&work,&result)==RF_RANGE);
    CHECK(!memcmp(&result,&kept,sizeof(result)));
    CHECK(rf_geomod_polygon_clip_solid_tracked(polygon,4,solid,faces,1,seed_edges,planes,&work,&result)==RF_RANGE);
    CHECK(!memcmp(&result,&kept,sizeof(result)));
    work.vertex_capacity=2048;work.edges[1]=NULL;
    CHECK(rf_geomod_polygon_clip_solid_tracked(polygon,4,solid,faces,1,seed_edges,planes,&work,&result)==RF_RANGE);
    CHECK(!memcmp(&result,&kept,sizeof(result)));
}

static void concave_caps(void)
{
    rf_geomod_mesh_view source;rf_geomod_terrain *t;rf_geomod_terrain_view view;
    float center[3]={9,9,0},extent[3]={2,2,12};
    make_source(&source);t=open_terrain(&source,1024*1024,800);
    CHECK(!rf_geomod_terrain_cut_box(t,center,extent,3));CHECK(!rf_geomod_terrain_get(t,&view));
    cap_area(view.faces,view.mesh.face_count,2,0,391);
    cap_area(view.faces,view.mesh.face_count,0,8,340);
    cap_area(view.faces,view.mesh.face_count,0,6,400);
    rf_geomod_terrain_close(&t);
    t=open_terrain(&source,1024*1024,800);center[0]=center[1]=0;extent[2]=2;
    CHECK(!rf_geomod_terrain_cut_box(t,center,extent,3));CHECK(!rf_geomod_terrain_get(t,&view));
    cap_area(view.faces,view.mesh.face_count,2,0,384);
    rf_geomod_terrain_close(&t);
    {
        uint32_t i;make_source(&source);
        for(i=0;i<source.vertex_count;i++) {
            float x=original_vertices[i].position[0],y=original_vertices[i].position[1];
            original_vertices[i].position[0]=.6f*x-.8f*y;
            original_vertices[i].position[1]=.8f*x+.6f*y;
        }
        t=open_terrain(&source,1024*1024,800);CHECK(!rf_geomod_terrain_get(t,&view));
        cap_area(view.faces,view.mesh.face_count,2,0,400);
        rf_geomod_terrain_close(&t);
    }
}

static void shapes(void)
{
    static const struct {uint32_t bounds[6],radius,attempts,shape[5],split;} rows[]={
#include "fixtures/geomod_piece_shape.inc"
    };
    uint32_t i;rf_geomod_piece_shape result,kept;float bounds[6],radius;
    for(i=0;i<sizeof(rows)/sizeof(rows[0]);i++) {
        memcpy(bounds,rows[i].bounds,sizeof(bounds));memcpy(&radius,&rows[i].radius,4);
        CHECK(!rf_geomod_piece_shape_get(bounds,bounds+3,radius,rows[i].attempts,&result));
        CHECK(!memcmp(&result,rows[i].shape,20) && result.subdivide==rows[i].split);
    }
    kept=result;bounds[3]=bounds[0];
    CHECK(rf_geomod_piece_shape_get(bounds,bounds+3,radius,0,&result)==RF_FORMAT);
    CHECK(!memcmp(&result,&kept,sizeof(result)));
    bounds[3]=NAN;
    CHECK(rf_geomod_piece_shape_get(bounds,bounds+3,radius,0,&result)==RF_FORMAT);
    CHECK(!memcmp(&result,&kept,sizeof(result)));
    printf("PASS %u original piece shapes and split gates\n",i);
}

static void extraction(const rf_geomod_terrain_view *view,const uint32_t *labels,uint32_t selected)
{
    rf_geomod_vertex vertices[256],saved_v[256];rf_geomod_face faces[64],saved_f[64];
    uint32_t old[64],saved_old[64],i,j,part,offset=0;uint16_t neighbors[256];
    rf_geomod_mesh_view output[2],saved_output[2];
    CHECK(view->mesh.vertex_count<=256 && view->mesh.face_count<=64);
    memset(vertices,0xa5,sizeof(vertices));memset(faces,0xa5,sizeof(faces));
    memset(old,0xa5,sizeof(old));memset(output,0xa5,sizeof(output));
    memcpy(saved_v,vertices,sizeof(vertices));memcpy(saved_f,faces,sizeof(faces));
    memcpy(saved_old,old,sizeof(old));memcpy(saved_output,output,sizeof(output));
#define EXTRACT(label,vc,fc) rf_geomod_component_extract(&view->mesh,labels,label,vertices,vc,faces,fc,old,output,output+1)
#define UNCHANGED() CHECK(!memcmp(vertices,saved_v,sizeof(vertices)) && !memcmp(faces,saved_f,sizeof(faces)) && !memcmp(old,saved_old,sizeof(old)) && !memcmp(output,saved_output,sizeof(output)))
    CHECK(EXTRACT(selected,0,64)==RF_RANGE);UNCHANGED();
    CHECK(EXTRACT(selected,256,0)==RF_RANGE);UNCHANGED();
    CHECK(EXTRACT(1000,256,64)==RF_NOT_FOUND);UNCHANGED();
    {
        rf_geomod_vertex bad_vertices[256];rf_geomod_mesh_view bad=view->mesh;
        memcpy(bad_vertices,bad.vertices,bad.vertex_count*sizeof(*bad_vertices));
        bad.vertices=bad_vertices;bad_vertices[bad.vertex_count-1].uv[1]=NAN;
        CHECK(rf_geomod_component_extract(&bad,labels,selected,vertices,256,faces,64,old,output,output+1)==RF_FORMAT);
        UNCHANGED();
    }
    CHECK(!EXTRACT(selected,256,64));
    CHECK(output[0].face_count+output[1].face_count==view->mesh.face_count);
    CHECK(output[0].vertex_count+output[1].vertex_count==view->mesh.vertex_count);
    for(part=0;part<2;part++) {
        const rf_geomod_mesh_view *mesh=output+part;
        rf_collision_face rebound[64];rf_collision_face_filter filter[64];float positions[256][3];
        if(!mesh->face_count)continue;
        CHECK(!rf_geomod_seed_adjacency(mesh,neighbors,256));
        for(i=0;i<mesh->face_count;i++) {
            const rf_geomod_face *a=mesh->faces+i,*b=view->mesh.faces+old[offset+i];
            CHECK((labels[old[offset+i]]==selected)==part);
            CHECK(a->count==b->count && a->material==b->material && a->source_face==b->source_face);
            CHECK(!memcmp(mesh->vertices+a->first,view->mesh.vertices+b->first,a->count*sizeof(*vertices)));
            if(i)CHECK(old[offset+i]>old[offset+i-1]);
            filter[i]=view->faces[old[offset+i]].filter;
        }
        CHECK(!rf_geomod_collision_faces(mesh,filter,positions,256,rebound,64));
        if(part==0 && mesh->face_count==6) {
            float lo[3]={FLT_MAX,FLT_MAX,FLT_MAX},hi[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};uint32_t axis;
            for(i=0;i<mesh->vertex_count;i++)for(axis=0;axis<3;axis++){float p=mesh->vertices[i].position[axis];if(p<lo[axis])lo[axis]=p;if(p>hi[axis])hi[axis]=p;}
            for(axis=0;axis<3;axis++) {
                cap_area(rebound,6,axis,(lo[axis]+hi[axis])*.5,(double)(hi[(axis+1)%3]-lo[(axis+1)%3])*(hi[(axis+2)%3]-lo[(axis+2)%3]));
                cap_area(rebound,6,axis,lo[axis]-2,0);
                cap_area(rebound,6,axis,hi[axis]+2,0);
            }
        }
        for(i=0;i<mesh->face_count;i++) {
            const rf_collision_face *a=rebound+i,*b=view->faces+old[offset+i];
            CHECK(!memcmp(a->plane,b->plane,sizeof(a->plane)) && a->count==b->count);
            for(j=0;j<a->count;j++)CHECK(!memcmp(a->vertices[j],b->vertices[j],12));
        }
        offset+=mesh->face_count;
    }
    CHECK(offset==view->mesh.face_count);
    {
        rf_geomod_mesh_view *mesh=output+1;rf_geomod_piece_placement placement;
        rf_collision_face rebound[64];rf_collision_face_filter filter[64];float positions[256][3];
        rf_geomod_vertex *corners=vertices+output[0].vertex_count;
        CHECK(!rf_geomod_mesh_recenter(corners,mesh->vertex_count,corners,&placement));
        for(i=0;i<mesh->face_count;i++)filter[i]=view->faces[old[output[0].face_count+i]].filter;
        CHECK(!rf_geomod_collision_faces(mesh,filter,positions,256,rebound,64));
        for(i=0;i<mesh->face_count;i++) {
            const rf_collision_face *before=view->faces+old[output[0].face_count+i];
            const rf_geomod_face *face=mesh->faces+i;
            const rf_geomod_vertex *original=view->mesh.vertices+view->mesh.faces[old[output[0].face_count+i]].first;
            double distance=before->plane[3];uint32_t axis;
            for(axis=0;axis<3;axis++)distance+=(double)before->plane[axis]*placement.origin[axis];
            CHECK(!memcmp(rebound[i].plane,before->plane,12));
            CHECK(fabs(rebound[i].plane[3]-distance)<0.00001);
            for(j=0;j<face->count;j++) {
                CHECK(!memcmp(mesh->vertices[face->first+j].uv,original[j].uv,8));
                for(axis=0;axis<3;axis++)CHECK(fabs((double)mesh->vertices[face->first+j].position[axis]+placement.origin[axis]-original[j].position[axis])<0.00001);
            }
        }
    }
#undef UNCHANGED
#undef EXTRACT
}

static void inspect(const rf_geomod_terrain_view *view, uint32_t expected)
{
    uint32_t words,count,largest,i,j,k,c,n,solid;
    uint32_t *work,*labels;
    int32_t *signed_labels;
    float (*points)[3],(*local)[3];double total_volume=0;
    CHECK(!rf_geomod_component_work_size(&view->mesh,&words));
    work=malloc(words*4);labels=malloc(view->mesh.face_count*4);
    signed_labels=malloc(view->mesh.face_count*4);
    points=malloc(view->mesh.vertex_count*sizeof(*points));
    local=malloc(view->mesh.vertex_count*sizeof(*local));
    CHECK(work && labels && signed_labels && points && local);
    CHECK(!rf_geomod_mesh_components(&view->mesh,NULL,work,words,labels,&count,&largest));
    CHECK(count==expected && largest<count);
    extraction(view,labels,count==1?0:(largest+1)%count);
    for(i=0;i<view->mesh.face_count;i++)signed_labels[i]=(int32_t)labels[i];
    for(c=0;c<count;c++) {
        rf_collision_bounds bounds={0};rf_geomod_piece_placement placement;double volume=0;
        for(k=0;k<3;k++){bounds.minimum[k]=FLT_MAX;bounds.maximum[k]=-FLT_MAX;}
        n=0;
        for(i=0;i<view->mesh.face_count;i++)if(labels[i]==c) {
            const rf_geomod_face *face=view->mesh.faces+i;
            const float *p=view->mesh.vertices[face->first].position;
            for(j=1;j+1<face->count;j++) {
                const float *q=view->mesh.vertices[face->first+j].position;
                const float *r=view->mesh.vertices[face->first+j+1].position;
                volume+=((double)p[0]*(q[1]*r[2]-q[2]*r[1])+
                    (double)p[1]*(q[2]*r[0]-q[0]*r[2])+
                    (double)p[2]*(q[0]*r[1]-q[1]*r[0]))/6;
            }
            for(j=0;j<face->count;j++,n++)for(k=0;k<3;k++) {
                float p=view->mesh.vertices[face->first+j].position[k];points[n][k]=p;
                if(p<bounds.minimum[k])bounds.minimum[k]=p;
                if(p>bounds.maximum[k])bounds.maximum[k]=p;
            }
        }
        CHECK(n);
        CHECK(!rf_geomod_component_classify(view->faces,signed_labels,view->mesh.face_count,(int32_t)c,&bounds,&solid));
        CHECK(solid==1);
        CHECK(fabs(volume-(double)(bounds.maximum[0]-bounds.minimum[0])*
            (bounds.maximum[1]-bounds.minimum[1])*(bounds.maximum[2]-bounds.minimum[2]))<0.001);
        total_volume+=volume;
        CHECK(!rf_geomod_piece_recenter(points,n,local,&placement));
        for(i=0;i<n;i++)for(k=0;k<3;k++)
            CHECK(fabs((double)local[i][k]+placement.origin[k]-points[i][k])<0.00001);
    }
    CHECK(fabs(total_volume-(8000-800*(expected-1)))<0.001);
    printf("split components=%u faces=%u scratch=%u\n",count,view->mesh.face_count,words*4);
    free(local);free(points);free(signed_labels);free(labels);free(work);
}
static void opening(const rf_geomod_terrain_view *view,uint32_t axis,float location)
{
    uint32_t side,matched;rf_collision_tree_hit hit;
    for(side=0;side<2;side++) {
        float p[3]={0},d[3]={0};p[axis]=location;d[axis]=side?20:-20;
        CHECK(!rf_collision_thin_tree(view->tree->nodes,view->tree->node_count,
            view->tree->faces,view->tree->face_count,0,p,d,1,query_stack,8192,&hit,&matched));
        CHECK(matched && fabs((double)hit.hit.fraction-0.05)<0.00001);
    }
    /* Along the slice, the former solid must no longer block travel. */
    {
        float p[3]={0},d[3]={0};p[axis]=location;p[(axis+1)%3]=-15;d[(axis+1)%3]=30;
        CHECK(!rf_collision_thin_tree(view->tree->nodes,view->tree->node_count,
            view->tree->faces,view->tree->face_count,0,p,d,1,query_stack,8192,&hit,&matched));
        CHECK(!matched);
    }
}
static void subdivision_mesh_cuts(void)
{
    uint32_t axis,seed;
    for(axis=0;axis<3;axis++)for(seed=0;seed<5;seed++) {
        rf_geomod_mesh_view source,cut,result;rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view view;
        rf_geomod_vertex vertices[24];rf_geomod_face faces[6];rf_geomod_piece_cutter pose;
        rf_random_state random={seed*12345};
        float direction[3]={0};uint32_t f,j,k,words,count,largest,*work,*labels;double volume=0;
        direction[axis]=1;make_source(&source);
        CHECK(!rf_geomod_piece_cutter_prepare(direction,20,&random,&pose));
        CHECK(!rf_geomod_piece_cutter_mesh(&pose,7,vertices,faces));cut=(rf_geomod_mesh_view){vertices,faces,24,6,0};
        terrain=open_terrain(&source,1024*1024,800);
        {int status=rf_geomod_terrain_cut_convex(terrain,&cut);if(status)fprintf(stderr,"convex axis%u seed%u status%d\n",axis,seed,status);CHECK(!status);}
        CHECK(!rf_geomod_terrain_get(terrain,&view));result=view.mesh;
        for(f=0;f<result.face_count;f++) {
            const rf_geomod_face *face=result.faces+f;const float *a=result.vertices[face->first].position;
            for(j=1;j+1<face->count;j++) {
                const float *b=result.vertices[face->first+j].position,*c=result.vertices[face->first+j+1].position;
                for(k=0;k<3;k++)volume+=(double)a[k]*((double)b[(k+1)%3]*c[(k+2)%3]-(double)b[(k+2)%3]*c[(k+1)%3])/6;
            }
        }
        CHECK(fabs(volume-(8000-80/pose.basis[6+axis]))<.01);
        CHECK(!rf_geomod_component_work_size(&result,&words));work=malloc(words*4);labels=malloc(result.face_count*4);
        CHECK(work && labels && !rf_geomod_mesh_components(&result,NULL,work,words,labels,&count,&largest));
        if(count!=2)fprintf(stderr,"slab axis%u seed%u groups%u faces%u\n",axis,seed,count,result.face_count);
        CHECK(count==2);
        {uint16_t neighbors[1024];CHECK(!rf_geomod_seed_adjacency(&result,neighbors,1024));}
        {
            rf_collision_tree_hit hit;uint32_t matched,side;float start[3],delta[3];
            for(side=0;side<2;side++) {
                for(k=0;k<3;k++){start[k]=pose.offset[k];delta[k]=pose.basis[6+k]*(side?20:-20);}
                CHECK(!rf_collision_thin_tree(view.tree->nodes,view.tree->node_count,view.tree->faces,view.tree->face_count,
                    0,start,delta,1,query_stack,8192,&hit,&matched));
                CHECK(matched && fabs(hit.hit.fraction-.005)<.00001);
            }
            for(k=0;k<3;k++){start[k]=pose.offset[k]-pose.basis[k]*30;delta[k]=pose.basis[k]*60;}
            CHECK(!rf_collision_thin_tree(view.tree->nodes,view.tree->node_count,view.tree->faces,view.tree->face_count,
                0,start,delta,1,query_stack,8192,&hit,&matched));CHECK(!matched);
        }
        {
            rf_geomod_terrain *reloaded=open_terrain(&source,1024*1024,800);rf_geomod_terrain_view restored;uint32_t bytes;
            CHECK(!rf_geomod_terrain_history_size(terrain,&bytes));CHECK(bytes<=sizeof(encoded));
            CHECK(!rf_geomod_terrain_history_encode(terrain,encoded,bytes));
            CHECK(!rf_geomod_terrain_history_decode(reloaded,encoded,bytes));CHECK(!rf_geomod_terrain_get(reloaded,&restored));
            compare(&view,&restored);rf_geomod_terrain_close(&reloaded);
        }
        free(work);free(labels);rf_geomod_terrain_close(&terrain);
    }
    printf("PASS 15 tilted slab cuts: analytic volume and two extracted groups\n");
}
static void subdivision_worker(void)
{
    uint32_t seed,i;rf_geomod_mesh_view source;
    for(seed=0;seed<3;seed++) {
        rf_geomod_piece_bank *bank=NULL;rf_geomod_subdivision_stats stats={0};rf_random_state random={seed*12345};
        rf_collision_face_filter worker_filters[6],worker_generated;
        make_source(&source);memcpy(worker_filters,filters,sizeof(worker_filters));worker_generated=generated;
        for(i=0;i<6;i++)worker_filters[i].owner_kind=i+1;worker_generated.owner_kind=77;
        {int status=rf_geomod_piece_subdivide(&source,worker_filters,&worker_generated,7,2.5f,&random,2097152,&bank,&stats);
         if(status)fprintf(stderr,"worker seed%u status%d\n",seed,status);CHECK(!status);}
        CHECK(stats.attempts>0 && stats.attempts<=10 && stats.terminal==rf_geomod_piece_bank_count(bank));
        CHECK(stats.terminal>1 && stats.peak_bytes<=2097152);
        for(i=0;i<stats.terminal;i++) {
            rf_geomod_owned_piece piece;rf_physics_body body={0};uint16_t neighbors[128];
            CHECK(!rf_geomod_piece_bank_get(bank,i,&piece));CHECK(piece.mass_ready && piece.mass.mass>0);
            CHECK(!rf_geomod_seed_adjacency(&piece.mesh,neighbors,128));
            CHECK(!rf_geomod_piece_body_open(&piece,.5f,.25f,4096,&body));rf_physics_body_close(&body);
        }
        printf("PASS recursive worker seed%u attempts%u terminal%u discarded%u peak%u\n",seed,stats.attempts,stats.terminal,stats.discarded,stats.peak_bytes);
        {
            rf_geomod_piece_bank *again=NULL;rf_geomod_subdivision_stats other={0},sentinel;
            rf_random_state replay_random={seed*12345};
            CHECK(!rf_geomod_piece_subdivide(&source,worker_filters,&worker_generated,7,2.5f,&replay_random,2097152,&again,&other));
            CHECK(replay_random.value==random.value && !memcmp(&stats,&other,sizeof(stats)));
            for(i=0;i<stats.terminal;i++) {
                rf_geomod_owned_piece a,b;uint32_t f;
                CHECK(!rf_geomod_piece_bank_get(bank,i,&a) && !rf_geomod_piece_bank_get(again,i,&b));
                CHECK(!memcmp(&a.mass,&b.mass,sizeof(a.mass)) && !memcmp(&a.placement,&b.placement,sizeof(a.placement)));
                CHECK(a.mesh.vertex_count==b.mesh.vertex_count && a.mesh.face_count==b.mesh.face_count);
                CHECK(!memcmp(a.mesh.vertices,b.mesh.vertices,a.mesh.vertex_count*sizeof(*a.mesh.vertices)));
                CHECK(!memcmp(a.mesh.faces,b.mesh.faces,a.mesh.face_count*sizeof(*a.mesh.faces)));
                for(f=0;f<a.mesh.face_count;f++) {
                    uint32_t id=a.mesh.faces[f].source_face;CHECK(id<6 || id==UINT32_MAX);
                    CHECK(!memcmp(a.filters+f,id==UINT32_MAX?&worker_generated:worker_filters+id,sizeof(*worker_filters)));
                }
            }
            rf_geomod_piece_bank_close(&again);replay_random.value=seed*12345;
            memset(&other,0xa5,sizeof(other));sentinel=other;
            CHECK(rf_geomod_piece_subdivide(&source,worker_filters,&worker_generated,7,2.5f,&replay_random,stats.peak_bytes-1,&again,&other)==RF_RANGE);
            CHECK(!again && replay_random.value==seed*12345 && !memcmp(&other,&sentinel,sizeof(other)));
        }
        rf_geomod_piece_bank_close(&bank);
    }
}
static int subdivision_cutter_cases(void)
{
    static const struct {uint32_t axis[3],length,seed,next,output[15];} rows[]={
#include "fixtures/geomod_piece_cutter.inc"
    };
    uint32_t i;float axis[3],length;rf_geomod_piece_cutter out,kept;rf_random_state random;
    for(i=0;i<sizeof(rows)/sizeof(rows[0]);i++) {
        memcpy(axis,rows[i].axis,12);memcpy(&length,&rows[i].length,4);random.value=rows[i].seed;
        CHECK(!rf_geomod_piece_cutter_prepare(axis,length,&random,&out));
        CHECK(random.value==rows[i].next);CHECK(!memcmp(&out,rows[i].output,sizeof(out)));
    }
    kept=out;random.value=42;axis[0]=axis[1]=axis[2]=0;
    CHECK(rf_geomod_piece_cutter_prepare(axis,length,&random,&out)==RF_RANGE);
    CHECK(random.value==42 && !memcmp(&out,&kept,sizeof(out)));
    printf("PASS %u original subdivision cutter poses and RNG states\n",i);return 0;
}
int main(void)
{
    CHECK(!subdivision_cutter_cases());
    subdivision_mesh_cuts();
    subdivision_worker();
    uint32_t axis,bytes;rf_geomod_mesh_view source;
    shapes();
    concave_caps();
    for(axis=0;axis<3;axis++) {
        rf_geomod_terrain *live,*reload;rf_geomod_terrain_view a,b;
        float center[3]={0},extent[3]={12,12,12};extent[axis]=1;
        make_source(&source);live=open_terrain(&source,1024*1024,800);
        reload=open_terrain(&source,1024*1024,800);
        CHECK(!rf_geomod_terrain_get(live,&a));inspect(&a,1);
        CHECK(!rf_geomod_terrain_cut_box(live,center,extent,3));
        CHECK(!rf_geomod_terrain_get(live,&a));inspect(&a,2);opening(&a,axis,0);
        cap_area(a.faces,a.mesh.face_count,(axis+1)%3,0,360);
        CHECK(!rf_geomod_terrain_history_size(live,&bytes));
        CHECK(!rf_geomod_terrain_history_encode(live,encoded,bytes));
        CHECK(!rf_geomod_terrain_history_decode(reload,encoded,bytes));
        CHECK(!rf_geomod_terrain_get(reload,&b));compare(&a,&b);inspect(&b,2);opening(&b,axis,0);
        center[axis]=5;
        CHECK(!rf_geomod_terrain_cut_box(live,center,extent,3));
        CHECK(!rf_geomod_terrain_cut_box(reload,center,extent,3));
        CHECK(!rf_geomod_terrain_get(live,&a));CHECK(!rf_geomod_terrain_get(reload,&b));
        compare(&a,&b);inspect(&a,3);inspect(&b,3);
        cap_area(a.faces,a.mesh.face_count,(axis+1)%3,0,320);
        opening(&a,axis,0);opening(&a,axis,5);opening(&b,axis,0);opening(&b,axis,5);
        rf_geomod_terrain_close(&reload);rf_geomod_terrain_close(&live);
    }
    puts("PASS actual cuts: disconnected solids, orientation, placement, reload and next cut on all axes");
    return 0;
}
