/* Independent double-precision swept point versus finite segment capsule.
 * Tests the two endpoint spheres and interior cylinder; no runtime collision
 * code, cached planes, tree traversal or fixture-specific coordinates. */
static double lineage_segment_time(const float start[3],const float delta[3],const float a[3],const float b[3],double radius)
{
    double e[3],w[3],length=0,wd=0,dd=0,best=2;unsigned k,part;
    for(k=0;k<3;k++){e[k]=(double)b[k]-a[k];length+=e[k]*e[k];w[k]=(double)start[k]-a[k];wd+=w[k]*e[k];dd+=(double)delta[k]*e[k];}
    for(part=0;part<3;part++) {
        double q[3],v[3],aa=0,bb=0,cc=-radius*radius,disc,t,u;
        if(part==2 && !length)continue;
        for(k=0;k<3;k++) {
            q[k]=part==2?w[k]-e[k]*wd/length:(double)start[k]-(part?b[k]:a[k]);
            v[k]=part==2?(double)delta[k]-e[k]*dd/length:delta[k];
            aa+=v[k]*v[k];bb+=2*q[k]*v[k];cc+=q[k]*q[k];
        }
        if(aa<=1e-24)continue;disc=bb*bb-4*aa*cc;if(disc<0)continue;
        t=(-bb-sqrt(disc))/(2*aa);if(t<0 || t>1 || t>=best)continue;
        u=length?(wd+t*dd)/length:0;if(part==2 && (u<0 || u>1))continue;
        best=t;
    }
    return best;
}
static double lineage_edge_oracle(const rf_geomod_mesh_view *mesh,const float start[3],const float delta[3],double radius)
{
    double best=2;unsigned f,e;
    for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *p=mesh->faces+f;
        for(e=0;e<p->count;e++) {
            double t=lineage_segment_time(start,delta,mesh->vertices[p->first+e].position,
                mesh->vertices[p->first+(e+1)%p->count].position,radius);
            if(t<best)best=t;
        }
    }
    return best;
}
/* Compare actual collision queries, not polygon-count equality. */
static int lineage_collision_equal(const rf_geomod_mesh_view *a,const rf_geomod_mesh_view *b)
{
    static float positions[2][4096][3];static rf_collision_face faces[2][1024];
    static rf_collision_face_filter filters[1024];rf_collision_tree trees[2]={{0}};
    const rf_geomod_mesh_view *meshes[2]={a,b};unsigned axis,side,x,y,i,queries=0;int status=0;
    for(i=0;i<2;i++) {
        status=rf_geomod_collision_faces(meshes[i],filters,positions[i],4096,faces[i],1024);if(status)goto done;
        status=rf_collision_tree_open(faces[i],meshes[i]->face_count,1048576,trees+i);if(status)goto done;
    }
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++)for(x=0;x<11;x++)for(y=0;y<11;y++) {
        float start[3]={0,10,0},delta[3]={0},fraction[2]={0};uint32_t hit[2],face_id[2]={0},edge_id[2]={0};
        unsigned j=(axis+1)%3,k=(axis+2)%3;
        start[axis]+=(side?5.f:-5.f);delta[axis]=side?-10.f:10.f;
        start[j]+=-3.83f+.761f*x;start[k]+=-3.79f+.757f*y;
        for(i=0;i<2;i++) {
            rf_collision_tree *t=trees+i;rf_collision_tree_hit r;
            status=rf_collision_thin_tree(t->nodes,t->node_count,t->faces,t->face_count,0,start,delta,1,t->stack,t->node_capacity,&r,hit+i);
            if(status)goto done;if(hit[i])fraction[i]=r.hit.fraction;
        }
        if(hit[0]!=hit[1] || (hit[0] && fabsf(fraction[0]-fraction[1])>1e-5f)){printf("COLLISION_DELTA query%u axis%u side%u start %.9g %.9g %.9g hit%u/%u fraction%.9g/%.9g\n",queries,axis,side,start[0],start[1],start[2],hit[0],hit[1],fraction[0],fraction[1]);status=RF_FORMAT;goto done;}
        queries++;
        for(i=0;i<2;i++) {
            rf_collision_tree *t=trees+i;rf_collision_sweep_tree_hit r;
            status=rf_collision_sweep_tree(t->nodes,t->node_count,t->faces,t->face_count,0,start,delta,delta,.1f,1,t->stack,t->node_capacity,&r,hit+i);
            if(status)goto done;if(hit[i])fraction[i]=r.hit.fraction;if(hit[i]){face_id[i]=t->source_indices[r.face_index];edge_id[i]=r.edge;}
        }
        if(hit[0]!=hit[1] || (hit[0] && fabsf(fraction[0]-fraction[1])>1e-5f)) {
            double expected[2]={lineage_edge_oracle(a,start,delta,.1f),lineage_edge_oracle(b,start,delta,.1f)};
            printf("EDGE_ORACLE baseline %.12g replay %.12g actual %.9g/%.9g\n",expected[0],expected[1],fraction[0],fraction[1]);
            /* This oracle is two-sided. Runtime plane admission is one-sided;
             * its diagnostic value must never waive a collision mismatch. */
            status=RF_FORMAT;goto done;
        }
        queries++;
    }
done:
    printf("SOLID_COLLISION_COMPARISON queries=%u status=%d tolerance=1e-5\n",queries,status);
    rf_collision_tree_close(trees);rf_collision_tree_close(trees+1);return status;
}
