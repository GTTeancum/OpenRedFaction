static int scene_fragment_mover_metadata_unused(void *context,uint32_t solid,uint32_t face,
    uint32_t *texture,uint32_t *material)
{(void)context;(void)solid;(void)face;*texture=*material=0;return RF_OK;}
/* Translational relative path in the final mover frame. Existing fragment
 * corner, reciprocal-vertex and edge geometry remains shared. Contact points
 * are mapped back to the actual contact-time world pose (port policy). */
static int scene_fragment_moving_sweep(const rf_geomod_mesh_view *mesh,const rf_physics_body_state *body,
    const rf_geometry_collision_movers *movers,const scene_mover_interval *intervals,
    float duration,float remaining,rf_geometry_body_hit *best,uint32_t *matched,
    rf_geometry_body_metadata metadata,void *context)
{
    rf_geometry_body_hit value=*best;uint32_t m,k,q,prior,step,changed=0;int status;
    float limit=*matched?best->contact.fraction:1;
    if(!mesh || !body || !movers || !metadata || !isfinite(duration) || duration<=0 ||
       !isfinite(remaining) || remaining<0 || remaining>duration ||
       (movers->count && (!intervals || !movers->poses || !movers->views || !movers->owned)))return RF_RANGE;
    for(m=0;m<movers->count;m++) {
        const scene_mover_interval *interval=intervals+m;const rf_group_attached_pose *pose=movers->poses+m;
        const rf_geometry_collision_flat *flat=movers->owned+m;
        rf_physics_body_state relative=*body;rf_collision_mover_motion motion={0};rf_collision_mover_relative aligned;
        rf_geometry_collision_movers one=*movers;rf_geometry_body_hit candidate={0};uint32_t yes=1;
        scene_fragment_path path;float shift[3],lo[3],hi[3];
        if(interval->changed!=1 || (pose->flags&0x40000u))continue;
        if(interval->handle!=movers->views[m].object_id || memcmp(interval->end,pose->position,12) ||
           memcmp(interval->end_matrix,pose->input_matrix,36))return RF_FORMAT;
        memcpy(motion.start,body->position,12);memcpy(motion.end,body->next_position,12);
        memcpy(motion.body_matrix,body->orientation,36);memcpy(motion.next_body_matrix,body->next_orientation,36);
        memcpy(motion.mover_start,interval->start,12);memcpy(motion.mover_end,interval->end,12);
        for(k=0;k<3;k++)motion.mover_matrix[k][k]=1;
        motion.body_remaining=remaining;motion.mover_remaining=duration;
        status=rf_collision_mover_relative_sphere(&motion,&aligned);if(status)return status;
        for(k=0;k<3;k++){shift[k]=interval->end[k]-aligned.origin[k];relative.position[k]+=shift[k];}
        status=scene_detached_mesh_bounds(mesh,&relative,lo,hi);if(status)return status;
        if(!scene_detached_box_overlap(pose->minimum,pose->maximum,lo,hi))continue;
        one.count=1;one.poses=movers->poses+m;one.views=movers->views+m;one.owned=movers->owned+m;
        candidate.solid=UINT32_MAX;candidate.contact.fraction=limit;
        status=scene_detached_mover_vertex_sweep(mesh,&relative,&one,&candidate,&yes,scene_fragment_mover_metadata_unused,NULL);if(status)return status;
        status=scene_fragment_path_prepare(&relative,&path);if(status)return status;
        for(q=0;q<mesh->vertex_count;q++) {
            const float *local=mesh->vertices[q].position;
            for(prior=0;prior<q;prior++)if(!memcmp(local,mesh->vertices[prior].position,12))break;
            if(prior<q)continue;
            for(step=0;step<4;step++) {
                float first[3],last[3],a[3],delta[3],zero[3]={0},identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
                float begin=step*.25f,nearest=candidate.contact.fraction;
                rf_collision_sweep_tree_hit hit;rf_collision_ray_hit world;uint32_t found,j;
                if(nearest<=begin)break;
                for(k=0;k<3;k++) {
                    double x=path.position[step][k],y=path.position[step+1][k];
                    for(j=0;j<3;j++){x+=(double)local[j]*path.basis[step][j*3+k];y+=(double)local[j]*path.basis[step+1][j*3+k];}
                    first[k]=(float)x;last[k]=(float)y;
                }
                status=rf_collision_mover_sphere_local(zero,identity,first,last,pose->position,
                    (const float(*)[3])pose->input_matrix,a,delta);if(status)return status;
                status=rf_collision_flat_faces(flat->faces,flat->count,0x464,a,delta,zero,identity,0,
                    fminf(1,(nearest-begin)*4),&hit,&found);if(status)return status;if(!found)continue;
                status=rf_collision_contact_world(&hit.hit,pose->position,(const float(*)[3])pose->input_matrix,&world);if(status)return status;
                if(scene_detached_plane_extent(mesh,path.position[step],path.basis[step],world.point,world.normal)<=1e-5f)continue;
                world.fraction=begin+world.fraction*.25f;if(world.fraction>=candidate.contact.fraction)continue;
                memset(&candidate,0,sizeof(candidate));candidate.solid=0;candidate.room=UINT32_MAX;candidate.face=hit.face_index;candidate.hits=1;
                candidate.contact.fraction=world.fraction;memcpy(candidate.contact.point,world.point,12);memcpy(candidate.contact.normal,world.normal,12);
                candidate.contact.face_token=hit.face_index;candidate.contact.face_flag=(flat->faces[hit.face_index].filter.face_flags>>2)&1;
                candidate.contact.object_id=interval->handle;memcpy(candidate.contact.velocity,pose->velocity,12);
            }
        }
        if(candidate.solid!=0)continue;
        for(k=0;k<3;k++)candidate.contact.point[k]-=shift[k]*(1-candidate.contact.fraction);
        candidate.solid=m;value=candidate;limit=candidate.contact.fraction;changed=1;
    }
    if(changed) {
        status=metadata(context,value.solid,value.face,&value.contact.texture,&value.contact.material);if(status)return status;
        *best=value;*matched=1;
    }
    return RF_OK;
}
uint32_t rf_scene_fragment_moving_audit[16];
static int scene_fragment_moving_check(void)
{
    rf_geomod_vertex vertices[4]={{{-.5f,-.25f,-.5f},{0,0}},{{.5f,-.25f,-.5f},{0,0}},
        {{.5f,-.25f,.5f},{0,0}},{{-.5f,-.25f,.5f},{0,0}}};
    rf_geomod_face mesh_face={0,4,0,0};rf_geomod_mesh_view mesh={vertices,&mesh_face,4,1,0};
    float points[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
    rf_collision_face face={0};rf_geometry_collision_flat flat={0};rf_group_attached_pose pose={0};
    rf_collision_solid_view view={0};rf_geometry_collision_movers movers={0};scene_mover_interval interval;
    rf_physics_body_state body={0};uint32_t i,mode;int status;
    memset(rf_scene_fragment_moving_audit,0,sizeof(rf_scene_fragment_moving_audit));rf_scene_fragment_moving_audit[0]=1;
    face.vertices=points;face.count=4;face.plane[1]=1;flat.faces=&face;flat.count=1;view.object_id=77;
    movers.owned=&flat;movers.poses=&pose;movers.views=&view;movers.count=1;
    for(i=0;i<3;i++)pose.input_matrix[i*4]=body.orientation[i*4]=body.next_orientation[i*4]=1;
    status=scene_mover_intervals_capture(&movers,&interval,1,0);if(status)goto done;
    pose.position[1]=2;for(i=0;i<3;i++){pose.minimum[i]=-3;pose.maximum[i]=3;}
    pose.minimum[1]=1.999f;pose.maximum[1]=2.001f;
    status=scene_mover_intervals_capture(&movers,&interval,1,1);if(status)goto done;
    body.position[1]=body.next_position[1]=1;
    for(mode=0;mode<3;mode++) {
        rf_geometry_body_hit hit={0};uint32_t found=0,*record=rf_scene_fragment_moving_audit+4+mode*4;
        float x=mode==0?2:(mode==1?.02f:.25f),z=mode==0?2:(mode==1?.02f:1);
        for(i=0;i<4;i++) {
            points[i][0]=points[i][0]<0?-x:x;points[i][2]=points[i][2]<0?-z:z;
            if(mode==2){vertices[i].position[0]*=2;vertices[i].position[2]*=.5f;}
        }
        face.minimum[0]=-x;face.maximum[0]=x;face.minimum[2]=-z;face.maximum[2]=z;
        status=scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,scene_fragment_mover_metadata_unused,NULL);
        if(status)goto done;
        if(!found || hit.contact.fraction!=.375f || hit.contact.point[1]!=.75f || hit.contact.normal[1]!=1){status=RF_FORMAT;goto done;}
        record[0]=found;memcpy(record+1,&hit.contact.fraction,4);memcpy(record+2,hit.contact.point+1,4);memcpy(record+3,hit.contact.normal+1,4);
        rf_scene_fragment_moving_audit[1]++;
    }
    rf_scene_fragment_moving_audit[3]=1;
done:rf_scene_fragment_moving_audit[2]=(uint32_t)status;return status;
}
