/* Four chord intervals, matching the existing fragment corner path. Shared
 * serialized scratch is rebuilt per query; oversized shapes use full fallback. */
#define SCENE_FRAGMENT_EDGE_CAPACITY 128
typedef struct scene_fragment_edge {
    uint32_t a,b;float point[5][2][3],minimum[4][3],maximum[4][3];
} scene_fragment_edge;
static struct {scene_fragment_edge edges[SCENE_FRAGMENT_EDGE_CAPACITY];uint32_t count,complete;float minimum[3],maximum[3];} scene_fragment_edges;
static void scene_fragment_edge_prepare(const rf_geomod_mesh_view *mesh,const scene_fragment_path *path,
    uint32_t a,uint32_t b,scene_fragment_edge *edge)
{
    uint32_t step,end,k,j;edge->a=a;edge->b=b;
    for(step=0;step<5;step++)for(end=0;end<2;end++)for(k=0;k<3;k++) {
        const float *local=mesh->vertices[end?b:a].position;double value=path->position[step][k];
        for(j=0;j<3;j++)value+=(double)local[j]*path->basis[step][j*3+k];edge->point[step][end][k]=(float)value;
    }
    for(step=0;step<4;step++)for(k=0;k<3;k++) {
        edge->minimum[step][k]=fminf(fminf(edge->point[step][0][k],edge->point[step][1][k]),
            fminf(edge->point[step+1][0][k],edge->point[step+1][1][k]))-.0001f;
        edge->maximum[step][k]=fmaxf(fmaxf(edge->point[step][0][k],edge->point[step][1][k]),
            fmaxf(edge->point[step+1][0][k],edge->point[step+1][1][k]))+.0001f;
    }
}
static int scene_fragment_edges_prepare(const rf_geomod_mesh_view *mesh,const scene_fragment_path *path)
{
    uint32_t f,v,e;scene_fragment_edges.count=0;scene_fragment_edges.complete=1;
    for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;
        if(face->first>mesh->vertex_count || face->count>mesh->vertex_count-face->first)return RF_FORMAT;
        for(v=0;v<face->count;v++) {
            uint32_t a=face->first+v,b=face->first+(v+1)%face->count;
            const float *pa=mesh->vertices[a].position,*pb=mesh->vertices[b].position;
            if(!memcmp(pa,pb,12))continue;
            for(e=0;e<scene_fragment_edges.count;e++) {
                const scene_fragment_edge *edge=scene_fragment_edges.edges+e;
                const float *ea=mesh->vertices[edge->a].position,*eb=mesh->vertices[edge->b].position;
                if((!memcmp(pa,ea,12) && !memcmp(pb,eb,12)) || (!memcmp(pa,eb,12) && !memcmp(pb,ea,12)))break;
            }
            if(e<scene_fragment_edges.count)continue;
            if(e==SCENE_FRAGMENT_EDGE_CAPACITY){scene_fragment_edges.complete=0;return RF_OK;}
            scene_fragment_edge_prepare(mesh,path,a,b,scene_fragment_edges.edges+scene_fragment_edges.count);
            {uint32_t step,k;const scene_fragment_edge *edge=scene_fragment_edges.edges+scene_fragment_edges.count;
             for(step=0;step<4;step++)for(k=0;k<3;k++) {
                if((!scene_fragment_edges.count && !step) || edge->minimum[step][k]<scene_fragment_edges.minimum[k])scene_fragment_edges.minimum[k]=edge->minimum[step][k];
                if((!scene_fragment_edges.count && !step) || edge->maximum[step][k]>scene_fragment_edges.maximum[k])scene_fragment_edges.maximum[k]=edge->maximum[step][k];
             }}
            scene_fragment_edges.count++;
        }
    }
    return RF_OK;
}
static int scene_fragment_edge_pair(const rf_geomod_mesh_view *mesh,const scene_fragment_path *path,
    const scene_fragment_edge *edge,const float a[3],const float b[3],const float normal[3],
    const float minimum[3],const float maximum[3],float *limit,rf_collision_ray_hit *best,uint32_t *found)
{
    uint32_t step,k;int status;
    for(step=0;step<4 && step*.25f<*limit;step++) {
        rf_collision_ray_hit hit;uint32_t yes;float alignment=0;
        if(!scene_detached_box_overlap(edge->minimum[step],edge->maximum[step],minimum,maximum))continue;
        status=rf_collision_swept_edge(edge->point[step][0],edge->point[step][1],edge->point[step+1][0],edge->point[step+1][1],
            a,b,fminf(1,(*limit-step*.25f)*4),&hit,&yes);if(status)return status;if(!yes)continue;
        for(k=0;k<3;k++)alignment+=hit.normal[k]*normal[k];if(alignment<=0)continue;
        if(scene_detached_plane_extent(mesh,path->position[step],path->basis[step],hit.point,normal)<=1e-5f &&
            !(step && hit.fraction==0 && scene_detached_plane_extent(mesh,path->position[step-1],path->basis[step-1],hit.point,normal)>1e-5f))continue;
        hit.fraction=step*.25f+hit.fraction*.25f;if(hit.fraction>=*limit)continue;
        *limit=hit.fraction;*best=hit;*found=1;
    }
    return RF_OK;
}
static int scene_detached_edge_sweep(const rf_geomod_mesh_view *mesh,const scene_fragment_path *path,
    const float a[3],const float b[3],const float normal[3],float limit,rf_collision_ray_hit *out,uint32_t *matched)
{
    float lo[3],hi[3];rf_collision_ray_hit best;uint32_t k,e,f,v,found=0;int status;
    for(k=0;k<3;k++){lo[k]=fminf(a[k],b[k]);hi[k]=fmaxf(a[k],b[k]);}
    if(scene_fragment_edges.complete && (!scene_fragment_edges.count ||
        !scene_detached_box_overlap(lo,hi,scene_fragment_edges.minimum,scene_fragment_edges.maximum))){*matched=0;return RF_OK;}
    if(scene_fragment_edges.complete)for(e=0;e<scene_fragment_edges.count;e++) {
        status=scene_fragment_edge_pair(mesh,path,scene_fragment_edges.edges+e,a,b,normal,lo,hi,&limit,&best,&found);if(status)return status;
    }
    else for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;
        if(face->first>mesh->vertex_count || face->count>mesh->vertex_count-face->first)return RF_FORMAT;
        for(v=0;v<face->count;v++) {
            scene_fragment_edge edge;scene_fragment_edge_prepare(mesh,path,face->first+v,face->first+(v+1)%face->count,&edge);
            status=scene_fragment_edge_pair(mesh,path,&edge,a,b,normal,lo,hi,&limit,&best,&found);if(status)return status;
        }
    }
    if(found)*out=best;*matched=found;return RF_OK;
}
