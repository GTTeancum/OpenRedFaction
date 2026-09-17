/* Compose real terrain cuts with component classification and placement.
 * This does not yet publish detached bodies into the live scene. */
#define main legacy_history_tests
#include "geomod_history_check_tests.c"
#undef main
#include <math.h>
#include <float.h>

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
int main(void)
{
    uint32_t axis,bytes;rf_geomod_mesh_view source;
    shapes();
    for(axis=0;axis<3;axis++) {
        rf_geomod_terrain *live,*reload;rf_geomod_terrain_view a,b;
        float center[3]={0},extent[3]={12,12,12};extent[axis]=1;
        make_source(&source);live=open_terrain(&source,1024*1024,800);
        reload=open_terrain(&source,1024*1024,800);
        CHECK(!rf_geomod_terrain_get(live,&a));inspect(&a,1);
        CHECK(!rf_geomod_terrain_cut_box(live,center,extent,3));
        CHECK(!rf_geomod_terrain_get(live,&a));inspect(&a,2);opening(&a,axis,0);
        CHECK(!rf_geomod_terrain_history_size(live,&bytes));
        CHECK(!rf_geomod_terrain_history_encode(live,encoded,bytes));
        CHECK(!rf_geomod_terrain_history_decode(reload,encoded,bytes));
        CHECK(!rf_geomod_terrain_get(reload,&b));compare(&a,&b);inspect(&b,2);opening(&b,axis,0);
        center[axis]=5;
        CHECK(!rf_geomod_terrain_cut_box(live,center,extent,3));
        CHECK(!rf_geomod_terrain_cut_box(reload,center,extent,3));
        CHECK(!rf_geomod_terrain_get(live,&a));CHECK(!rf_geomod_terrain_get(reload,&b));
        compare(&a,&b);inspect(&a,3);inspect(&b,3);
        opening(&a,axis,0);opening(&a,axis,5);opening(&b,axis,0);opening(&b,axis,5);
        rf_geomod_terrain_close(&reload);rf_geomod_terrain_close(&live);
    }
    puts("PASS actual cuts: disconnected solids, orientation, placement, reload and next cut on all axes");
    return 0;
}
