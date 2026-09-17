/* Test-only geometric edge coverage; copied from the established interior
 * fixture. T-junction subdivision uses1e-6 distance, never welds production. */
#ifndef SNAPSHOT_VERTICES
#define SNAPSHOT_VERTICES 4096
#endif
#ifndef SNAPSHOT_FACES
#define SNAPSHOT_FACES 1024
#endif
static rf_geomod_vertex surface[SNAPSHOT_VERTICES];
static rf_geomod_fragment polygons[SNAPSHOT_FACES];
static unsigned surface_count,polygon_count;
static int report_closure;
static int lineage_closed_impl(void)
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
static int lineage_closed(const rf_geomod_mesh_view *mesh)
{
    unsigned i;if(mesh->vertex_count>SNAPSHOT_VERTICES || mesh->face_count>SNAPSHOT_FACES)return 0;
    surface_count=mesh->vertex_count;polygon_count=mesh->face_count;
    memcpy(surface,mesh->vertices,surface_count*sizeof(*surface));
    for(i=0;i<polygon_count;i++)polygons[i]=(rf_geomod_fragment){mesh->faces[i].first,mesh->faces[i].count};
    return lineage_closed_impl();
}
