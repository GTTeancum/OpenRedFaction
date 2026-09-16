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
        float start[3]={0,10,0},delta[3]={0},fraction[2]={0};uint32_t hit[2];
        unsigned j=(axis+1)%3,k=(axis+2)%3;
        start[axis]+=(side?5.f:-5.f);delta[axis]=side?-10.f:10.f;
        start[j]+=-3.83f+.761f*x;start[k]+=-3.79f+.757f*y;
        for(i=0;i<2;i++) {
            rf_collision_tree *t=trees+i;rf_collision_tree_hit r;
            status=rf_collision_thin_tree(t->nodes,t->node_count,t->faces,t->face_count,0,start,delta,1,t->stack,t->node_capacity,&r,hit+i);
            if(status)goto done;if(hit[i])fraction[i]=r.hit.fraction;
        }
        if(hit[0]!=hit[1] || (hit[0] && fabsf(fraction[0]-fraction[1])>1e-5f)){status=RF_FORMAT;goto done;}
        queries++;
        for(i=0;i<2;i++) {
            rf_collision_tree *t=trees+i;rf_collision_sweep_tree_hit r;
            status=rf_collision_sweep_tree(t->nodes,t->node_count,t->faces,t->face_count,0,start,delta,delta,.1f,1,t->stack,t->node_capacity,&r,hit+i);
            if(status)goto done;if(hit[i])fraction[i]=r.hit.fraction;
        }
        if(hit[0]!=hit[1] || (hit[0] && fabsf(fraction[0]-fraction[1])>1e-5f)){status=RF_FORMAT;goto done;}
        queries++;
    }
done:
    printf("SOLID_COLLISION_COMPARISON queries=%u status=%d tolerance=1e-5\n",queries,status);
    rf_collision_tree_close(trees);rf_collision_tree_close(trees+1);return status;
}
