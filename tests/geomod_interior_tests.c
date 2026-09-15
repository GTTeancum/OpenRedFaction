#include "rf/geomod.h"
#include "../tools/pc_raster.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"interior line %d\n",__LINE__);return 1;}} while(0)
static void box(const float lo[3],const float hi[3],float planes[6][4],rf_geomod_vertex faces[6][4])
{
    unsigned axis,side,j;const int u[4]={-1,1,1,-1},v[4]={-1,-1,1,1};
    memset(planes,0,6*4*sizeof(float));memset(faces,0,6*4*sizeof(rf_geomod_vertex));
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++) {
        unsigned f=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;
        planes[f][axis]=side?1:-1;planes[f][3]=side?-hi[axis]:lo[axis];
        for(j=0;j<4;j++) {
            unsigned k=side?j:3-j;
            faces[f][j].position[axis]=side?hi[axis]:lo[axis];
            faces[f][j].position[a]=u[k]>0?hi[a]:lo[a];
            faces[f][j].position[b]=v[k]>0?hi[b]:lo[b];
            faces[f][j].uv[0]=(float)u[k];faces[f][j].uv[1]=(float)v[k];
        }
    }
}
static double volume(const rf_geomod_vertex *p,unsigned n)
{
    unsigned i;double result=0;const float *a=p[0].position;
    for(i=1;i+1<n;i++) {
        const float *b=p[i].position,*c=p[i+1].position;
        result+=((double)a[0]*(b[1]*c[2]-b[2]*c[1])+(double)a[1]*(b[2]*c[0]-b[0]*c[2])+(double)a[2]*(b[0]*c[1]-b[1]*c[0]))/6;
    }
    return result;
}
/* Host snapshot inspection may measure candidates larger than runtime owners. */
#define SNAPSHOT_VERTICES 8192
#define SNAPSHOT_FACES 1536
static rf_geomod_vertex surface[SNAPSHOT_VERTICES];
static rf_geomod_fragment polygons[SNAPSHOT_FACES];
static unsigned surface_count,polygon_count;
static unsigned report_closure;
static void trace_intersection(void *context,const float plane[4],const float a[3],const float b[3],const float result[3])
{
    FILE *file=context;float row[13];
    memcpy(row,plane,16);memcpy(row+4,a,12);memcpy(row+7,b,12);memcpy(row+10,result,12);
    if(fwrite(row,sizeof(row),1,file)!=1){fprintf(stderr,"intersection trace write failed\n");exit(1);}
}
static void trace_compaction(void *context,const rf_geomod_mesh_view *mesh,const uint16_t *planes,const uint16_t *edges)
{
    FILE *file=context;uint32_t f,e;
    fprintf(file,"face,corner,face_support,edge_support,x,y,z\n");
    for(f=0;f<mesh->face_count;f++)for(e=0;e<mesh->faces[f].count;e++) {
        uint32_t i=mesh->faces[f].first+e;const float *p=mesh->vertices[i].position;
        fprintf(file,"%u,%u,%u,%u,%.9g,%.9g,%.9g\n",f,e,planes[f],edges[i],p[0],p[1],p[2]);
    }
    if(ferror(file)){fprintf(stderr,"compaction trace write failed\n");exit(1);}
}
static int check_clipping_provenance(const rf_geomod_multi_work *work)
{
    /* Last cutter face starts in bank0. Check real retained edge
     * IDs after seed clipping/history copies and any reversal. */
    const rf_geomod_fragment *fragment=work->fragments[0];
    uint32_t edge,end;
    CHECK(fragment->count>=3 && fragment->count<=64);
    for(edge=0;edge<fragment->count;edge++) {
        uint16_t id=work->edges[0][fragment->first+edge];const float *support;
        if(id<32){CHECK(id<6);support=work->source_planes[id];}
        else {
            uint32_t cutter=(id-32)/128,local=(id-32)%128;
            CHECK(cutter<RF_GEOMOD_CUT_LIMIT && local/4<work->star_count[cutter]);
            support=work->star_planes[cutter][local/4][local%4];
        }
        for(end=0;end<2;end++) {
            const float *point=work->vertices[0][fragment->first+(edge+end)%fragment->count].position;
            CHECK(fabs((double)support[0]*point[0]+(double)support[1]*point[1]+(double)support[2]*point[2]+support[3])<1e-4);
        }
    }
    return 0;
}
static int check_compact_provenance(const rf_geomod_multi_work *work,const rf_geomod_mesh_view *pending)
{
    uint32_t face,edge,end,checked=0;
    for(face=0;face<pending->face_count;face++) {
        const rf_geomod_face *f=pending->faces+face;
        for(edge=0;edge<f->count;edge++) {
            uint16_t id=work->compact_edges[f->first+edge];const float *support;
            if(id<32){CHECK(id<6);support=work->source_planes[id];}
            else {
                uint32_t cutter=(id-32)/128,local=(id-32)%128;
                CHECK(cutter<RF_GEOMOD_CUT_LIMIT && local/4<work->star_count[cutter]);
                support=work->star_planes[cutter][local/4][local%4];
            }
            for(end=0;end<2;end++) {
                const float *p=pending->vertices[f->first+(edge+end)%f->count].position;
                CHECK(fabs((double)support[0]*p[0]+(double)support[1]*p[1]+(double)support[2]*p[2]+support[3])<1e-4);
            }
            ++checked;
        }
        if(f->source_face!=UINT32_MAX)CHECK(work->compact_planes[face]==f->source_face);
    }
    CHECK(checked==pending->vertex_count);
    printf("COMPACT_SUPPORTS edges%u\n",checked);
    return 0;
}
static int provenance_result;
static void observe_clipping_provenance(void *context,const rf_geomod_mesh_view *mesh,
    const uint16_t *planes,const uint16_t *edges)
{
    (void)mesh;(void)planes;(void)edges;
    provenance_result=check_clipping_provenance(context);
    if(!provenance_result)provenance_result=check_compact_provenance(context,mesh);
}
static int record(const rf_geomod_vertex *v,unsigned n)
{
    if(surface_count+n>SNAPSHOT_VERTICES || polygon_count==SNAPSHOT_FACES)return 0;
    polygons[polygon_count].first=surface_count;polygons[polygon_count++].count=n;
    memcpy(surface+surface_count,v,n*sizeof(*v));surface_count+=n;return 1;
}
/* Geometric edge coverage, including T-junction subdivisions. This does not
 * weld the production output or claim shared topological edge indices. */
static int closed(void)
{
    unsigned p,e,v,i,j,q,f,failures=0;
    for(p=0;p<polygon_count;p++)for(e=0;e<polygons[p].count;e++) {
        const float *a=surface[polygons[p].first+e].position;
        const float *b=surface[polygons[p].first+(e+1)%polygons[p].count].position;
        double d[3],len=0,t[SNAPSHOT_VERTICES+2]={0,1};unsigned n=2;
        for(i=0;i<3;i++){d[i]=(double)b[i]-a[i];len+=d[i]*d[i];}
        if(len<1e-16)return 0;
        for(v=0;v<surface_count;v++) {
            double along=0,error=0;
            for(i=0;i<3;i++)along+=((double)surface[v].position[i]-a[i])*d[i];
            along/=len;
            for(i=0;i<3;i++){double x=(double)surface[v].position[i]-a[i]-along*d[i];error+=x*x;}
            if(along>1e-6/sqrt(len) && along<1-1e-6/sqrt(len) && error<1e-12)t[n++]=along;
        }
        for(i=1;i<n;i++){double value=t[i];j=i;while(j && t[j-1]>value){t[j]=t[j-1];j--;}t[j]=value;}
        for(j=1;j<n;j++)if((t[j]-t[j-1])*sqrt(len)>1e-6) {
            double mid[3],match_error[8],match_along[8],nearest=1e30;unsigned nearest_face=0,nearest_edge=0;unsigned match_face[8],match_edge[8];int matches=0,balance=0;
            for(i=0;i<3;i++)mid[i]=a[i]+d[i]*(t[j]+t[j-1])*.5;
            for(q=0;q<polygon_count;q++)for(f=0;f<polygons[q].count;f++) {
                const float *x=surface[polygons[q].first+f].position;
                const float *y=surface[polygons[q].first+(f+1)%polygons[q].count].position;
                double edge[3],size=0,along=0,error=0,dot=0;
                for(i=0;i<3;i++){edge[i]=(double)y[i]-x[i];size+=edge[i]*edge[i];along+=(mid[i]-x[i])*edge[i];dot+=d[i]*edge[i];}
                if(size<1e-16)return 0;
                along/=size;
                for(i=0;i<3;i++){double z=mid[i]-x[i]-along*edge[i];error+=z*z;}
                if(dot<0 && along>1e-8 && along<1-1e-8 && fabs(dot*dot-len*size)<1e-8*len*size && error<nearest) {nearest=error;nearest_face=q;nearest_edge=f;}
                if(along>1e-8 && along<1-1e-8 && error<1e-12 && fabs(dot*dot-len*size)<1e-8*len*size) {
                    if(matches<8){match_face[matches]=q;match_edge[matches]=f;match_error[matches]=sqrt(error);match_along[matches]=along;}
                    matches++;balance+=dot>0?1:-1;
                }
            }
            if(matches!=2 || balance) {
                if(report_closure)printf("CLOSURE_DIAGNOSTIC face%u edge%u length%.9g matches%d balance%d start %.9g %.9g %.9g end %.9g %.9g %.9g\n",
                    p,e,sqrt(len),matches,balance,a[0],a[1],a[2],b[0],b[1],b[2]);
                if(report_closure) {
                    printf("CLOSURE_INTERVAL width %.12g midpoint %.12g %.12g %.12g\n",(t[j]-t[j-1])*sqrt(len),mid[0],mid[1],mid[2]);
                    if(report_closure>=2)printf("CLOSURE_NEAREST face%u edge%u distance %.12g found %u\n",nearest_face,nearest_edge,sqrt(nearest),nearest<1e30);
                    for(i=0;i<(unsigned)matches && i<8;i++) {
                        const rf_geomod_fragment *poly=polygons+match_face[i];
                        const float *x=surface[poly->first+match_edge[i]].position;
                        const float *y=surface[poly->first+(match_edge[i]+1)%poly->count].position;
                        printf("CLOSURE_MATCH face%u edge%u distance %.12g along %.12g start %.9g %.9g %.9g end %.9g %.9g %.9g\n",
                            match_face[i],match_edge[i],match_error[i],match_along[i],x[0],x[1],x[2],y[0],y[1],y[2]);
                    }
                }
                ++failures;if(report_closure<2)return 0;
            }
        }
    }
    return failures==0;
}
static int collision_clearance(const rf_geomod_mesh_view *mesh,int tunnel)
{
    static float positions[2048][3];static rf_collision_face faces[128];
    static rf_collision_face_filter filters[128];
    static float saved_positions[2048][3];static rf_collision_face saved_faces[128];
    rf_collision_tree tree={0};rf_collision_tree_hit ray;rf_collision_sweep_tree_hit sweep;
    float start[3]={0,0,-4},delta[3]={0,0,8};uint32_t matched;
    CHECK(!rf_geomod_collision_faces(mesh,filters,positions,2048,faces,128));
    memcpy(saved_positions,positions,sizeof(positions));memcpy(saved_faces,faces,sizeof(faces));
    CHECK(rf_geomod_collision_faces(mesh,filters,positions,mesh->vertex_count-1,faces,128)==RF_RANGE);
    filters[mesh->face_count-1].owner_present=2;
    CHECK(rf_geomod_collision_faces(mesh,filters,positions,2048,faces,128)==RF_RANGE);
    filters[mesh->face_count-1].owner_present=0;
    CHECK(!memcmp(saved_positions,positions,sizeof(positions)) && !memcmp(saved_faces,faces,sizeof(faces)));
    {
        static rf_preview_vertex vertices[8192],saved[8192];rf_preview_mesh draw={vertices,0,0};
        rf_level camera={0};rf_pc_raster raster={0};rf_materials materials={0};rf_lightmaps maps={0};
        const char *capture=getenv("RF_GEOMOD_CAPTURE_DIR");unsigned i,j,interiors=0;
        camera.player_position[2]=-6;
        for(i=0;i<3;i++)camera.player_orientation[i][i]=1;
        CHECK(!rf_preview_geomod(&draw,sizeof(vertices),mesh,faces,400,&camera));
        CHECK(draw.count && draw.bytes==draw.count*sizeof(*vertices));
        for(i=0;i<draw.count;i++) {
            CHECK(vertices[i].lightmap==UINT32_MAX && vertices[i].texture[2]>0);
            for(j=0;j<3;j++)CHECK(isfinite(vertices[i].position[j]) && isfinite(vertices[i].texture[j]));
            if(vertices[i].material>=200)interiors++;
        }
        CHECK(tunnel?interiors>0:interiors==0);
        memcpy(saved,vertices,sizeof(saved));
        CHECK(rf_preview_geomod(&draw,draw.bytes-sizeof(*vertices),mesh,faces,400,&camera)==RF_RANGE);
        CHECK(!memcmp(saved,vertices,sizeof(saved)));
        CHECK(!rf_pc_raster_open(&raster,1));
        CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
        CHECK(tunnel?raster.depth[240*640+320]==16777216:raster.depth[240*640+320]<16777216);
        CHECK(raster.depth[240*640+450]<16777216); /* Original surface survives. */
        CHECK(raster.depth[240*640+380]<16777216); /* Interior sidewall after cut. */
        if(capture) {
            char path[1024];CHECK(snprintf(path,sizeof(path),"%s/%s.ppm",capture,tunnel?"cut":"original")<(int)sizeof(path));
            CHECK(!rf_pc_raster_save(&raster,path));
        }
        rf_pc_raster_close(&raster);
    }
    CHECK(!rf_collision_tree_open(faces,mesh->face_count,1024*1024,&tree));
    {
        rf_collision_tree reused={0},guard={0},saved=guard;
        uint32_t bytes=mesh->face_count*(sizeof(rf_collision_face)+5);void *scratch=malloc(bytes);
        CHECK(scratch);
        CHECK(rf_collision_tree_open_scratch(faces,mesh->face_count,tree.allocated_bytes,&guard,scratch,bytes-1)==RF_RANGE);
        CHECK(!memcmp(&guard,&saved,sizeof(guard)));
        CHECK(!rf_collision_tree_open_scratch(faces,mesh->face_count,tree.allocated_bytes,&reused,scratch,bytes));
        CHECK(reused.node_count==tree.node_count && reused.face_count==tree.face_count);
        CHECK(reused.allocated_bytes==tree.allocated_bytes && reused.peak_bytes+bytes==tree.peak_bytes);
        CHECK(!memcmp(reused.nodes,tree.nodes,tree.node_count*sizeof(*tree.nodes)));
        CHECK(!memcmp(reused.faces,tree.faces,tree.face_count*sizeof(*tree.faces)));
        CHECK(!memcmp(reused.source_indices,tree.source_indices,tree.face_count*sizeof(*tree.source_indices)));
        free(scratch);rf_collision_tree_close(&tree);tree=reused;
    }

    CHECK(!rf_collision_thin_tree(tree.nodes,tree.node_count,tree.faces,tree.face_count,0,
        start,delta,1,tree.stack,tree.node_capacity,&ray,&matched));
    CHECK(matched==(tunnel?0u:1u));
    if(!tunnel)CHECK(fabs(ray.hit.fraction-.25f)<1e-5 && ray.hit.normal[2]<-.99f);
    CHECK(!rf_collision_sweep_tree(tree.nodes,tree.node_count,tree.faces,tree.face_count,0,
        start,delta,delta,.5f,1,tree.stack,tree.node_capacity,&sweep,&matched));
    CHECK(matched==(tunnel?0u:1u));
    if(!tunnel)CHECK(fabs(sweep.hit.fraction-.1875f)<1e-5);
    if(tunnel) {
        CHECK(!rf_collision_sweep_tree(tree.nodes,tree.node_count,tree.faces,tree.face_count,0,
            start,delta,delta,1.1f,1,tree.stack,tree.node_capacity,&sweep,&matched));
        CHECK(matched==1); /* Body larger than the hole still hits its rim. */
        start[2]=0;delta[0]=3;delta[2]=0;
        CHECK(!rf_collision_thin_tree(tree.nodes,tree.node_count,tree.faces,tree.face_count,0,
            start,delta,1,tree.stack,tree.node_capacity,&ray,&matched));
        CHECK(matched==1 && fabs(ray.hit.fraction-1.f/3)<1e-5 && ray.hit.normal[0]<-.99f);
        CHECK(mesh->faces[tree.source_indices[ray.face_index]].source_face==UINT32_MAX);
    }
    rf_collision_tree_close(&tree);return 0;
}
/* Independent double-precision fan intersection for the axis-aligned
 * junction probes. No production polygon predicate or tree is used. */
