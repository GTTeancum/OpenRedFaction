#include "rf/geomod.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
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
static rf_geomod_vertex surface[2048];
static rf_geomod_fragment polygons[128];
static unsigned surface_count,polygon_count;
static int record(const rf_geomod_vertex *v,unsigned n)
{
    if(surface_count+n>2048 || polygon_count==128)return 0;
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
        double d[3],len=0,t[2050]={0,1};unsigned n=2;
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
            double mid[3];int matches=0,balance=0;
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
                    matches++;balance+=dot>0?1:-1;
                }
            }
            if(matches!=2 || balance)return 0;
        }
    }
    return 1;
}
int main(void)
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
        CHECK(!rf_geomod_storage_commit(storage));CHECK(!rf_geomod_storage_view(storage,&live) && live.generation==2);
        CHECK(rf_geomod_storage_prepare_convex_cut(storage,&cut,&work)==RF_FORMAT); /* Do not treat concave result as convex. */
        CHECK(!rf_geomod_storage_reset(storage));CHECK(!rf_geomod_storage_view(storage,&live) && live.face_count==6);
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
    puts("PASS: repeated/coplanar cuts, tunnel volumes, geometric edge closure, materials and rollback");return 0;
}
