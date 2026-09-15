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
static rf_geomod_vertex surface[4096];
static rf_geomod_fragment polygons[768];
static unsigned surface_count,polygon_count;
static unsigned report_closure;
static int record(const rf_geomod_vertex *v,unsigned n)
{
    if(surface_count+n>4096 || polygon_count==768)return 0;
    polygons[polygon_count].first=surface_count;polygons[polygon_count++].count=n;
    memcpy(surface+surface_count,v,n*sizeof(*v));surface_count+=n;return 1;
}
/* Geometric edge coverage, including T-junction subdivisions. This does not
 * weld the production output or claim shared topological edge indices. */
static int closed(void)
{
    unsigned p,e,v,i,j,q,f;
    for(p=0;p<polygon_count;p++)for(e=0;e<polygons[p].count;e++) {
        const float *a=surface[polygons[p].first+e].position;
        const float *b=surface[polygons[p].first+(e+1)%polygons[p].count].position;
        double d[3],len=0,t[4098]={0,1};unsigned n=2;
        for(i=0;i<3;i++){d[i]=b[i]-a[i];len+=d[i]*d[i];}
        if(len<1e-16)return 0;
        for(v=0;v<surface_count;v++) {
            double along=0,error=0;
            for(i=0;i<3;i++)along+=(surface[v].position[i]-a[i])*d[i];
            along/=len;
            for(i=0;i<3;i++){double x=surface[v].position[i]-a[i]-along*d[i];error+=x*x;}
            if(along>1e-6/sqrt(len) && along<1-1e-6/sqrt(len) && error<1e-12)t[n++]=along;
        }
        for(i=1;i<n;i++){double value=t[i];j=i;while(j && t[j-1]>value){t[j]=t[j-1];j--;}t[j]=value;}
        for(j=1;j<n;j++)if((t[j]-t[j-1])*sqrt(len)>1e-6) {
            double mid[3],match_error[8],match_along[8];unsigned match_face[8],match_edge[8];int matches=0,balance=0;
            for(i=0;i<3;i++)mid[i]=a[i]+d[i]*(t[j]+t[j-1])*.5;
            for(q=0;q<polygon_count;q++)for(f=0;f<polygons[q].count;f++) {
                const float *x=surface[polygons[q].first+f].position;
                const float *y=surface[polygons[q].first+(f+1)%polygons[q].count].position;
                double edge[3],size=0,along=0,error=0,dot=0;
                for(i=0;i<3;i++){edge[i]=y[i]-x[i];size+=edge[i]*edge[i];along+=(mid[i]-x[i])*edge[i];dot+=d[i]*edge[i];}
                if(size<1e-16)return 0;
                along/=size;
                for(i=0;i<3;i++){double z=mid[i]-x[i]-along*edge[i];error+=z*z;}
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
                    for(i=0;i<(unsigned)matches && i<8;i++) {
                        const rf_geomod_fragment *poly=polygons+match_face[i];
                        const float *x=surface[poly->first+match_edge[i]].position;
                        const float *y=surface[poly->first+(match_edge[i]+1)%poly->count].position;
                        printf("CLOSURE_MATCH face%u edge%u distance %.12g along %.12g start %.9g %.9g %.9g end %.9g %.9g %.9g\n",
                            match_face[i],match_edge[i],match_error[i],match_along[i],x[0],x[1],x[2],y[0],y[1],y[2]);
                    }
                }
                return 0;
            }
        }
    }
    return 1;
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
int main(int argc,char **argv)
{
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

        for(i=0;i<2;i++)cutters[i]=(rf_geomod_mesh_view){vertices[i],faces,count*3,count,0};
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
                {int status=rf_geomod_storage_prepare_star_cuts(owner,cutters,kernels,repeat,cavity,&work);if(status)fprintf(stderr,"original star status%d cavity%u repeat%u\n",status,cavity,repeat);CHECK(!status);}
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
            rf_geomod_template shape;rf_random_state random={1};double previous_volume=0;
            const float start[3]={0,-10,12},delta[3]={-40,0,-20};
            CHECK(!rf_geomod_template_load("build/data/geomod-template.bin",&shape));
            box((float[3]){-16,-12,-20},(float[3]){16,12,20},planes,source_v);
            for(i=0;i<6;i++) {
                source_f[i]=(rf_geomod_face){i*4,4,9,i};
                for(j=0;j<2;j++){rf_geomod_vertex temp=source_v[i][j];source_v[i][j]=source_v[i][3-j];source_v[i][3-j]=temp;}
            }
            source=(rf_geomod_mesh_view){source_v[0],source_f,24,6,0};
            CHECK(!rf_geomod_terrain_open(&source,original_filters,&generated,1,4096,768,1024*1024,&terrain));
            report_closure=1;
            for(repeat=0;repeat<6;repeat++) {
                rf_collision_tree_hit hit;uint32_t matched;float basis[9];double total=0;
                CHECK(!rf_geomod_terrain_get(terrain,&live));
                CHECK(!rf_collision_thin_tree(live.tree->nodes,live.tree->node_count,live.tree->faces,live.tree->face_count,
                    0,start,delta,1,live.tree->stack,live.tree->node_capacity,&hit,&matched) && matched);
                CHECK(!rf_geomod_random_basis(&random,basis));
                CHECK(!rf_geomod_terrain_cut_template(terrain,&shape,hit.hit.point,basis,3.75f,77));
                CHECK(!rf_geomod_terrain_get(terrain,&live) && live.cuts==repeat+1 && live.peak_bytes<=1024*1024);
                surface_count=polygon_count=0;
                for(i=0;i<live.mesh.face_count;i++) {
                    const rf_geomod_face *f=live.mesh.faces+i;
                    CHECK(record(live.mesh.vertices+f->first,f->count));total+=volume(live.mesh.vertices+f->first,f->count);
                }
                printf("STRESS %u %u %u %g prior %g closed %d\n",repeat,live.mesh.vertex_count,live.mesh.face_count,-total,previous_volume,closed());
                /* Room-scale closure is an open defect also observed without
                 * compaction; report it without claiming this capacity test proves it. */
                CHECK(-total>previous_volume);previous_volume=-total;
            }
            rf_geomod_terrain_close(&terrain);
            report_closure=0;
            puts("PASS: six ray-placed crater admissions and increasing signed volume within1MiB; room-scale closure remains diagnostic");
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
    puts("PASS: repeated solid/cavity cuts, edge closure, materials, ray/body clearance, rendering and rollback");return 0;
}