static unsigned junction_reference(const rf_geomod_mesh_view *mesh,const float probe[3])
{
    unsigned f,k;
    for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;
        const float *a=mesh->vertices[face->first].position;
        for(k=1;k+1<face->count;k++) {
            const float *b=mesh->vertices[face->first+k].position;
            const float *c=mesh->vertices[face->first+k+1].position;
            double by=(double)b[1]-a[1],bz=(double)b[2]-a[2];
            double cy=(double)c[1]-a[1],cz=(double)c[2]-a[2];
            double py=(double)probe[1]-a[1],pz=(double)probe[2]-a[2];
            double det=by*cz-bz*cy,u,v,x;
            if(det==0)continue;
            u=(py*cz-pz*cy)/det;v=(by*pz-bz*py)/det;
            if(u<0 || v<0 || u+v>1)continue;
            x=a[0]+u*((double)b[0]-a[0])+v*((double)c[0]-a[0]);
            if(x<=probe[0] && x>=probe[0]-15)return f+1;
        }
    }
    return 0;
}
/* Independent plane/barycentric reference, distinct from the production
 * ray-space edge projection. Returns the nearest front-facing triangle. */
static double terrain_reference(const rf_geomod_mesh_view *mesh,const float start[3],const float delta[3])
{
    unsigned f,k,j;double best=2;
    for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;const float *a=mesh->vertices[face->first].position;
        for(k=1;k+1<face->count;k++) {
            const float *b=mesh->vertices[face->first+k].position,*c=mesh->vertices[face->first+k+1].position;
            double e[3],g[3],n[3],p[3],den=0,num=0,ee=0,eg=0,gg=0,ep=0,gp=0,t,u,v,det;
            for(j=0;j<3;j++){e[j]=(double)b[j]-a[j];g[j]=(double)c[j]-a[j];}
            for(j=0;j<3;j++) {
                n[j]=e[(j+1)%3]*g[(j+2)%3]-e[(j+2)%3]*g[(j+1)%3];
                den+=n[j]*delta[j];num+=n[j]*((double)a[j]-start[j]);
            }
            if(den>=0)continue;t=num/den;if(t<0 || t>1 || t>best)continue;
            for(j=0;j<3;j++) {
                p[j]=((double)start[j]-a[j])+t*delta[j];
                ee+=e[j]*e[j];eg+=e[j]*g[j];gg+=g[j]*g[j];ep+=e[j]*p[j];gp+=g[j]*p[j];
            }
            det=ee*gg-eg*eg;if(det<=0)continue;
            u=(gg*ep-eg*gp)/det;v=(ee*gp-eg*ep)/det;
            if(u>=0 && v>=0 && u+v<=1)best=t;
        }
    }
    return best;
}
static int terrain_ray_coverage(const rf_geomod_terrain_view *live,unsigned cut)
{
    const float start[3]={0,-10,12},extent[3]={32,24,40};unsigned axis,side,u,v,j,probes=0;
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++)for(u=0;u<17;u++)for(v=0;v<17;v++) {
        float end[3]={0},delta[3];double expected;rf_collision_tree_hit hit;unsigned matched;
        end[axis]=(side?1:-1)*extent[axis];
        end[(axis+1)%3]=extent[(axis+1)%3]*((u+.37f)/17-.5f);
        end[(axis+2)%3]=extent[(axis+2)%3]*((v+.61f)/17-.5f);
        for(j=0;j<3;j++)delta[j]=end[j]-start[j];
        expected=terrain_reference(&live->mesh,start,delta);CHECK(expected<=1);
        CHECK(!rf_collision_thin_tree(live->tree->nodes,live->tree->node_count,live->tree->faces,
            live->tree->face_count,0,start,delta,1,live->tree->stack,live->tree->node_capacity,&hit,&matched));
        if(!matched || fabs(hit.hit.fraction-expected)>1e-5)
            printf("COVERAGE_FAILURE cut %u axis %u side %u grid %u %u expected %.12g hit %u fraction %.9g\n",cut,axis,side,u,v,expected,matched,matched?hit.hit.fraction:0);
        CHECK(matched && fabs(hit.hit.fraction-expected)<=1e-5);
        CHECK(!rf_collision_thin_tree(live->tree->nodes,live->tree->node_count,live->tree->faces,
            live->tree->face_count,0,start,delta,(float)(expected*.5),live->tree->stack,live->tree->node_capacity,&hit,&matched));
        CHECK(!matched);probes++;
    }
    printf("TERRAIN_COVERAGE cut %u nearest %u short_segment %u\n",cut,probes,probes);return 0;
}
static int light_bake_check(void)
{
    rf_geomod_vertex vertices[4]={{{-2,-2,0},{0}},{{2,-2,0},{0}},{{2,2,0},{0}},{{-2,2,0},{0}}};
    rf_geomod_light_grid grid;rf_geomod_light_bake lighting={0};float plane[4]={0,0,1,0};
    unsigned char pixels[576],saved[576],shadow[576];uint32_t stats[3]={99,99,99},mode=0,x,y;unsigned short word;
    rf_vfx_light_definition definition={0};rf_vfx_light_candidate light;
    CHECK(!rf_geomod_light_grid_open(vertices,4,plane,.5f,&grid) && grid.width==16 && grid.height==16);
    lighting.ambient[0]=.25f;lighting.ambient[1]=.5f;lighting.ambient[2]=.75f;
    memset(pixels,0xa5,sizeof(pixels));
    CHECK(!rf_geomod_light_grid_bake(&grid,vertices,4,&lighting,pixels,36,sizeof(pixels),stats));
    CHECK(stats[0]==256 && !stats[1] && !stats[2]);
    for(y=0;y<16;y++) {
        for(x=0;x<16;x++){memcpy(&word,pixels+y*36+x*2,2);CHECK(word==0x9df7);}
        for(x=32;x<36;x++)CHECK(pixels[y*36+x]==0xa5);
    }
    memset(shadow,0xa5,sizeof(shadow));
    for(x=0;x<256;x+=13) {
        unsigned count=256-x<13?256-x:13;
        CHECK(!rf_geomod_light_grid_bake_range(&grid,vertices,4,&lighting,shadow,36,sizeof(shadow),x,count,stats));
        CHECK(stats[0]==count);
    }
    CHECK(!memcmp(shadow,pixels,sizeof(pixels)));
    CHECK(rf_geomod_light_grid_bake_range(&grid,vertices,4,&lighting,shadow,36,sizeof(shadow),255,2,stats)==RF_RANGE);
    CHECK(!memcmp(shadow,pixels,sizeof(pixels)));
    memcpy(saved,pixels,sizeof(saved));
    CHECK(rf_geomod_light_grid_bake(&grid,vertices,4,&lighting,pixels,36,571,stats)==RF_RANGE);
    CHECK(!memcmp(saved,pixels,sizeof(saved)));
    definition.type=2;definition.position[2]=2;definition.radius=3;definition.intensity=1;
    definition.color[0]=definition.color[1]=definition.color[2]=1;
    CHECK(!rf_vfx_light_create(&definition,&light));lighting.sources=&light.source;lighting.shadow_modes=&mode;lighting.count=1;
    lighting.ambient[0]=lighting.ambient[1]=lighting.ambient[2]=.1f;
    CHECK(!rf_geomod_light_grid_bake(&grid,vertices,4,&lighting,pixels,36,sizeof(pixels),stats));
    {unsigned short center,corner;memcpy(&center,pixels+8*36+8*2,2);memcpy(&corner,pixels+36+2,2);CHECK(center>corner);}
    {
        rf_geomod_vertex blocker[4]={{{-.5f,-2,1},{0}},{{.5f,-2,1},{0}},{{.5f,2,1},{0}},{{-.5f,2,1},{0}}};
        rf_geomod_face face={0,4,0,UINT32_MAX};rf_geomod_mesh_view mesh={blocker,&face,4,1,0};
        rf_collision_face_filter filter={0};rf_collision_face bound;float positions[4][3];
        rf_collision_tree tree={0};rf_geomod_terrain_view terrain={0};unsigned short center,unshadowed;unsigned unchanged=0;
        CHECK(!rf_geomod_collision_faces(&mesh,&filter,positions,4,&bound,1));
        CHECK(!rf_collision_tree_open(&bound,1,65536,&tree));terrain.tree=&tree;lighting.terrain=&terrain;mode=1;
        CHECK(!rf_geomod_light_grid_bake(&grid,vertices,4,&lighting,shadow,36,sizeof(shadow),stats));
        CHECK(stats[0]==256 && stats[1]==256 && stats[2]>0 && stats[2]<256);
        memcpy(&center,shadow+8*36+8*2,2);memcpy(&unshadowed,pixels+8*36+8*2,2);CHECK(center<unshadowed);
        for(y=0;y<16;y++)for(x=0;x<16;x++)if(!memcmp(shadow+y*36+x*2,pixels+y*36+x*2,2))unchanged++;
        CHECK(unchanged>0);
        {
            rf_geomod_face receiver={0,4,0,UINT32_MAX};rf_geomod_mesh_view surface={vertices,&receiver,4,1,0};
            rf_collision_face receiver_bound;float receiver_positions[4][3];rf_preview_surface_lightmap binding={0};
            rf_preview_vertex projected[64],saved_vertices[64];rf_preview_mesh draw={projected,0,0};rf_level camera={0};
            rf_pc_raster raster={0};rf_image atlas={0};rf_lightmaps maps={0};rf_material material={0};rf_materials materials={0};
            unsigned char white[4]={255,255,255,255};unsigned before,after,index=3*(240*640+320);
            CHECK(!rf_geomod_collision_faces(&surface,&filter,receiver_positions,4,&receiver_bound,1));
            camera.player_position[2]=5;camera.player_orientation[0][0]=camera.player_orientation[1][1]=1;camera.player_orientation[2][2]=-1;
            atlas.width=32;atlas.height=16;atlas.bytes=32*16*2;atlas.source_format=5;
            CHECK(!rf_image_allocate_pixels(&atlas));maps.images=&atlas;maps.count=1;
            for(y=0;y<16;y++)for(x=0;x<32;x++) {
                unsigned short value=0xfc00; /* Adjacent atlas region must not leak into this face. */
                if(x>=16)memcpy(&value,pixels+y*36+(x-16)*2,2);
                memcpy(rf_image_pixel(&atlas,x,y),&value,2);
            }
            material.image.width=material.image.height=1;material.image.bytes=4;material.image.rgba=white;
            materials.items=&material;materials.count=1;
            binding.image=0;binding.projection.axes[0]=0;binding.projection.axes[1]=1;
            binding.projection.scale[0]=13.f/(4*32);binding.projection.offset[0]=24.f/32;
            binding.projection.scale[1]=13.f/(4*16);binding.projection.offset[1]=8.f/16;
            CHECK(!rf_preview_geomod_lightmapped(&draw,sizeof(projected),&surface,&receiver_bound,1,&camera,NULL,NULL,&binding));
            CHECK(draw.count==6 && projected[0].lightmap==0);
            for(x=0;x<draw.count;x++)CHECK(projected[x].lightmap_texture[0]/projected[x].lightmap_texture[2]>.5f);
            CHECK(!rf_pc_raster_open(&raster,1));
            CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));before=raster.rgb[index];
            CHECK(before==raster.rgb[index+1] && before==raster.rgb[index+2]);
            for(y=0;y<16;y++)for(x=0;x<16;x++)memcpy(rf_image_pixel(&atlas,x+16,y),shadow+y*36+x*2,2);
            CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));after=raster.rgb[index];
            CHECK(after<before && after==raster.rgb[index+1] && after==raster.rgb[index+2]);
            memcpy(saved_vertices,projected,sizeof(projected));binding.projection.scale[0]=NAN;
            CHECK(rf_preview_geomod_lightmapped(&draw,sizeof(projected),&surface,&receiver_bound,1,&camera,NULL,NULL,&binding)==RF_FORMAT);
            CHECK(draw.count==6 && !memcmp(saved_vertices,projected,sizeof(projected)));
            printf("CRATER_MAPPED_PIXELS center_unshadowed %u shadowed %u\n",before,after);
            rf_pc_raster_close(&raster);rf_image_close(&atlas);
        }
        memcpy(saved,shadow,sizeof(saved));light.source.type=1;
        CHECK(rf_geomod_light_grid_bake(&grid,vertices,4,&lighting,shadow,36,sizeof(shadow),stats)==RF_NOT_FOUND);
        CHECK(!memcmp(saved,shadow,sizeof(saved)));
        light.source.type=2;lighting.terrain=NULL;
        CHECK(rf_geomod_light_grid_bake(&grid,vertices,4,&lighting,shadow,36,sizeof(shadow),stats)==RF_RANGE);
        CHECK(!memcmp(saved,shadow,sizeof(saved)));rf_collision_tree_close(&tree);
    }
    return 0;
}
static int light_grid_check(void)
{
    unsigned axis,x,y;rf_geomod_vertex vertices[3]={0};
    for(axis=0;axis<3;axis++) {
        unsigned u=(axis+1)%3,v=(axis+2)%3;float plane[4]={0},point[3],uv[2];rf_geomod_light_grid grid,saved;
        plane[axis]=1;plane[3]=-2;
        memset(vertices,0,sizeof(vertices));
        vertices[0].position[u]=-1;vertices[0].position[v]=-1;
        vertices[1].position[u]=1;vertices[1].position[v]=-1;
        vertices[2].position[u]=-1;vertices[2].position[v]=1;
        for(x=0;x<3;x++)vertices[x].position[axis]=2;
        CHECK(!rf_geomod_light_grid_open(vertices,3,plane,.5f,&grid));
        CHECK(grid.width==8 && grid.height==8 && grid.axis==axis);
        for(y=0;y<8;y++)for(x=0;x<8;x++) {
            CHECK(!rf_geomod_light_grid_sample(&grid,vertices,3,x,y,point));
            CHECK(point[axis]==2 && point[u]>=-1 && point[v]>=-1 && point[u]+point[v]<=1e-6);
        }
        CHECK(!rf_geomod_light_grid_uv(&grid,vertices[0].position,uv));
        CHECK(uv[0]==1.5f/8 && uv[1]==1.5f/8);
        CHECK(!rf_geomod_light_grid_sample(&grid,vertices,3,1,1,point));
        CHECK(!memcmp(point,vertices[0].position,12));
        /* A bright interior sample cannot be reconstructed from three dark
         * corner samples; the grid includes interior surface positions. */
        CHECK(!rf_geomod_light_grid_sample(&grid,vertices,3,3,3,point));
        CHECK(point[u]>-.5f && point[v]>-.5f && point[u]<0 && point[v]<0);
        saved=grid;CHECK(rf_geomod_light_grid_open(vertices,3,plane,.001f,&grid)==RF_RANGE);
        CHECK(!memcmp(&saved,&grid,sizeof(grid)));
        CHECK(rf_geomod_light_grid_open(vertices,3,plane,NAN,&grid)==RF_FORMAT);
        point[0]=point[1]=point[2]=123;
        CHECK(rf_geomod_light_grid_sample(&grid,vertices,3,8,0,point)==RF_RANGE && point[0]==123);
    }
    return 0;
}
static int partition_contract(void)
{
    /* First original-template crater: support insertion made this boundary
     * fail convex collision binding. Keep its float coordinates verbatim. */
    rf_geomod_vertex source_vertices[10]={
        {{-16.0f,-7.36763811f,1.4182359f},{0.25f,-0.5f}},
        {{-16.0f,-7.36746788f,1.08706689f},{1.25f,-1.5f}},
        {{-16.0f,-7.35663891f,-20.0f},{2.25f,-2.5f}},
        {{-16.0f,12.0f,-20.0f},{3.25f,-3.5f}},
        {{-16.0f,12.0f,-17.5771255f},{4.25f,-4.5f}},
        {{-16.0f,12.0f,5.15646219f},{5.25f,-5.5f}},
        {{-16.0f,12.0f,7.87391949f},{6.25f,-6.5f}},
        {{-16.0f,-4.48685169f,4.97079515f},{7.25f,-7.5f}},
        {{-16.0f,-6.92312765f,2.45680904f},{8.25f,-8.5f}},
        {{-16.0f,-7.36793566f,1.99781287f},{9.25f,-9.5f}}
    },saved_vertices[10],vertices[64];
    rf_geomod_face source_face={0,10,77,19},faces[16];
    rf_geomod_mesh_view source={source_vertices,&source_face,10,1,42},out={0},saved;
    rf_collision_face_filter filters[16]={{0}};rf_collision_face bound[16];
    float positions[64][3];unsigned i,j;
    memcpy(saved_vertices,source_vertices,sizeof(source_vertices));
    CHECK(rf_geomod_collision_faces(&source,filters,positions,64,bound,16)==RF_FORMAT);
    CHECK(!rf_geomod_partition_mesh(&source,vertices,64,faces,16,&out));
    CHECK(out.face_count==2 && out.vertex_count==12 && out.generation==42);
    CHECK(!rf_geomod_collision_faces(&out,filters,positions,64,bound,16));
    for(i=0;i<out.face_count;i++)CHECK(faces[i].material==77 && faces[i].source_face==19);
    for(i=0;i<out.vertex_count;i++) {
        for(j=0;j<10;j++)if(!memcmp(vertices+i,source_vertices+j,sizeof(*vertices)))break;
        CHECK(j<10); /* Every boundary UV survives exactly, including duplicates. */
    }
    for(i=0;i<10;i++) {
        for(j=0;j<out.vertex_count;j++)if(!memcmp(source_vertices+i,vertices+j,sizeof(*vertices)))break;
        CHECK(j<out.vertex_count);
    }
    saved=out;
    CHECK(rf_geomod_partition_mesh(&source,vertices,64,faces,1,&out)==RF_RANGE);
    CHECK(!memcmp(&saved,&out,sizeof(out)));
    CHECK(rf_geomod_partition_mesh(&source,vertices,10,faces,16,&out)==RF_RANGE);
    CHECK(!memcmp(&saved,&out,sizeof(out)));
    source_face.count=65;
    CHECK(rf_geomod_partition_mesh(&source,vertices,64,faces,16,&out)==RF_FORMAT);
    CHECK(!memcmp(&saved,&out,sizeof(out)));
    CHECK(!memcmp(saved_vertices,source_vertices,sizeof(source_vertices)));
    {
        const float xy[8][2]={{2,0},{.5f,.5f},{0,2},{-.5f,.5f},{-2,0},{-.5f,-.5f},{0,-2},{.5f,-.5f}};
        rf_geomod_vertex star[8];rf_geomod_face face={0,8,31,UINT32_MAX};
        rf_geomod_mesh_view mesh={star,&face,8,1,7};
        for(i=0;i<8;i++)star[i]=(rf_geomod_vertex){{xy[i][0],xy[i][1],0},{3+xy[i][0]*.25f,5+xy[i][1]*.25f}};
        CHECK(!rf_geomod_partition_mesh(&mesh,vertices,64,faces,16,&out));
        CHECK(out.face_count==8 && out.vertex_count==24);
        CHECK(!rf_geomod_collision_faces(&out,filters,positions,64,bound,16));
        for(i=0;i<8;i++) {
            const rf_geomod_vertex *center=vertices+faces[i].first;
            CHECK(faces[i].material==31 && faces[i].source_face==UINT32_MAX);
            CHECK(center->position[0]==0 && center->position[1]==0 && center->position[2]==0);
            CHECK(center->uv[0]==3 && center->uv[1]==5);
        }
    }
    return 0;
}
int main(int argc,char **argv)
{
    CHECK(!partition_contract());
    if(argc==3 && !strcmp(argv[1],"--mesh")) {
        FILE *file=fopen(argv[2],"rb");char magic[4];uint32_t counts[2],i,packed=0;
        static rf_geomod_face input_faces[SNAPSHOT_FACES];int result;CHECK(file);
        CHECK(fread(magic,1,4,file)==4 && !memcmp(magic,"RGM1",4));
        CHECK(fread(counts,4,2,file)==2 && counts[0]>0 && counts[0]<=SNAPSHOT_VERTICES && counts[1]>0 && counts[1]<=SNAPSHOT_FACES);
        CHECK(fread(surface,sizeof(*surface),counts[0],file)==counts[0]);
        CHECK(fread(input_faces,sizeof(*input_faces),counts[1],file)==counts[1] && fgetc(file)==EOF);CHECK(!fclose(file));
        surface_count=counts[0];polygon_count=counts[1];
        for(i=0;i<polygon_count;i++) {
            CHECK(input_faces[i].first==packed && input_faces[i].count>=3 && input_faces[i].count<=64 && input_faces[i].count<=surface_count-packed);
            polygons[i]=(rf_geomod_fragment){input_faces[i].first,input_faces[i].count};packed+=input_faces[i].count;
        }
        CHECK(packed==surface_count);report_closure=2;result=closed();
        printf("SNAPSHOT_CLOSURE %d vertices%u faces%u\n",result,surface_count,polygon_count);return result?0:1;
    }
    {
        rf_lightmap_sample_lighting lighting={0};rf_vfx_light_source light={0};
        rf_random_state random={0x12345678};unsigned char rgb[48],base[40],lit[40],saved[40];unsigned x,y;
        lighting.width=lighting.height=4;lighting.sample.image_width=lighting.sample.image_height=8;
        lighting.sample.x=lighting.sample.y=1;lighting.sample.scale[0]=lighting.sample.scale[1]=.25f;
        lighting.sample.plane[2]=1;lighting.sample.normal_axis=2;lighting.sample.u_axis=0;lighting.directional_scale=.25f;
        memset(base,0x7e,sizeof(base));memset(lit,0x7e,sizeof(lit));
        CHECK(!rf_geomod_light_noise(rgb,sizeof(rgb),12,4,4,&random));
        CHECK(!rf_lightmap_pack_1555(rgb,sizeof(rgb),4,4,0,base,10,sizeof(base)));
        CHECK(!rf_lightmap_noise_live_rectangle(&lighting,0x12345678,lit,10,sizeof(lit)));
        CHECK(!memcmp(base,lit,sizeof(base)));
        light.type=2;light.radius=16;light.position[0]=light.position[1]=1.5f;light.position[2]=4;light.color[0]=1;
        lighting.lights=&light;lighting.light_count=1;
        CHECK(!rf_lightmap_noise_live_rectangle(&lighting,0x12345678,lit,10,sizeof(lit)));
        for(y=0;y<4;y++) {
            for(x=0;x<4;x++){uint16_t a,c;memcpy(&a,base+y*10+x*2,2);memcpy(&c,lit+y*10+x*2,2);CHECK((c&1023)==(a&1023) && ((c>>10)&31)>((a>>10)&31));}
            CHECK(lit[y*10+8]==0x7e && lit[y*10+9]==0x7e);
        }
        memcpy(saved,lit,sizeof(saved));
        CHECK(rf_lightmap_noise_live_rectangle(&lighting,0x12345678,lit,10,37)==RF_RANGE && !memcmp(saved,lit,sizeof(saved)));
        lighting.light_count=0;
        CHECK(!rf_lightmap_noise_live_rectangle(&lighting,0x12345678,lit,10,sizeof(lit)) && !memcmp(base,lit,sizeof(base)));
        puts("PASS: retained noise base, additive red light, exact removal restoration and rectangle guards");
    }
    {
        rf_geomod_vertex v[4]={{{-1,-1,0},{0,0}},{{1,-1,0},{1,0}},{{1,1,0},{1,1}},{{-1,1,0},{0,1}}};
        uint16_t ids[4]={10,11,12,13},fi[64],bi[64],next_ids[64],saved_ids[64];
        rf_geomod_vertex f[64],b[64],plain_f[64],plain_b[64],next[64],saved[64];
        const float planes[][4]={{1,0,0,0},{1,0,0,1},{1,0,0,-1},{0,1,0,0},{.70710677f,.70710677f,0,0}};
        unsigned p,i,k,side;uint32_t nf,nb,pf,pb,nn,nback;
        for(p=0;p<5;p++) {
            CHECK(!rf_geomod_polygon_split(v,4,planes[p],plain_f,64,plain_b,64,&pf,&pb));
            CHECK(!rf_geomod_polygon_split_tracked(v,4,planes[p],ids,99,f,fi,64,b,bi,64,&nf,&nb));
            CHECK(nf==pf && nb==pb && !memcmp(f,plain_f,nf*sizeof(*f)) && !memcmp(b,plain_b,nb*sizeof(*b)));
            for(side=0;side<2;side++) {
                rf_geomod_vertex *verts=side?b:f;uint16_t *tags=side?bi:fi;unsigned n=side?nb:nf;
                for(i=0;i<n;i++)for(k=0;k<2;k++) {
                    const float *point=verts[(i+k)%n].position;
                    if(tags[i]==99)CHECK(fabs(planes[p][0]*point[0]+planes[p][1]*point[1]+planes[p][3])<1e-5);
                    else {unsigned edge=tags[i]-10;CHECK(edge<4);CHECK(fabs(point[edge%2?0:1]-(edge==1 || edge==2?1:-1))<1e-6);}
                }
            }
        }
        CHECK(!rf_geomod_polygon_split_tracked(v,4,planes[0],ids,99,f,fi,64,b,bi,64,&nf,&nb));
        CHECK(!rf_geomod_polygon_split_tracked(f,nf,planes[3],fi,98,next,next_ids,64,b,bi,64,&nn,&nback));
        {unsigned mask=0;for(i=0;i<nn;i++){if(next_ids[i]==98)mask|=1;if(next_ids[i]==99)mask|=2;}CHECK(mask==3);}
        memcpy(saved,next,sizeof(saved));memcpy(saved_ids,next_ids,sizeof(saved_ids));nn=123;nback=456;
        CHECK(rf_geomod_polygon_split_tracked(v,4,planes[0],ids,99,next,next_ids,1,b,bi,64,&nn,&nback)==RF_RANGE);
        CHECK(nn==123 && nback==456 && !memcmp(next,saved,sizeof(saved)) && !memcmp(next_ids,saved_ids,sizeof(saved_ids)));
        puts("PASS: tracked split support IDs, geometry equivalence, consecutive cuts and rollback");
        {
            const float cutters[4][2][4]={{{1,0,0,0},{0,1,0,0}},{{0,1,0,0},{1,0,0,0}},
                {{1,0,0,2},{0,1,0,0}},{{0,0,1,0},{1,0,0,0}}};
            uint16_t cut_ids[2]={98,99};rf_geomod_fragment fragments[16],plain_fragments[16];
            rf_geomod_edge_tracking tracking={ids,cut_ids,next_ids};unsigned c,q;
            for(c=0;c<4;c++) {
                tracking.output=NULL;
                CHECK(!rf_geomod_polygon_subtract_tracked(v,4,cutters[c],2,NULL,0,NULL,0,&nn,&nback,&tracking));
                tracking.output=next_ids;
                CHECK(!rf_geomod_polygon_subtract_tracked(v,4,cutters[c],2,next,64,fragments,16,&nf,&nb,&tracking));
                CHECK(nf==nn && nb==nback);
                CHECK(!rf_geomod_polygon_subtract(v,4,cutters[c],2,plain_f,64,plain_fragments,16,&pf,&pb));
                CHECK(nf==pf && nb==pb && !memcmp(next,plain_f,nf*sizeof(*next)) && !memcmp(fragments,plain_fragments,nb*sizeof(*fragments)));
                for(q=0;q<nb;q++)for(i=0;i<fragments[q].count;i++)for(k=0;k<2;k++) {
                    unsigned at=fragments[q].first+i;uint16_t tag=next_ids[at];
                    const float *point=next[fragments[q].first+(i+k)%fragments[q].count].position;
                    if(tag>=98 && tag<=99){const float *plane=cutters[c][tag-98];CHECK(fabs(plane[0]*point[0]+plane[1]*point[1]+plane[2]*point[2]+plane[3])<1e-5);}
                    else {unsigned edge=tag-10;CHECK(edge<4);CHECK(fabs(point[edge%2?0:1]-(edge==1 || edge==2?1:-1))<1e-6);}
                }
            }
            memcpy(saved,next,sizeof(saved));memcpy(saved_ids,next_ids,sizeof(saved_ids));nf=123;nb=456;
            CHECK(rf_geomod_polygon_subtract_tracked(v,4,cutters[0],2,next,1,fragments,16,&nf,&nb,&tracking)==RF_RANGE);
            CHECK(nf==123 && nb==456 && !memcmp(next,saved,sizeof(saved)) && !memcmp(next_ids,saved_ids,sizeof(saved_ids)));
            puts("PASS: multi-plane edge provenance, coplanar/separated cases, geometry and query/rollback");
        }
    }
    {
        float planes[6][4];rf_geomod_vertex v[6][4];rf_geomod_face faces[6];
        uint16_t neighbors[24],saved[24];unsigned i,j;
        rf_geomod_mesh_view mesh={v[0],faces,24,6,0};
        box((float[3]){-16,-12,-20},(float[3]){16,12,20},planes,v);
        for(i=0;i<6;i++)faces[i]=(rf_geomod_face){i*4,4,0,i};
        CHECK(!rf_geomod_seed_adjacency(&mesh,neighbors,24));
        for(i=0;i<6;i++)for(j=0;j<4;j++)CHECK(neighbors[i*4+j]<6 && neighbors[i*4+j]/2!=i/2);
        memcpy(saved,neighbors,sizeof(saved));mesh.face_count=5;
        CHECK(rf_geomod_seed_adjacency(&mesh,neighbors,24)==RF_FORMAT && !memcmp(neighbors,saved,sizeof(saved)));
        mesh.face_count=6;faces[5].first=16;
        CHECK(rf_geomod_seed_adjacency(&mesh,neighbors,24)==RF_FORMAT && !memcmp(neighbors,saved,sizeof(saved)));
        faces[5].first=20;
        CHECK(rf_geomod_seed_adjacency(&mesh,neighbors,23)==RF_RANGE && !memcmp(neighbors,saved,sizeof(saved)));
        puts("PASS: exact seed edge adjacency and malformed/short-output rollback");
    }
    {
        const float planes[3][4]={{.8841080665588379f,-.17530789971351624f,-.4331513047218323f,14.125255584716797f},
            {.41470983624458313f,.8557782173156738f,-.30928853154182434f,13.848827362060547f},{-1,0,0,-16}};
        const unsigned order[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
        float expected[3]={-16,-7.3684163093566895f,2.9349284172058105f},out[3],variant[3][4];unsigned a,b,i,j;
        for(a=0;a<6;a++)for(b=0;b<8;b++) {
            for(i=0;i<3;i++)for(j=0;j<4;j++)variant[i][j]=planes[order[a][i]][j]*((b&(1u<<i))?-1:1);
            CHECK(!rf_geomod_plane_corner(variant,out));CHECK(!memcmp(out,expected,sizeof(out)));
        }
        memcpy(variant,planes,sizeof(variant));memcpy(variant[1],variant[0],sizeof(variant[0]));
        CHECK(rf_geomod_plane_corner(variant,out)==RF_FORMAT && !memcmp(out,expected,sizeof(out)));
        memcpy(variant,planes,sizeof(variant));variant[0][0]=NAN;
        CHECK(rf_geomod_plane_corner(variant,out)==RF_FORMAT && !memcmp(out,expected,sizeof(out)));
        CHECK(rf_geomod_plane_corner(NULL,out)==RF_RANGE && !memcmp(out,expected,sizeof(out)));
        puts("PASS: traced plane-corner solution,48 order/sign variants and invalid rollback");
    }
    {
        rf_geomod_debris_mesh output,saved;rf_random_state random={1};
        const float invalid[]={0,-1,NAN,INFINITY,1e38f,1e-40f};unsigned k;
        memset(&output,0x5a,sizeof(output));saved=output;
        for(k=0;k<sizeof(invalid)/sizeof(invalid[0]);k++) {
            CHECK(rf_geomod_debris_build(invalid[k],256,256,&random,&output)==RF_FORMAT);
            CHECK(random.value==1 && !memcmp(&output,&saved,sizeof(output)));
        }
        CHECK(rf_geomod_debris_build(.1f,0,256,&random,&output)==RF_RANGE);
        CHECK(random.value==1 && !memcmp(&output,&saved,sizeof(output)));
        CHECK(!rf_geomod_debris_build(.1f,256,256,&random,&output));
        CHECK(output.lifetime>=1 && output.lifetime<4 && random.value!=1);
        {
            float position[3]={0,0,0},origin[3]={0,0,0},velocity[3]={11,22,33},saved_velocity[3];
            uint32_t before=random.value;memcpy(saved_velocity,velocity,sizeof(velocity));
            CHECK(rf_geomod_debris_launch(position,origin,NAN,0,&random,velocity)==RF_FORMAT);
            CHECK(rf_geomod_debris_launch(position,origin,.1f,INFINITY,&random,velocity)==RF_FORMAT);
            position[0]=INFINITY;
            CHECK(rf_geomod_debris_launch(position,origin,.1f,0,&random,velocity)==RF_FORMAT);
            CHECK(random.value==before && !memcmp(velocity,saved_velocity,sizeof(velocity)));
            position[0]=0;
            CHECK(!rf_geomod_debris_launch(position,origin,.1f,0,&random,velocity));
            CHECK(fabsf(sqrtf(velocity[0]*velocity[0]+velocity[1]*velocity[1]+velocity[2]*velocity[2])-12)<.00001f);
            CHECK(!rf_geomod_debris_launch(position,origin,.25f,1,&random,velocity));
            CHECK(velocity[0]==0 && velocity[1]==0 && velocity[2]==0);
        }

    }

    {
        rf_geomod_debris_probe probes[14]={{0}};int32_t count=123;unsigned i;
        float origin[3]={0,0,0},points[14][3],saved_points[14][3];
        CHECK(!rf_geomod_debris_count(.1f,probes,&count) && count==-1);
        for(i=0;i<14;i++){probes[i].hit=1;probes[i].has_face=1;}
        CHECK(!rf_geomod_debris_count(3.75f,probes,&count) && count==16);
        count=123;probes[4].distance=NAN;
        CHECK(rf_geomod_debris_count(1,probes,&count)==RF_FORMAT && count==123);
        probes[4].face_flags=8;
        CHECK(!rf_geomod_debris_count(1,probes,&count) && count==16);
        memset(points,0x5a,sizeof(points));memcpy(saved_points,points,sizeof(points));origin[2]=INFINITY;
        CHECK(rf_geomod_debris_probe_points(origin,1,points)==RF_FORMAT);
        CHECK(!memcmp(points,saved_points,sizeof(points)));
        origin[2]=0;CHECK(!rf_geomod_debris_probe_points(origin,1,points));
        CHECK(points[0][0]==1 && points[1][0]==-1 && points[4][2]==1 && points[5][2]==-1);
    }
    CHECK(!light_grid_check());
    CHECK(!light_bake_check());
    {
        unsigned axis,k;const int signs[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
        for(axis=0;axis<3;axis++) {
            float vertices[4][3]={{0}},start[3]={0},delta[3]={0};
            unsigned x=(axis+1)%3,y=(axis+2)%3,matched;
            rf_collision_face face={0};rf_collision_ray_hit hit;
            face.vertices=vertices;face.count=4;face.triangle_surface=1;
            face.plane[axis]=1;face.plane[3]=-2;
            for(k=0;k<3;k++){face.minimum[k]=-10;face.maximum[k]=10;}
            for(k=0;k<4;k++){vertices[k][axis]=2;vertices[k][x]=(float)signs[k][0];vertices[k][y]=(float)signs[k][1];}
            start[axis]=5;delta[axis]=-6;
            CHECK(!rf_collision_thin_face(&face,start,delta,1,&hit,&matched) && matched && hit.fraction==.5f);
            CHECK(!rf_collision_thin_face(&face,start,delta,.49f,&hit,&matched) && !matched);
            start[x]=nextafterf(1,0);
            CHECK(!rf_collision_thin_face(&face,start,delta,1,&hit,&matched) && matched);
            start[x]=nextafterf(1,2);
            CHECK(!rf_collision_thin_face(&face,start,delta,1,&hit,&matched) && !matched);
            start[x]=0;start[axis]=-1;delta[axis]=6;
            CHECK(!rf_collision_thin_face(&face,start,delta,1,&hit,&matched) && !matched);
            start[axis]=2;delta[axis]=0;delta[x]=1;
            CHECK(!rf_collision_thin_face(&face,start,delta,1,&hit,&matched) && !matched);
            start[axis]=5;delta[axis]=-6;delta[x]=.25f;
            CHECK(!rf_collision_thin_face(&face,start,delta,1,&hit,&matched) && matched && hit.fraction==.5f);
        }
    }
    float source[6][4],cutter[6][4],lo[3]={-2,-2,-2},hi[3]={2,2,2};
    rf_geomod_vertex faces[6][4],cut_faces[6][4],out[2048],sentinel[64];rf_geomod_fragment fragments[32];
    unsigned i,j,n,pieces,caps=0;double result=0;
    const float cut_lo[3]={-1,-1,-3},cut_hi[3]={1,1,3};
    box(lo,hi,source,faces);box(cut_lo,cut_hi,cutter,cut_faces);
    for(i=0;i<6;i++) {
        CHECK(!rf_geomod_polygon_subtract(faces[i],4,cutter,6,out,2048,fragments,32,&n,&pieces));
        for(j=0;j<pieces;j++){result+=volume(out+fragments[j].first,fragments[j].count);CHECK(record(out+fragments[j].first,fragments[j].count));}
        CHECK(!rf_geomod_interior_face(cut_faces[i],4,source,6,out,2048,&n));
        if(n){CHECK(n==4);caps++;result+=volume(out,n);CHECK(record(out,n));}
    }
    CHECK(caps==4 && fabs(result-48)<1e-5); /* Cube64 minus through-tunnel16. */
    CHECK(closed());
    polygon_count--;CHECK(!closed());polygon_count++; /* Missing-face control. */
    memset(out,0xa5,sizeof(out));memcpy(sentinel,out,sizeof(sentinel));n=77;
    CHECK(rf_geomod_interior_face(cut_faces[0],4,source,6,out,3,&n)==RF_RANGE);
    CHECK(n==77 && !memcmp(out,sentinel,sizeof(sentinel)));
    CHECK(!rf_geomod_interior_face(faces[0],4,source,6,out,64,&n) && n==0);
    CHECK(!rf_geomod_interior_face(cut_faces[0],4,source,6,NULL,0,&n) && n==4);
    {
        const float lows[][3]={{-2,-2,-2},{2,-2,-2},{-1,-1,0},{-3,-3,-3},{3,3,3}};
        const float highs[][3]={{2,2,2},{4,2,2},{1,1,2},{3,3,3},{4,4,4}};
        const double expected[]={0,64,56,0,64};unsigned c;
        for(c=0;c<5;c++) {
            result=0;surface_count=polygon_count=0;box(lows[c],highs[c],cutter,cut_faces);
            for(i=0;i<6;i++) {
                CHECK(!rf_geomod_polygon_subtract(faces[i],4,cutter,6,out,2048,fragments,32,&n,&pieces));
                for(j=0;j<pieces;j++){result+=volume(out+fragments[j].first,fragments[j].count);CHECK(record(out+fragments[j].first,fragments[j].count));}
                CHECK(!rf_geomod_interior_face(cut_faces[i],4,source,6,out,2048,&n));
                if(n){result+=volume(out,n);CHECK(record(out,n));}
            }
            if(fabs(result-expected[c])>1e-5){fprintf(stderr,"boundary case%u volume %.9g expected %.9g\n",c,result,expected[c]);return 1;}
            CHECK(closed());
        }
    }
    {
        static rf_geomod_cut_work work;
        rf_geomod_face source_faces[6],cutter_faces[6];rf_geomod_mesh_view src,cut,live,pending;
        rf_geomod_storage *storage=NULL,*small=NULL;unsigned interiors=0;
        box(cut_lo,cut_hi,cutter,cut_faces);
        for(i=0;i<6;i++) {
            source_faces[i]=(rf_geomod_face){i*4,4,100+i,i};
            cutter_faces[i]=(rf_geomod_face){i*4,4,200+i,i};
        }
        src=(rf_geomod_mesh_view){faces[0],source_faces,24,6,0};
        cut=(rf_geomod_mesh_view){cut_faces[0],cutter_faces,24,6,0};
        CHECK(!collision_clearance(&src,0));
        CHECK(!rf_geomod_storage_open(&src,512,128,65536,&storage));
        CHECK(!rf_geomod_storage_prepare_convex_cut(storage,&cut,&work));
        CHECK(!rf_geomod_storage_view(storage,&live) && live.generation==1 && live.face_count==6);
        CHECK(!rf_geomod_storage_pending(storage,&pending));
        result=0;surface_count=polygon_count=0;
        for(i=0;i<pending.face_count;i++) {
            const rf_geomod_face *f=pending.faces+i;
            result+=volume(pending.vertices+f->first,f->count);CHECK(record(pending.vertices+f->first,f->count));
            if(f->source_face==UINT32_MAX){interiors++;CHECK(f->material>=200 && f->material<206);}
            else CHECK(f->source_face<6 && f->material==100+f->source_face);
        }
        CHECK(interiors==4 && fabs(result-48)<1e-5 && closed());
        CHECK(!collision_clearance(&pending,1));
        CHECK(!rf_geomod_storage_commit(storage));CHECK(!rf_geomod_storage_view(storage,&live) && live.generation==2);
        CHECK(rf_geomod_storage_prepare_convex_cut(storage,&cut,&work)==RF_FORMAT); /* Do not treat concave result as convex. */
        CHECK(!rf_geomod_storage_reset(storage));CHECK(!rf_geomod_storage_view(storage,&live) && live.face_count==6);
        CHECK(!collision_clearance(&live,0));
        CHECK(!rf_geomod_storage_open(&src,24,6,4096,&small));
        CHECK(rf_geomod_storage_prepare_convex_cut(small,&cut,&work)==RF_RANGE);
        CHECK(!rf_geomod_storage_view(small,&live) && live.generation==1 && live.face_count==6 && !memcmp(live.vertices,faces,sizeof(faces)));
        CHECK(rf_geomod_storage_pending(small,&pending)==RF_RANGE);
        rf_geomod_storage_close(&small);rf_geomod_storage_close(&storage);
    }
    {
        static rf_geomod_multi_work work;
        rf_geomod_vertex other_vertices[6][4];rf_geomod_face sf[6],cf[2][6];
        rf_geomod_mesh_view src,history[2],pending,live;rf_geomod_storage *s=NULL,*small=NULL;
        /* Crossed tunnels; identical, adjacent, separated and nested cutters;
         * partial same-plane overlap; whole-source removal; external contact. */
        const float lows[][3]={{-1,-3,-1},{-1,-1,-3},{1,-1,-3},{1.25f,-1,-3},{-.5f,-.5f,-3},{0,-1,-3},{-3,-3,-3},{2,-1,-3}};
        const float highs[][3]={{1,3,1},{1,1,3},{2,1,3},{1.75f,1,3},{.5f,.5f,3},{1.5f,1,3},{3,3,3},{3,1,3}};
        const double expected[]={40,48,40,44,48,44,0,48};unsigned c,order,repeat;
        box(cut_lo,cut_hi,cutter,cut_faces);
        for(i=0;i<6;i++) {
            sf[i]=(rf_geomod_face){i*4,4,100+i,i};
            cf[0][i]=(rf_geomod_face){i*4,4,200+i,i};
            cf[1][i]=(rf_geomod_face){i*4,4,300+i,i};
        }
        src=(rf_geomod_mesh_view){faces[0],sf,24,6,0};
        CHECK(!rf_geomod_storage_open(&src,2048,128,100000,&s));
        for(c=0;c<sizeof(expected)/sizeof(*expected);c++)for(order=0;order<2;order++) {
            box(lows[c],highs[c],cutter,other_vertices);
            history[order]=(rf_geomod_mesh_view){cut_faces[0],cf[0],24,6,0};
            history[order^1]=(rf_geomod_mesh_view){other_vertices[0],cf[1],24,6,0};
            CHECK(!rf_geomod_storage_reset(s));
            CHECK(!rf_geomod_storage_prepare_cuts(s,history,1,&work));CHECK(!rf_geomod_storage_commit(s));
            for(repeat=0;repeat<2;repeat++) {
                CHECK(!rf_geomod_storage_view(s,&live));
                CHECK(!rf_geomod_storage_prepare_cuts(s,history,2,&work));
                CHECK(!rf_geomod_storage_pending(s,&pending));
                CHECK(pending.generation==live.generation+1);
                result=0;surface_count=polygon_count=0;
                for(i=0;i<pending.face_count;i++) {
                    const rf_geomod_face *f=pending.faces+i;
                    result+=volume(pending.vertices+f->first,f->count);
                    CHECK(record(pending.vertices+f->first,f->count));
                    if(f->source_face!=UINT32_MAX)CHECK(f->material==100+f->source_face);
                    else {
                        CHECK((f->material>=200 && f->material<206) || (f->material>=300 && f->material<306));
                        if(c==1)CHECK(f->material/100==(order?3u:2u)); /* Earlier cap owns duplicates. */
                    }
                }
                if(fabs(result-expected[c])>1e-5 || !closed()) {
                    fprintf(stderr,"history case%u order%u repeat%u volume %.9g expected %.9g\n",c,order,repeat,result,expected[c]);return 1;
                }
                CHECK(!rf_geomod_storage_commit(s));
            }
        }
        CHECK(!rf_geomod_storage_prepare_cuts(s,NULL,0,&work));
        CHECK(!rf_geomod_storage_commit(s));CHECK(!rf_geomod_storage_view(s,&live));
        CHECK(live.face_count==6 && live.vertex_count==24 && !memcmp(live.vertices,faces,sizeof(faces)));
        history[1].face_count=33;
        CHECK(rf_geomod_storage_prepare_cuts(s,history,2,&work)==RF_RANGE);
        CHECK(rf_geomod_storage_pending(s,&pending)==RF_RANGE);
        CHECK(!rf_geomod_storage_view(s,&pending) && pending.generation==live.generation);
        CHECK(rf_geomod_storage_prepare_cuts(s,history,9,&work)==RF_RANGE);
        CHECK(!rf_geomod_storage_open(&src,24,6,4096,&small));
        history[0]=(rf_geomod_mesh_view){cut_faces[0],cf[0],24,6,0};
        CHECK(rf_geomod_storage_prepare_cuts(small,history,1,&work)==RF_RANGE);
        CHECK(!rf_geomod_storage_view(small,&live) && live.generation==1 && live.face_count==6 && !memcmp(live.vertices,faces,sizeof(faces)));
        CHECK(rf_geomod_storage_pending(small,&pending)==RF_RANGE);
        rf_geomod_storage_close(&small);rf_geomod_storage_close(&s);
        {
            rf_geomod_vertex third[6][4];rf_geomod_mesh_view many[8];
            const float l[3]={-3,-1,-1},h[3]={3,1,1};unsigned rotation,k;
            box(lows[0],highs[0],cutter,other_vertices);box(l,h,cutter,third);
            many[0]=(rf_geomod_mesh_view){cut_faces[0],cf[0],24,6,0};
            many[1]=(rf_geomod_mesh_view){other_vertices[0],cf[1],24,6,0};
            many[2]=(rf_geomod_mesh_view){third[0],cf[1],24,6,0};
            for(i=3;i<8;i++)many[i]=many[i%3];
            /* Three intersecting tunnels and five duplicate cutters. Rotate
             * the entire construction to exercise non-axis-aligned planes. */
            for(rotation=0;rotation<2;rotation++) {
                if(rotation)for(k=0;k<4;k++)for(i=0;i<6;i++)for(j=0;j<4;j++) {
                    rf_geomod_vertex *v=k==0?&faces[i][j]:k==1?&cut_faces[i][j]:k==2?&other_vertices[i][j]:&third[i][j];
                    float x=v->position[0],y=v->position[1];
                    v->position[0]=.8f*x-.6f*y;v->position[1]=.6f*x+.8f*y;
                }
                CHECK(!rf_geomod_storage_open(&src,2048,128,100000,&s));
                CHECK(!rf_geomod_storage_prepare_cuts(s,many,8,&work));
                CHECK(!rf_geomod_storage_pending(s,&pending));
                {
                    static float collision_positions[2048][3];static rf_collision_face collision_faces[128];
                    static rf_collision_face_filter collision_filters[128];
                    CHECK(!rf_geomod_collision_faces(&pending,collision_filters,collision_positions,2048,collision_faces,128));
                }
                result=0;surface_count=polygon_count=0;
                for(i=0;i<pending.face_count;i++) {
                    const rf_geomod_face *f=pending.faces+i;
                    result+=volume(pending.vertices+f->first,f->count);
                    CHECK(record(pending.vertices+f->first,f->count));
                }
                if(fabs(result-32)>=1e-4 || !closed()) {
                    fprintf(stderr,"eight cuts rotation%u volume %.9g faces%u\n",rotation,result,pending.face_count);return 1;
                }
                rf_geomod_storage_close(&s);
            }
        }
    }
    {
        static rf_geomod_multi_work work;
        rf_geomod_face sf[6],cf[6];rf_geomod_mesh_view src,cuts[2],pending;
        rf_geomod_vertex second[6][4];rf_geomod_storage *s=NULL;
        const float other_lo[3]={-1,-3,-1},other_hi[3]={1,3,1};
        box(lo,hi,source,faces);box(cut_lo,cut_hi,cutter,cut_faces);box(other_lo,other_hi,cutter,second);
        for(i=0;i<6;i++) {
            rf_geomod_vertex t=faces[i][0];faces[i][0]=faces[i][3];faces[i][3]=t;
            t=faces[i][1];faces[i][1]=faces[i][2];faces[i][2]=t;
            sf[i]=(rf_geomod_face){i*4,4,100+i,i};cf[i]=(rf_geomod_face){i*4,4,200+i,i};
        }
        src=(rf_geomod_mesh_view){faces[0],sf,24,6,0};
        cuts[0]=(rf_geomod_mesh_view){cut_faces[0],cf,24,6,0};cuts[1]=(rf_geomod_mesh_view){second[0],cf,24,6,0};
        CHECK(!rf_geomod_storage_open(&src,2048,128,100000,&s));
        CHECK(rf_geomod_storage_prepare_cuts(s,cuts,1,&work)==RF_FORMAT);
        for(unsigned count=1;count<=3;count++) {
            if(count==3)cuts[1]=cuts[0];
            CHECK(!rf_geomod_storage_prepare_cavity_cuts(s,cuts,count==3?2:count,&work));
            CHECK(!rf_geomod_storage_pending(s,&pending));
            result=0;surface_count=polygon_count=0;
            for(i=0;i<pending.face_count;i++) {
                const rf_geomod_face *f=pending.faces+i;result+=volume(pending.vertices+f->first,f->count);
                CHECK(record(pending.vertices+f->first,f->count));
            }
            CHECK(fabs(result-(-64-8*(double)(count==3?1:count)))<1e-5 && closed());
            CHECK(!rf_geomod_storage_commit(s));
        }
        rf_geomod_storage_close(&s);
    }
    if(argc>=2) {
        static rf_geomod_multi_work work;static float positions[2048][3];
        static rf_collision_face bound[128];static rf_collision_face_filter filters[128];
        rf_vpp archive={0};rf_level level={0};rf_geometry geometry={0};
        rf_geomod_face sf[6],cf[6];rf_geomod_mesh_view src,cut,pending;rf_geomod_storage *s=NULL;
        rf_collision_tree tree={0};rf_collision_tree_hit hit;uint32_t matched;char path[1024];
        const float wall_lo[3]={-1,-11,19},wall_hi[3]={1,-9,21};
        float ray_start[3]={0,-10,19},ray_delta[3]={0,0,4};
        snprintf(path,sizeof(path),"%s/levelsm.vpp",argv[1]);
        CHECK(!rf_vpp_open(&archive,path));CHECK(!rf_level_open(&level,&archive,"glass_house.rfl"));
        CHECK(!rf_geometry_open(&geometry,&level,1024*1024));
        CHECK(geometry.faces==598 && geometry.vertices==782);
        for(i=0;i<6;i++) {
            rf_geometry_face f;CHECK(!rf_geometry_get_face(&geometry,i,&f) && f.corners==4);
            sf[i]=(rf_geomod_face){i*4,4,f.texture,i};
            filters[i].face_flags=f.flags;
            for(j=0;j<4;j++) {
                rf_geometry_corner c;CHECK(!rf_geometry_get_corner(&geometry,i,j,&c));
                CHECK(!rf_geometry_vertex(&geometry,c.vertex,faces[i][j].position));memcpy(faces[i][j].uv,c.uv,8);
            }
            cf[i]=(rf_geomod_face){i*4,4,2,UINT32_MAX};
        }
        src=(rf_geomod_mesh_view){faces[0],sf,24,6,0};result=0;
        for(i=0;i<6;i++)result+=volume(faces[i],4);
        CHECK(fabs(result+30720)<1e-4); /* Actual inward room,32x24x40. */
        box(wall_lo,wall_hi,cutter,cut_faces);cut=(rf_geomod_mesh_view){cut_faces[0],cf,24,6,0};
        CHECK(!rf_geomod_storage_open(&src,2048,128,100000,&s));
        CHECK(!rf_geomod_storage_prepare_cavity_cuts(s,&cut,1,&work));
        CHECK(!rf_geomod_storage_pending(s,&pending));result=0;surface_count=polygon_count=0;
        for(i=0;i<pending.face_count;i++) {
            const rf_geomod_face *f=pending.faces+i;result+=volume(pending.vertices+f->first,f->count);
            CHECK(record(pending.vertices+f->first,f->count));
        }
        CHECK(fabs(result+30724)<1e-4 && closed());
        CHECK(!rf_geomod_collision_faces(&pending,filters,positions,2048,bound,128));
        CHECK(!rf_collision_tree_open(bound,pending.face_count,1024*1024,&tree));
        CHECK(!rf_collision_thin_tree(tree.nodes,tree.node_count,tree.faces,tree.face_count,0,
            ray_start,ray_delta,1,tree.stack,tree.node_capacity,&hit,&matched));
        CHECK(matched && fabs(hit.hit.fraction-.5f)<1e-5 && hit.hit.normal[2]<-.99f);
        {
            rf_geomod_terrain *terrain=NULL,*limited=NULL;rf_geomod_terrain_view live,before;
            rf_collision_face_filter generated={0};uint32_t minimum_budget,baseline_bytes;
            rf_geometry_collision_world base={0};rf_geometry_collision_overlay overlay={0};
            rf_geometry_world_hit original_hit,overlay_hit;uint32_t original_matched,ids[128],fallback=UINT32_MAX;
            const float center[3]={0,-10,20},extent[3]={1,1,1};float invalid[3]={0,1,1};
            generated.face_flags=256;
            CHECK(!rf_geometry_collision_world_open(&geometry,8*1024*1024,&base));
            CHECK(!rf_geometry_collision_overlay_open(&base,0,128,65536,&overlay));
            for(i=0;i<6;i++)if(sf[i].material==2)fallback=i;
            CHECK(fallback!=UINT32_MAX);
            CHECK(!rf_geomod_terrain_open(&src,filters,&generated,1,2048,128,1024*1024,&terrain));
            CHECK(!rf_geomod_terrain_get(terrain,&before));minimum_budget=before.peak_bytes;baseline_bytes=before.resident_bytes;
            CHECK(before.cuts==0 && before.mesh.generation==1);
            for(i=0;i<8;i++) {
                CHECK(!rf_geomod_terrain_cut_box(terrain,center,extent,2));
                CHECK(!rf_geomod_terrain_get(terrain,&live));CHECK(live.cuts==i+1 && live.mesh.generation==i+2);
                CHECK(live.resident_bytes<=live.peak_bytes && live.peak_bytes<=1024*1024);
                for(j=0;j<live.mesh.face_count;j++)ids[j]=live.mesh.faces[j].source_face==UINT32_MAX?fallback:live.mesh.faces[j].source_face;
                CHECK(!rf_geometry_collision_overlay_bind(&overlay,live.tree,ids,live.mesh.face_count));
                CHECK(!rf_geometry_collision_world_ray(&overlay.world,0,ray_start,ray_delta,1,&overlay_hit,&matched));
                CHECK(matched && overlay_hit.room==0 && fabs(overlay_hit.hit.fraction-.5f)<1e-5);
                {
                    rf_geometry_world_sweep_hit body;rf_collision_room_location location;uint32_t tracked;
                    const float inside[3]={0,-10,20.25f};
                    CHECK(!rf_geometry_collision_world_sweep(&overlay.world,0,ray_start,ray_delta,.5f,1,&body,&matched));
                    CHECK(matched && body.room==0 && fabs(body.hit.fraction-.375f)<1e-5);
                    {int located=rf_geometry_collision_world_locate(&overlay.world,inside,&location);
                     if(located || location.room!=0){fprintf(stderr,"locate status%d room%u face%u retries%u\n",located,location.room,location.face,location.retries);return 1;}}
                    CHECK(!rf_geometry_collision_world_track(&overlay.world,0,ray_start,inside,0,&tracked) && tracked==0);
                }
                CHECK(!rf_geometry_collision_world_ray(&base,0,ray_start,ray_delta,1,&original_hit,&original_matched));
                CHECK(original_matched && fabs(original_hit.hit.fraction-.25f)<1e-5); /* Base never mutated. */
                {
                    const float p[3]={0,-10,10},d[3]={0,0,-20};
                    CHECK(!rf_geometry_collision_world_ray(&base,0,p,d,1,&original_hit,&original_matched));
                    CHECK(!rf_geometry_collision_world_ray(&overlay.world,0,p,d,1,&overlay_hit,&matched));
                    CHECK(original_matched && matched && original_hit.room!=0 && original_hit.face==overlay_hit.face &&
                        fabs(original_hit.hit.fraction-overlay_hit.hit.fraction)<1e-6);
                }
                ids[0]=UINT32_MAX;
                CHECK(rf_geometry_collision_overlay_bind(&overlay,live.tree,ids,live.mesh.face_count)==RF_FORMAT);
                CHECK(!rf_geometry_collision_world_ray(&overlay.world,0,ray_start,ray_delta,1,&overlay_hit,&matched));
                CHECK(matched && fabs(overlay_hit.hit.fraction-.5f)<1e-5); /* Failed bind preserves map/tree. */
                CHECK(!rf_collision_thin_tree(live.tree->nodes,live.tree->node_count,live.tree->faces,live.tree->face_count,0,
                    ray_start,ray_delta,1,live.tree->stack,live.tree->node_capacity,&hit,&matched));
                CHECK(matched && fabs(hit.hit.fraction-.5f)<1e-5);
                for(j=0;j<live.mesh.face_count;j++) {
                    uint32_t id=live.mesh.faces[j].source_face;
                    CHECK(live.faces[j].filter.face_flags==(id==UINT32_MAX?256:filters[id].face_flags));
                }
            }
            before=live;
            CHECK(rf_geomod_terrain_cut_box(terrain,center,extent,2)==RF_RANGE);
            CHECK(!rf_geomod_terrain_get(terrain,&live) && live.mesh.generation==before.mesh.generation && live.cuts==8);
            CHECK(!rf_geomod_terrain_reset(terrain));CHECK(!rf_geomod_terrain_get(terrain,&live));
            for(j=0;j<live.mesh.face_count;j++)ids[j]=live.mesh.faces[j].source_face;
            CHECK(!rf_geometry_collision_overlay_bind(&overlay,live.tree,ids,live.mesh.face_count));
            CHECK(!rf_geometry_collision_world_ray(&overlay.world,0,ray_start,ray_delta,1,&overlay_hit,&matched));
            CHECK(matched && fabs(overlay_hit.hit.fraction-.25f)<1e-5);
            CHECK(!live.cuts && live.mesh.face_count==6 && live.resident_bytes==baseline_bytes);
            CHECK(!rf_collision_thin_tree(live.tree->nodes,live.tree->node_count,live.tree->faces,live.tree->face_count,0,
                ray_start,ray_delta,1,live.tree->stack,live.tree->node_capacity,&hit,&matched));
            CHECK(matched && fabs(hit.hit.fraction-.25f)<1e-5);
            before=live;CHECK(rf_geomod_terrain_cut_box(terrain,center,invalid,2)==RF_FORMAT);
            CHECK(!rf_geomod_terrain_get(terrain,&live) && live.mesh.generation==before.mesh.generation);
            CHECK(rf_geomod_terrain_open(&src,filters,&generated,1,2048,128,minimum_budget-1,&limited)==RF_RANGE && !limited);
            CHECK(!rf_geomod_terrain_open(&src,filters,&generated,1,2048,128,minimum_budget,&limited));
            CHECK(!rf_geomod_terrain_get(limited,&before));
            CHECK(rf_geomod_terrain_cut_box(limited,center,extent,2)==RF_RANGE);
            CHECK(!rf_geomod_terrain_get(limited,&live) && !live.cuts && live.mesh.generation==before.mesh.generation &&
                live.mesh.vertices==before.mesh.vertices && live.faces==before.faces);
            CHECK(!rf_collision_thin_tree(live.tree->nodes,live.tree->node_count,live.tree->faces,live.tree->face_count,0,
                ray_start,ray_delta,1,live.tree->stack,live.tree->node_capacity,&hit,&matched));
            CHECK(matched && fabs(hit.hit.fraction-.25f)<1e-5);
            printf("PASS: terrain publication/reset,8-cut admission and allocation rollback; initial peak%u bytes\n",minimum_budget);
            rf_geometry_collision_overlay_close(&overlay);rf_geometry_collision_overlay_close(&overlay);
            rf_geometry_collision_world_close(&base);
            rf_geomod_terrain_close(&limited);rf_geomod_terrain_close(&terrain);rf_geomod_terrain_close(&terrain);
        }
        rf_collision_tree_close(&tree);rf_geomod_storage_close(&s);rf_geometry_close(&geometry);rf_vpp_close(&archive);
        puts("PASS: authored Glass House cavity expansion, closed edges and excavated-wall collision");
    }
    {
        /* A cube with a depressed top center: volume7, not convex-hull8.
         * This independent analytic fixture exposes accidental hull filling. */
        rf_geomod_vertex source_v[6][4],cube[6][4],cut_v[2][42];
        rf_geomod_face source_f[6],cut_f[14];float planes[6][4];
        float kernels[2][3]={{0,0,0},{0,0,0}};
        rf_geomod_mesh_view cuts[2],source,pending,live;
        static rf_geomod_multi_work work;
        static float positions[2048][3];static rf_collision_face bound[128];
        static rf_collision_face_filter filters[128];
        rf_geomod_storage *owner=NULL,*limited=NULL;uint32_t i,j,k,n=0,cavity,repeat;
        box((float[3]){-1,-1,-1},(float[3]){1,1,1},planes,cube);
        for(i=0;i<6;i++) {
            uint32_t triangles=i==5?4:2;
            for(j=0;j<triangles;j++) {
                rf_geomod_vertex *v=cut_v[0]+n*3;
                cut_f[n]=(rf_geomod_face){n*3,3,77,UINT32_MAX};
                if(i==5) {
                    v[0]=cube[i][j];v[1]=cube[i][(j+1)%4];
                    v[2]=(rf_geomod_vertex){{0,0,.25f},{0,0}};
                } else {v[0]=cube[i][0];v[1]=cube[i][j+1];v[2]=cube[i][j+2];}
                /* Globally affine UVs let clipped-corner interpolation be checked. */
                for(k=0;k<3;k++){v[k].uv[0]=v[k].position[0]*2+v[k].position[2];v[k].uv[1]=v[k].position[1]*3;}
                n++;
            }
        }
        CHECK(n==14);memcpy(cut_v[1],cut_v[0],sizeof(cut_v[0]));
        for(i=0;i<2;i++)cuts[i]=(rf_geomod_mesh_view){cut_v[i],cut_f,42,14,0};
        for(cavity=0;cavity<2;cavity++) {
            box((float[3]){-2,-2,-2},(float[3]){2,2,cavity?0:2},planes,source_v);
            for(i=0;i<6;i++) {
                source_f[i]=(rf_geomod_face){i*4,4,9,i};
                if(cavity)for(j=0;j<2;j++){rf_geomod_vertex temp=source_v[i][j];source_v[i][j]=source_v[i][3-j];source_v[i][3-j]=temp;}
            }
            source=(rf_geomod_mesh_view){source_v[0],source_f,24,6,0};
            CHECK(!rf_geomod_storage_open(&source,2048,128,1024*1024,&owner));
            for(repeat=1;repeat<=2;repeat++) {
                double total=0;
                {int status=rf_geomod_storage_prepare_star_cuts(owner,cuts,kernels,repeat,cavity,&work);if(status)fprintf(stderr,"star status%d cavity%u repeat%u\n",status,cavity,repeat);CHECK(!status);}
                CHECK(!rf_geomod_storage_pending(owner,&pending));surface_count=polygon_count=0;

                for(i=0;i<pending.face_count;i++) {
                    const rf_geomod_face *f=pending.faces+i;
                    total+=volume(pending.vertices+f->first,f->count);
                    CHECK(record(pending.vertices+f->first,f->count));
                    if(f->material==77)for(j=0;j<f->count;j++) {
                        const rf_geomod_vertex *v=pending.vertices+f->first+j;
                        CHECK(fabs(v->uv[0]-v->position[0]*2-v->position[2])<1e-5);
                        CHECK(fabs(v->uv[1]-v->position[1]*3)<1e-5);
                    }
                }
                CHECK(fabs(total-(cavity?-35:57))<1e-5 && closed());
                CHECK(!rf_geomod_collision_faces(&pending,filters,positions,2048,bound,128));
                {
                    rf_collision_tree tree={0};rf_collision_tree_hit hit;uint32_t matched,x,y;
                    CHECK(!rf_collision_tree_open(bound,pending.face_count,1024*1024,&tree));
                    for(x=0;x<9;x++)for(y=0;y<9;y++) {
                        float start[3]={-.88f+x*.22f,-.87f+y*.2175f,0},delta[3]={0,0,2};
                        float height=.25f+.75f*fmaxf(fabsf(start[0]),fabsf(start[1]));
                        CHECK(!rf_collision_thin_tree(tree.nodes,tree.node_count,tree.faces,tree.face_count,0,
                            start,delta,1,tree.stack,tree.node_capacity,&hit,&matched));
                        CHECK(matched && fabs(hit.hit.fraction-height*.5f)<1e-5);
                    }
                    rf_collision_tree_close(&tree);
                }
                CHECK(!rf_geomod_storage_commit(owner));
            }
            CHECK(!rf_geomod_storage_view(owner,&live));
            kernels[0][2]=2;
            CHECK(rf_geomod_storage_prepare_star_cuts(owner,cuts,kernels,1,cavity,&work)==RF_FORMAT);
            kernels[0][2]=0;cut_f[0].count=2;
            CHECK(rf_geomod_storage_prepare_star_cuts(owner,cuts,kernels,1,cavity,&work)==RF_FORMAT);
            cut_f[0].count=3;
            cut_v[0][0].position[0]+=.125f;
            CHECK(rf_geomod_storage_prepare_star_cuts(owner,cuts,kernels,1,cavity,&work)==RF_FORMAT);
            cut_v[0][0].position[0]-=.125f;
            {rf_geomod_vertex temp=cut_v[0][0];cut_v[0][0]=cut_v[0][1];cut_v[0][1]=temp;}
            CHECK(rf_geomod_storage_prepare_star_cuts(owner,cuts,kernels,1,cavity,&work)==RF_FORMAT);
            {rf_geomod_vertex temp=cut_v[0][0];cut_v[0][0]=cut_v[0][1];cut_v[0][1]=temp;}
            CHECK(!rf_geomod_storage_view(owner,&pending) && pending.generation==live.generation && pending.vertices==live.vertices);
            CHECK(!rf_geomod_storage_open(&source,24,6,1024*1024,&limited));
            CHECK(rf_geomod_storage_prepare_star_cuts(limited,cuts,kernels,1,cavity,&work)==RF_RANGE);
            CHECK(!rf_geomod_storage_view(limited,&pending) && pending.generation==1 && pending.face_count==6);
            rf_geomod_storage_close(&limited);rf_geomod_storage_close(&owner);
        }
        puts("PASS: concave star cuts preserve volume, closed edges, affine UVs,324 analytic collision rays and rollback");
    }
    if(argc>2) {
        /* Independently captured original factory corners, generated locally.
         * Upward intersections are checked against those input triangles. */
        FILE *file=fopen(argv[2],"rb");uint32_t count,i,j,k,cavity,repeat;
        rf_geomod_vertex vertices[2][96],source_v[6][4];rf_geomod_face faces[32],source_f[6];
        rf_geomod_mesh_view cutters[2],source,pending;rf_geomod_storage *owner=NULL;
        rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view live,before;rf_collision_face_filter original_filters[6]={{0}},generated={0};
        static rf_geomod_multi_work work;float kernels[2][3]={{0,0,0},{.4f,0,0}},planes[6][4];
        static float positions[4096][3];static rf_collision_face bound[512];
        static rf_collision_face_filter filters[512];double removed=0;
        CHECK(file && fread(&count,4,1,file)==1 && count>=4 && count<=32);
        CHECK(fread(vertices[0],sizeof(rf_geomod_vertex),count*3,file)==count*3 && fgetc(file)==EOF);fclose(file);
        memcpy(vertices[1],vertices[0],count*3*sizeof(rf_geomod_vertex));
        for(i=0;i<count;i++) {
            faces[i]=(rf_geomod_face){i*3,3,77,UINT32_MAX};
            removed+=volume(vertices[0]+i*3,3);
            for(j=0;j<3;j++)vertices[1][i*3+j].position[0]+=.4f;
        }
        CHECK(removed>0);
        {
            rf_geomod_template shape,saved,decoded;unsigned char packed[1229];size_t bytes;uint32_t length;
            FILE *asset=fopen("build/data/geomod-template.bin","rb");CHECK(asset);
            bytes=fread(packed,1,sizeof(packed),asset);CHECK(!ferror(asset));fclose(asset);
            CHECK(!rf_geomod_template_load("build/data/geomod-template.bin",&shape));
            CHECK(shape.face_count==count && shape.radius>0 && shape.kernel[0]==0 && shape.kernel[1]==0 && shape.kernel[2]==0);
            for(i=0;i<count*3;i++) {
                for(j=0;j<3;j++)CHECK(fabs(shape.vertices[i].position[j]*10-vertices[0][i].position[j])<1e-7);
                CHECK(!memcmp(shape.vertices[i].uv,vertices[0][i].uv,8));
            }
            saved=shape;decoded=saved;
            for(length=0;length<bytes;length++) {
                CHECK(rf_geomod_template_decode(packed,length,&decoded)!=RF_OK);
                CHECK(!memcmp(&decoded,&saved,sizeof(saved)));
            }
            packed[bytes]=0;CHECK(rf_geomod_template_decode(packed,(uint32_t)bytes+1,&decoded)==RF_FORMAT);
            packed[0]^=1;CHECK(rf_geomod_template_decode(packed,(uint32_t)bytes,&decoded)==RF_FORMAT);packed[0]^=1;
            packed[15]=0x7f;packed[14]=0xc0; /* Nonfinite source radius. */
            CHECK(rf_geomod_template_decode(packed,(uint32_t)bytes,&decoded)==RF_FORMAT);
            CHECK(!memcmp(&decoded,&saved,sizeof(saved)));
        }

        for(i=0;i<2;i++){uint16_t edges[96];cutters[i]=(rf_geomod_mesh_view){vertices[i],faces,count*3,count,0};CHECK(!rf_geomod_seed_adjacency(cutters+i,edges,96));}
        for(cavity=0;cavity<2;cavity++) {
            box((float[3]){-4,-4,-4},(float[3]){4,4,cavity?0:4},planes,source_v);
            for(i=0;i<6;i++) {
                source_f[i]=(rf_geomod_face){i*4,4,9,i};
                if(cavity)for(j=0;j<2;j++){rf_geomod_vertex temp=source_v[i][j];source_v[i][j]=source_v[i][3-j];source_v[i][3-j]=temp;}
            }
            source=(rf_geomod_mesh_view){source_v[0],source_f,24,6,0};
            CHECK(!rf_geomod_storage_open(&source,4096,512,1024*1024,&owner));
            CHECK(!rf_geomod_terrain_open(&source,original_filters,&generated,cavity,4096,512,1024*1024,&terrain));
            for(repeat=1;repeat<=2;repeat++) {
                double total=0;rf_collision_tree tree={0};uint32_t x,y;
                if(cavity){provenance_result=-1;rf_geomod_observe_compaction(observe_clipping_provenance,&work);}
                {int status=rf_geomod_storage_prepare_star_cuts(owner,cutters,kernels,repeat,cavity,&work);if(status)fprintf(stderr,"original star status%d cavity%u repeat%u\n",status,cavity,repeat);CHECK(!status);}
                rf_geomod_observe_compaction(NULL,NULL);
                if(cavity)CHECK(provenance_result==0);
                {
                    uint32_t c,f,e,g,h,k,paired=0;
                    for(c=0;c<repeat;c++)for(f=0;f<count;f++)for(e=0;e<3;e++)for(g=f+1;g<count;g++)for(h=0;h<3;h++) {
                        const rf_geomod_vertex *a=cutters[c].vertices+f*3,*b=cutters[c].vertices+g*3;
                        if(memcmp(a[e].position,b[(h+1)%3].position,12) || memcmp(a[(e+1)%3].position,b[h].position,12))continue;
                        for(k=0;k<4;k++)CHECK(work.star_planes[c][f][e+1][k]==-work.star_planes[c][g][h+1][k]);
                        paired++;
                    }
                    CHECK(paired==repeat*count*3/2);
                }
                CHECK(!rf_geomod_storage_pending(owner,&pending));surface_count=polygon_count=0;


                CHECK(!rf_geomod_terrain_cut_star(terrain,cutters+repeat-1,kernels[repeat-1]));
                CHECK(!rf_geomod_terrain_get(terrain,&live));
                CHECK(live.cuts==repeat && live.mesh.vertex_count==pending.vertex_count && live.mesh.face_count==pending.face_count);
                CHECK(!memcmp(live.mesh.vertices,pending.vertices,pending.vertex_count*sizeof(rf_geomod_vertex)));
                CHECK(!memcmp(live.mesh.faces,pending.faces,pending.face_count*sizeof(rf_geomod_face)));
                for(i=0;i<pending.face_count;i++) {
                    const rf_geomod_face *f=pending.faces+i;total+=volume(pending.vertices+f->first,f->count);
                    CHECK(record(pending.vertices+f->first,f->count));
                }
                printf("Original star cavity%u cuts%u: %u vertices, %u faces\n",cavity,repeat,pending.vertex_count,pending.face_count);
                CHECK(closed());if(!cavity && repeat==1)CHECK(fabs(total-(512-removed))<1e-4);
                CHECK(!rf_geomod_collision_faces(&pending,filters,positions,4096,bound,512));
                CHECK(!rf_collision_tree_open(bound,pending.face_count,1024*1024,&tree));
                for(x=0;x<9;x++)for(y=0;y<9;y++) {
                    float start[3]={-.4f+x*.1f,-.4f+y*.1f,0},delta[3]={0,0,3};
                    double expected=-1;rf_collision_tree_hit hit;uint32_t matched,c;
                    for(c=0;c<repeat;c++)for(i=0;i<count;i++) {
                        const float *a=vertices[c][i*3].position,*b=vertices[c][i*3+1].position,*d=vertices[c][i*3+2].position;
                        double ux=b[0]-a[0],uy=b[1]-a[1],vx=d[0]-a[0],vy=d[1]-a[1];
                        double det=ux*vy-uy*vx,px=start[0]-a[0],py=start[1]-a[1],u,v,z;
                        if(det<=1e-10)continue;
                        u=(px*vy-py*vx)/det;v=(ux*py-uy*px)/det;
                        if(u< -1e-6 || v< -1e-6 || u+v>1.000001)continue;
                        z=a[2]+u*(b[2]-a[2])+v*(d[2]-a[2]);if(z>expected)expected=z;
                    }
                    CHECK(expected>0);
                    CHECK(!rf_collision_thin_tree(tree.nodes,tree.node_count,tree.faces,tree.face_count,0,
                        start,delta,1,tree.stack,tree.node_capacity,&hit,&matched));
                    CHECK(matched && fabs(hit.hit.fraction-expected/3)<1e-5);
                    CHECK(!rf_collision_thin_tree(live.tree->nodes,live.tree->node_count,live.tree->faces,live.tree->face_count,0,
                        start,delta,1,live.tree->stack,live.tree->node_capacity,&hit,&matched));
                    CHECK(matched && fabs(hit.hit.fraction-expected/3)<1e-5);
                }
                rf_collision_tree_close(&tree);CHECK(!rf_geomod_storage_commit(owner));
            }
            before=live;kernels[0][0]=100;
            CHECK(rf_geomod_terrain_cut_star(terrain,cutters,kernels[0])==RF_FORMAT);kernels[0][0]=0;
            CHECK(!rf_geomod_terrain_get(terrain,&live) && live.cuts==before.cuts && live.mesh.generation==before.mesh.generation);
            CHECK(!rf_geomod_terrain_cut_box(terrain,(float[3]){0,0,0},(float[3]){.1f,.1f,.1f},77));
            CHECK(!rf_geomod_terrain_get(terrain,&live) && live.cuts==3);
            CHECK(!rf_geomod_terrain_reset(terrain));
            CHECK(!rf_geomod_terrain_get(terrain,&live) && !live.cuts && live.mesh.face_count==6);
            /* Reusing a former star slot for a convex cut must clear its type. */
            CHECK(!rf_geomod_terrain_cut_box(terrain,(float[3]){0,0,0},(float[3]){.1f,.1f,.1f},77));
            CHECK(!rf_geomod_terrain_get(terrain,&live) && live.cuts==1);
            {
                rf_geomod_template shape;float basis[9]={0,0,-1,0,1,0,1,0,0},center[3]={.3f,0,0};
                CHECK(!rf_geomod_template_load("build/data/geomod-template.bin",&shape));
                CHECK(!rf_geomod_terrain_reset(terrain));
                CHECK(!rf_geomod_terrain_cut_template(terrain,&shape,center,basis,shape.radius*10,77));
                CHECK(!rf_geomod_terrain_get(terrain,&before) && before.cuts==1);
                {
                    static rf_geomod_vertex saved_vertices[4096];static rf_geomod_face saved_faces[512];
                    uint32_t vc=before.mesh.vertex_count,fc=before.mesh.face_count,generated_count=0;
                    memcpy(saved_vertices,before.mesh.vertices,vc*sizeof(*saved_vertices));
                    memcpy(saved_faces,before.mesh.faces,fc*sizeof(*saved_faces));
                    CHECK(rf_geomod_terrain_set_mapping(terrain,256,128)==RF_RANGE);
                    CHECK(!rf_geomod_terrain_reset(terrain));
                    CHECK(rf_geomod_terrain_set_mapping(terrain,0,128)==RF_RANGE);
                    CHECK(!rf_geomod_terrain_set_mapping(terrain,256,128));
                    CHECK(!rf_geomod_terrain_cut_template(terrain,&shape,center,basis,shape.radius*10,77));
                    CHECK(!rf_geomod_terrain_get(terrain,&before));
                    CHECK(before.mesh.vertex_count==vc && before.mesh.face_count==fc);
                    CHECK(!memcmp(before.mesh.faces,saved_faces,fc*sizeof(*saved_faces)));
                    for(i=0;i<fc;i++) {
                        const rf_geomod_face *face=before.mesh.faces+i;
                        for(j=0;j<face->count;j++) {
                            uint32_t index=face->first+j;const rf_geomod_vertex *v=before.mesh.vertices+index;
                            CHECK(!memcmp(v->position,saved_vertices[index].position,12));
                            if(face->source_face==UINT32_MAX) {
                                float expected[2];CHECK(!rf_geomod_planar_uv(before.faces[i].plane,v->position,256,128,expected));
                                CHECK(!memcmp(v->uv,expected,8));generated_count++;
                            } else CHECK(!memcmp(v->uv,saved_vertices[index].uv,8));
                        }
                    }
                    CHECK(generated_count>0);
                }

                basis[0]=2;CHECK(rf_geomod_terrain_cut_template(terrain,&shape,center,basis,1,77)==RF_FORMAT);
                CHECK(!rf_geomod_terrain_get(terrain,&live) && live.mesh.generation==before.mesh.generation && live.cuts==1);
                basis[0]=0;center[0]=NAN;CHECK(rf_geomod_terrain_cut_template(terrain,&shape,center,basis,1,77)==RF_FORMAT);
                CHECK(!rf_geomod_terrain_get(terrain,&live) && live.mesh.generation==before.mesh.generation);
            }
            rf_geomod_terrain_close(&terrain);rf_geomod_storage_close(&owner);
        }
        {
            rf_geomod_template shape;rf_random_state random={1};double previous_volume=0;unsigned junction_misses=0;
            rf_geomod_terrain *uncached=NULL,*limited=NULL;rf_geomod_terrain_view reference;
            const char *trace_cut=getenv("RF_GEOMOD_INTERSECTION_CUT");unsigned trace_index=0,stress_count=6;
            const char *stress=getenv("RF_GEOMOD_STRESS_COUNT");
            if(stress){CHECK(stress[0]>='6' && stress[0]<='8' && !stress[1]);stress_count=(unsigned)(stress[0]-'0');}
            if(trace_cut){CHECK(trace_cut[0]>='1' && trace_cut[0]<='8' && !trace_cut[1]);trace_index=(unsigned)(trace_cut[0]-'1');CHECK(trace_index<stress_count);}
            const float start[3]={0,-10,12},delta[3]={-40,0,-20};
            CHECK(!rf_geomod_template_load("build/data/geomod-template.bin",&shape));
            box((float[3]){-16,-12,-20},(float[3]){16,12,20},planes,source_v);
            for(i=0;i<6;i++) {
                source_f[i]=(rf_geomod_face){i*4,4,9,i};
                for(j=0;j<2;j++){rf_geomod_vertex temp=source_v[i][j];source_v[i][j]=source_v[i][3-j];source_v[i][3-j]=temp;}
            }
            source=(rf_geomod_mesh_view){source_v[0],source_f,24,6,0};
            CHECK(!rf_geomod_terrain_open(&source,original_filters,&generated,1,4096,stress_count>6?800:768,1024*1024,&terrain));
            CHECK(!rf_geomod_terrain_open(&source,original_filters,&generated,1,4096,801,1024*1024,&uncached));
            if(stress_count==8)CHECK(!rf_geomod_terrain_open(&source,original_filters,&generated,1,4096,768,1024*1024,&limited));
            report_closure=getenv("RF_GEOMOD_CLOSURE_ALL")?2:1;
            {uint16_t edges[24];CHECK(!rf_geomod_seed_adjacency(&source,edges,24));}
            for(repeat=0;repeat<stress_count;repeat++) {
                rf_collision_tree_hit hit;uint32_t matched;float basis[9];double total=0;
                CHECK(!rf_geomod_terrain_get(terrain,&live));
                CHECK(!rf_collision_thin_tree(live.tree->nodes,live.tree->node_count,live.tree->faces,live.tree->face_count,
                    0,start,delta,1,live.tree->stack,live.tree->node_capacity,&hit,&matched) && matched);
                CHECK(!rf_geomod_random_basis(&random,basis));
                {
                    FILE *trace=NULL,*compact_trace=NULL;const char *path=getenv("RF_GEOMOD_INTERSECTION_TRACE");
                    if(repeat==trace_index && getenv("RF_GEOMOD_COMPACTION_TRACE")) {
                        compact_trace=fopen(getenv("RF_GEOMOD_COMPACTION_TRACE"),"wb");CHECK(compact_trace);
                        rf_geomod_observe_compaction(trace_compaction,compact_trace);
                    }
                    if(repeat==trace_index && path){trace=fopen(path,"wb");CHECK(trace);CHECK(fwrite("RFI1",4,1,trace)==1);rf_geomod_observe_intersections(trace_intersection,trace);}
                    CHECK(!rf_geomod_terrain_cut_template(terrain,&shape,hit.hit.point,basis,3.75f,77));
                    rf_geomod_observe_intersections(NULL,NULL);
                    rf_geomod_observe_compaction(NULL,NULL);
                    if(compact_trace)CHECK(!fclose(compact_trace));
                    if(trace)CHECK(!fclose(trace));
                }
                if(limited) {
                    if(repeat<7)CHECK(!rf_geomod_terrain_cut_template(limited,&shape,hit.hit.point,basis,3.75f,77));
                    else {
                        rf_geomod_terrain_view before,after;
                        static rf_geomod_vertex kept_vertices[4096];static rf_geomod_face kept_faces[768];
                        CHECK(!rf_geomod_terrain_get(limited,&before));
                        memcpy(kept_vertices,before.mesh.vertices,before.mesh.vertex_count*sizeof(*kept_vertices));
                        memcpy(kept_faces,before.mesh.faces,before.mesh.face_count*sizeof(*kept_faces));
                        CHECK(rf_geomod_terrain_cut_template(limited,&shape,hit.hit.point,basis,3.75f,77)==RF_RANGE);
                        CHECK(!rf_geomod_terrain_get(limited,&after));
                        CHECK(after.cuts==7 && after.mesh.generation==before.mesh.generation);
                        CHECK(after.mesh.vertex_count==before.mesh.vertex_count && after.mesh.face_count==before.mesh.face_count);
                        CHECK(!memcmp(kept_vertices,after.mesh.vertices,after.mesh.vertex_count*sizeof(*kept_vertices)));
                        CHECK(!memcmp(kept_faces,after.mesh.faces,after.mesh.face_count*sizeof(*kept_faces)));
                        CHECK(!terrain_ray_coverage(&after,8));
                        puts("PASS: repaired eighth-cut face overflow preserves seven-cut live geometry and collision");
                    }
                }
                CHECK(!rf_geomod_terrain_cut_template(uncached,&shape,hit.hit.point,basis,3.75f,77));
                CHECK(!rf_geomod_terrain_get(uncached,&reference));
                CHECK(!rf_geomod_terrain_get(terrain,&live) && live.cuts==repeat+1 && live.peak_bytes<=1024*1024);
                CHECK(reference.mesh.vertex_count==live.mesh.vertex_count && reference.mesh.face_count==live.mesh.face_count);
                CHECK(!memcmp(reference.mesh.vertices,live.mesh.vertices,live.mesh.vertex_count*sizeof(*live.mesh.vertices)));
                CHECK(!memcmp(reference.mesh.faces,live.mesh.faces,live.mesh.face_count*sizeof(*live.mesh.faces)));
                if(repeat==trace_index && getenv("RF_GEOMOD_MESH_TRACE")) {
                    FILE *mesh_file=fopen(getenv("RF_GEOMOD_MESH_TRACE"),"wb");CHECK(mesh_file);
                    fprintf(mesh_file,"face,corner,source,nx,ny,nz,d,x,y,z,u,v\n");
                    for(i=0;i<live.mesh.face_count;i++)for(j=0;j<live.mesh.faces[i].count;j++) {
                        const rf_geomod_vertex *v=live.mesh.vertices+live.mesh.faces[i].first+j;const float *p=live.faces[i].plane;
                        CHECK(fprintf(mesh_file,"%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
                            i,j,live.mesh.faces[i].source_face,p[0],p[1],p[2],p[3],v->position[0],v->position[1],v->position[2],v->uv[0],v->uv[1])>0);
                    }
                    CHECK(!fclose(mesh_file));
                }
                surface_count=polygon_count=0;
                for(i=0;i<live.mesh.face_count;i++) {
                    const rf_geomod_face *f=live.mesh.faces+i;
                    CHECK(record(live.mesh.vertices+f->first,f->count));total+=volume(live.mesh.vertices+f->first,f->count);
                }
                {int closure=closed();CHECK(closure);
                 printf("STRESS %u %u %u %g prior %g closed %d\n",repeat,live.mesh.vertex_count,live.mesh.face_count,-total,previous_volume,closure);}
                printf("STRESS_MEMORY cut%u resident%u peak%u\n",repeat+1,live.resident_bytes,live.peak_bytes);
                /* Closure is required after every live repaired admission. */
                {
                    /* Probe individual float steps across the known near-coincident
                     * junction. A closed room must stop every outward segment,
                     * whether at the original wall or the new crater boundary. */
                    int y,z;unsigned probes=0,misses=0,body_misses=0,reference_hits=0,direct_hits=0;
                    const float outward[3]={-15,0,0};
                    for(y=-32;y<=32;y++)for(z=-32;z<=32;z++) {
                        float probe[3]={-15,-7.36841655756f+y*0x1p-21f,2.93492785774f+z*0x1p-22f};
                        CHECK(!rf_collision_thin_tree(live.tree->nodes,live.tree->node_count,
                            live.tree->faces,live.tree->face_count,0,probe,outward,1,
                            live.tree->stack,live.tree->node_capacity,&hit,&matched));
                        probes++;if(!matched) {
                            rf_collision_sweep_tree_hit sweep;
                            unsigned face_index;rf_collision_ray_hit direct;
                            misses++;
                            {
                                unsigned reference=junction_reference(&live.mesh,probe);
                                if(reference)reference_hits++;
                                if(reference && misses==1) {
                                    const rf_collision_face *face=live.faces+reference-1;
                                    float end[3]={probe[0]-15,probe[1],probe[2]},point[3],fraction;unsigned box_hit,plane_hit,inside;
                                    CHECK(!rf_collision_segment_box(face->minimum,face->maximum,probe,end,point,&box_hit));
                                    CHECK(!rf_collision_segment_plane(probe,outward,face->plane,&fraction,&plane_hit));
                                    point[0]=probe[0]+outward[0]*fraction;point[1]=probe[1];point[2]=probe[2];
                                    CHECK(!rf_collision_polygon_contains(face->plane,point,face->vertices,face->count,&inside));
                                    printf("JUNCTION_STAGE face %u probe %.9g %.9g %.9g box %u plane %u fraction %.9g inside %u normal %.9g %.9g %.9g\n",
                                        reference-1,probe[0],probe[1],probe[2],box_hit,plane_hit,fraction,inside,face->plane[0],face->plane[1],face->plane[2]);
                                }
                            }
                            for(face_index=0;face_index<live.tree->face_count;face_index++) {
                                CHECK(!rf_collision_thin_face(live.tree->faces+face_index,probe,outward,1,&direct,&matched));
                                if(matched){direct_hits++;break;}
                            }
                            CHECK(!rf_collision_sweep_tree(live.tree->nodes,live.tree->node_count,
                                live.tree->faces,live.tree->face_count,0,probe,outward,outward,.5f,1,
                                live.tree->stack,live.tree->node_capacity,&sweep,&matched));
                            if(!matched)body_misses++;
                        }
                    }
                    printf("JUNCTION_RAYS cut %u probes %u misses %u body_misses %u\n",repeat+1,probes,misses,body_misses);
                    printf("JUNCTION_REFERENCE cut %u double_hits %u direct_hits %u\n",repeat+1,reference_hits,direct_hits);
                    junction_misses+=misses;CHECK(!body_misses);
                    CHECK(reference_hits==misses); /* These misses have mesh intersections. */
                }
                {
                    unsigned samples=0;
                    for(i=0;i<live.mesh.face_count;i++) {
                        const rf_geomod_face *face=live.mesh.faces+i;rf_geomod_light_grid grid;
                        unsigned x,y;float sample[3];
                        if(face->source_face!=UINT32_MAX)continue;
                        CHECK(!rf_geomod_light_grid_open(live.mesh.vertices+face->first,face->count,live.faces[i].plane,.5f,&grid));
                        for(y=0;y<grid.height;y++)for(x=0;x<grid.width;x++) {
                            double distance=grid.plane[3];unsigned k;
                            CHECK(!rf_geomod_light_grid_sample(&grid,live.mesh.vertices+face->first,face->count,x,y,sample));
                            for(k=0;k<3;k++)distance+=(double)grid.plane[k]*sample[k];
                            CHECK(fabs(distance)<1e-5);
                            for(k=0;k<face->count;k++) {
                                const float *a=live.mesh.vertices[face->first+k].position;
                                const float *b=live.mesh.vertices[face->first+(k+1)%face->count].position;
                                double edge[3],q[3],side=0,length=0;unsigned c;
                                for(c=0;c<3;c++){edge[c]=(double)b[c]-a[c];q[c]=(double)sample[c]-a[c];length+=edge[c]*edge[c];}
                                for(c=0;c<3;c++)side+=grid.plane[c]*(edge[(c+1)%3]*q[(c+2)%3]-edge[(c+2)%3]*q[(c+1)%3]);
                                CHECK(side>=-1e-5*sqrt(length));
                            }
                            samples++;
                        }
                    }
                    printf("CRATER_LIGHT_GRID cut %u samples %u\n",repeat+1,samples);CHECK(samples>0);
                }
                CHECK(!terrain_ray_coverage(&live,repeat+1));
                CHECK(-total>previous_volume);previous_volume=-total;
            }
            rf_geomod_terrain_close(&terrain);
            rf_geomod_terrain_close(&uncached);
            rf_geomod_terrain_close(&limited);
            report_closure=0;
            CHECK(!junction_misses);
            printf("PASS: %u ray-placed crater admissions and increasing signed volume within1MiB; closed room-scale edges verified\n",stress_count);
        }
        puts("PASS: original concave template, overlapping cuts, closed edges, volume and324 independent triangle rays");
    }
    {
        rf_geomod_vertex vertices[4]={{{-1,-1,3},{0,0}},{{-1,1,3},{0,1}},{{1,1,3},{1,1}},{{1,-1,3},{1,0}}};
        rf_geomod_face face={0,4,0,UINT32_MAX};rf_geomod_mesh_view source={vertices,&face,4,1,0};
        rf_collision_face_filter filter={0};rf_collision_face bound;float positions[4][3],colors[1][3]={{.2f,.4f,.6f}};
        rf_preview_vertex projected[64],saved[64];rf_preview_mesh draw={projected,0,0};rf_level camera={0};
        rf_pc_raster raster={0};unsigned char pixel[4]={200,160,80,255};rf_material item={0};rf_materials materials={0};rf_lightmaps maps={0};
        uint32_t center=(240*640+320)*3,i;
        CHECK(!rf_geomod_collision_faces(&source,&filter,positions,4,&bound,1));
        {
            rf_collision_tree tree={0};rf_geomod_terrain_view terrain={0};uint32_t visible=77;
            float light[3]={0,0,0},sample[3]={0,0,3};
            CHECK(!rf_collision_tree_open(&bound,1,65536,&tree));terrain.tree=&tree;
            CHECK(!rf_geomod_light_visible(&terrain,light,sample,&visible) && visible==1);
            sample[2]=4;CHECK(!rf_geomod_light_visible(&terrain,light,sample,&visible) && visible==0);
            sample[0]=4;CHECK(!rf_geomod_light_visible(&terrain,light,sample,&visible) && visible==1);
            sample[0]=NAN;visible=77;
            CHECK(rf_geomod_light_visible(&terrain,light,sample,&visible)==RF_FORMAT && visible==77);
            rf_collision_tree_close(&tree);
        }
        for(i=0;i<3;i++)camera.player_orientation[i][i]=1;
        item.image.width=item.image.height=1;item.image.bytes=4;item.image.rgba=pixel;materials.items=&item;materials.count=materials.loaded=1;
        CHECK(!rf_pc_raster_open(&raster,1));
        CHECK(!rf_preview_geomod_lit(&draw,sizeof(projected),&source,&bound,1,&camera,colors));
        CHECK(draw.count==6 && projected[0].lightmap==RF_PREVIEW_VERTEX_LIT);
        CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
        CHECK(raster.rgb[center]==40 && raster.rgb[center+1]==64 && raster.rgb[center+2]==48);
        {
            uint32_t at=(240*640+330)*3;
            for(i=0;i<draw.count;i++)projected[i].lightmap=RF_PREVIEW_FADE_TAG|128u;
            CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
            CHECK(raster.rgb[at]==28 && raster.rgb[at+1]==40 && raster.rgb[at+2]==36);
            CHECK(raster.rgb[center]==28 && raster.rgb[center+1]==40 && raster.rgb[center+2]==36);
            CHECK(raster.depth[at/3]==16777216);
            for(i=0;i<draw.count;i++)projected[i].lightmap=RF_PREVIEW_FADE_TAG;
            CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
            CHECK(raster.rgb[at]==16 && raster.rgb[at+1]==16 && raster.rgb[at+2]==24);
            CHECK(raster.depth[at/3]==16777216);
            for(i=0;i<draw.count;i++)projected[i].lightmap=RF_PREVIEW_VERTEX_LIT;
            {
                uint32_t original=draw.count;float original_depth=projected[0].position[2];
                for(i=0;i<original;i++)projected[i].position[2]=100;
                memcpy(projected+original,projected,original*sizeof(*projected));
                for(i=original;i<original*2;i++) {
                    projected[i].position[2]=50;projected[i].lightmap=RF_PREVIEW_FADE_TAG|128u;
                    projected[i].color[0]=projected[i].color[1]=projected[i].color[2]=1;
                }
                draw.count*=2;draw.bytes=draw.count*sizeof(*projected);
                CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
                CHECK(raster.rgb[at]==120 && raster.rgb[at+1]==112 && raster.rgb[at+2]==64);
                CHECK(raster.depth[at/3]==100);
                for(i=original;i<original*2;i++)projected[i].position[2]=150;
                CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
                CHECK(raster.rgb[at]==40 && raster.rgb[at+1]==64 && raster.rgb[at+2]==48);
                for(i=0;i<original;i++)projected[i].position[2]=original_depth;
                draw.count=original;draw.bytes=original*sizeof(*projected);
            }
        }
        memcpy(saved,projected,sizeof(projected));colors[0][0]=NAN;
        CHECK(rf_preview_geomod_lit(&draw,sizeof(projected),&source,&bound,1,&camera,colors)==RF_FORMAT);
        CHECK(draw.count==6 && !memcmp(saved,projected,sizeof(projected)));
        CHECK(!rf_preview_geomod(&draw,sizeof(projected),&source,&bound,1,&camera));
        CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
        CHECK(raster.rgb[center]==200 && raster.rgb[center+1]==160 && raster.rgb[center+2]==80);
        {
            float corners[4][3]={{0,.5f,.25f},{0,.5f,.25f},{1,.5f,.25f},{1,.5f,.25f}};uint32_t clipped=0,count;
            vertices[0].position[0]=vertices[1].position[0]=-4;
            CHECK(!rf_geomod_collision_faces(&source,&filter,positions,4,&bound,1));
            CHECK(!rf_preview_geomod_vertex_lit(&draw,sizeof(projected),&source,&bound,1,&camera,corners));
            count=draw.count;CHECK(count>=6);
            for(i=0;i<count;i++) {
                float x=(projected[i].position[0]-320.f)*3.f/320.f;
                CHECK(projected[i].lightmap==RF_PREVIEW_VERTEX_LIT);
                CHECK(fabsf(projected[i].color[0]-(x+4.f)/5.f)<.0002f);
                CHECK(projected[i].color[1]==.5f && projected[i].color[2]==.25f);
                if(projected[i].position[0]==0){CHECK(fabsf(projected[i].color[0]-.2f)<1e-6f);clipped++;}
            }
            CHECK(clipped>0);
            CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
            CHECK(raster.rgb[center]>=159 && raster.rgb[center]<=161 && raster.rgb[center+1]==80 && raster.rgb[center+2]==20);
            memcpy(saved,projected,sizeof(projected));corners[3][2]=NAN;
            CHECK(rf_preview_geomod_vertex_lit(&draw,sizeof(projected),&source,&bound,1,&camera,corners)==RF_FORMAT);
            CHECK(draw.count==count && !memcmp(saved,projected,sizeof(projected)));
            {
                unsigned char data[160]={0},light_pixel[4]={64,32,128,255};uint32_t offset=0,word;
                rf_geometry authored={0};rf_image images[3]={{0}};float value;
                authored.data=data;authored.faces=1;authored.face_offsets=&offset;authored.mapping_offset=64;authored.mappings=1;
                word=2;memcpy(data+64,&word,4);word=1;memcpy(data+64+72,&word,4);
                value=.5f;memcpy(data+64+76,&value,4);memcpy(data+64+80,&value,4);
                value=.125f;memcpy(data+64+84,&value,4);value=.25f;memcpy(data+64+88,&value,4);
                face.source_face=0;corners[3][2]=.25f;
                images[2].width=images[2].height=1;images[2].bytes=4;images[2].rgba=light_pixel;
                maps.images=images;maps.count=3;
                CHECK(!rf_preview_geomod_world_lit(&draw,sizeof(projected),&source,&bound,1,&camera,corners,&authored));
                for(i=0;i<draw.count;i++) {
                    float x=(projected[i].position[0]-320.f)*3.f/320.f;
                    float y=(240.f-projected[i].position[1])*3.f/320.f;
                    CHECK(projected[i].lightmap==2);
                    CHECK(fabsf(projected[i].lightmap_texture[0]/projected[i].lightmap_texture[2]-(x*.125f+.5f))<.0002f);
                    CHECK(fabsf(projected[i].lightmap_texture[1]/projected[i].lightmap_texture[2]-(y*.25f+.5f))<.0002f);
                }
                CHECK(!rf_pc_raster_frame(&raster,&draw,&materials,&maps,draw.count));
                CHECK(raster.rgb[center]>=99 && raster.rgb[center]<=101 && raster.rgb[center+1]>=39 && raster.rgb[center+1]<=41 && raster.rgb[center+2]==80);
                memcpy(saved,projected,sizeof(projected));count=draw.count;word=3;memcpy(data+64+72,&word,4);
                CHECK(rf_preview_geomod_world_lit(&draw,sizeof(projected),&source,&bound,1,&camera,corners,&authored)==RF_FORMAT);
                CHECK(draw.count==count && !memcmp(saved,projected,sizeof(projected)));
                face.source_face=UINT32_MAX;
                CHECK(!rf_preview_geomod_world_lit(&draw,sizeof(projected),&source,&bound,1,&camera,corners,&authored));
                CHECK(projected[0].lightmap==RF_PREVIEW_VERTEX_LIT);
                maps.images=NULL;maps.count=0;
            }
        }
        rf_pc_raster_close(&raster);
        puts("PASS: textured face/corner lighting reaches pixels, clipped color gradients and invalid RGB rollback, legacy path unchanged");
    }
    {
        rf_geomod_debris_lifecycle value={12,34,56},saved=value;
        CHECK(rf_geomod_debris_age(NAN,2,1.f/60,0,&value)==RF_RANGE);
        CHECK(!memcmp(&value,&saved,sizeof(value)));
        CHECK(rf_geomod_debris_age(0,2,-1,0,&value)==RF_RANGE);
        CHECK(!memcmp(&value,&saved,sizeof(value)));
        CHECK(rf_geomod_debris_age(0,2,0,2,&value)==RF_RANGE);
        CHECK(!memcmp(&value,&saved,sizeof(value)));
        CHECK(!rf_geomod_debris_age(2,2,.5f,0,&value));
        CHECK(value.age==2.5f && !value.removed && value.alpha==127);
        CHECK(!rf_geomod_debris_age(3,2,.25f,0,&value));
        CHECK(value.age==3.25f && !value.removed && !value.alpha);
        CHECK(!rf_geomod_debris_age(value.age,2,0,1,&value));
        CHECK(value.removed && value.age==3.25f);
        puts("PASS: debris fade age boundaries, pause/removal order and invalid-input rollback");
    }
    {
        unsigned char pixels[30],saved[30];rf_random_state random={1};uint32_t x,y;
        memset(pixels,165,sizeof(pixels));memcpy(saved,pixels,sizeof(pixels));
        CHECK(rf_geomod_light_noise(pixels,14,9,2,2,&random)==RF_RANGE);
        CHECK(random.value==1 && !memcmp(pixels,saved,sizeof(pixels)));
        CHECK(rf_geomod_light_noise(pixels,sizeof(pixels),9,0,2,&random)==RF_RANGE);
        CHECK(random.value==1 && !memcmp(pixels,saved,sizeof(pixels)));
        CHECK(!rf_geomod_light_noise(pixels,sizeof(pixels),9,2,2,&random));
        for(y=0;y<2;y++) {
            for(x=0;x<2;x++)CHECK(pixels[y*9+x*3]>=32 && pixels[y*9+x*3]<=95 &&
                pixels[y*9+x*3]==pixels[y*9+x*3+1] && pixels[y*9+x*3]==pixels[y*9+x*3+2]);
            for(x=6;x<9;x++)CHECK(pixels[y*9+x]==165);
        }
        for(x=18;x<sizeof(pixels);x++)CHECK(pixels[x]==165);
        puts("PASS: new-face randomized lightmap fill, padding and invalid-input rollback");
    }
    {
        float span[2]={2.25f,20},density[2]={4,4},adjusted[2]={123,456};uint32_t dims[2]={77,88};
        CHECK(!rf_geomod_lightmap_size(span,density,0,dims,adjusted));
        CHECK(dims[0]==9 && dims[1]==64 && adjusted[0]==4 && adjusted[1]==3.2f);
        span[0]=span[1]=0;
        CHECK(!rf_geomod_lightmap_size(span,density,1,dims,adjusted));
        CHECK(dims[0]==8 && dims[1]==8 && adjusted[0]==32 && adjusted[1]==32);
        span[0]=NAN;
        CHECK(rf_geomod_lightmap_size(span,density,0,dims,adjusted)==RF_RANGE);
        CHECK(dims[0]==8 && dims[1]==8 && adjusted[0]==32 && adjusted[1]==32);
        puts("PASS: original non-power-of-two lightmap extents, clamps and rollback");
    }
    puts("PASS: repeated solid/cavity cuts, edge closure, materials, ray/body clearance, rendering and rollback");return 0;
}
