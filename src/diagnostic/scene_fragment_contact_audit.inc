/* Opt-in, process-local numerical fixture. Uses production contact paths with
 * isolated owned world/mover geometry; does not edit the loaded DEV room. */
uint32_t rf_scene_fragment_contact_audit[64];
static int scene_fragment_audit_material(void *context,uint32_t solid,uint32_t face,
    uint32_t *texture,uint32_t *material)
{
    if(context)return RF_IO;
    if(solid==UINT32_MAX && face==17){*texture=9;*material=3;return RF_OK;}
    if(solid==0 && face==0){*texture=11;*material=7;return RF_OK;}
    return RF_FORMAT;
}
#include "scene_fragment_edge_audit.inc"
static void scene_fragment_audit_hit(uint32_t row,uint32_t found,const rf_geometry_body_hit *hit)
{
    uint32_t *words=rf_scene_fragment_contact_audit+4+row*8;
    words[0]=row;words[1]=found;memcpy(words+2,&hit->contact.fraction,4);memcpy(words+3,hit->contact.normal,12);
    words[6]=hit->solid==UINT32_MAX?hit->face:hit->contact.object_id;words[7]=hit->contact.material;
}
int rf_scene_fragment_contact_check(void)
{
    rf_geomod_vertex vertices[4]={{{-.5f,-.5f,-.5f},{0,0}},{{.5f,-.5f,-.5f},{0,0}},
        {{.5f,-.5f,.5f},{0,0}},{{-.5f,-.5f,.5f},{0,0}}};
    rf_geomod_face polygon={0,4,0,0};rf_geomod_mesh_view mesh={vertices,&polygon,4,1,0};
    float patch[4][3]={{-.02f,0,-.02f},{-.02f,0,.02f},{.02f,0,.02f},{.02f,0,-.02f}},delta[3]={0,-2,0};
    rf_collision_face face={0};rf_geometry_collision_room room={0};rf_collision_room_view view={0};
    rf_geometry_collision_world world={0};rf_physics_body_state body={0};scene_detached_query_context query={0};
    rf_geometry_collision_flat flat={0};rf_group_attached_pose pose={0};rf_collision_solid_view mover_view={0};
    rf_geometry_collision_movers movers={0};rf_geometry_body_hit hit={0},saved;scene_stream *scene=NULL;
    uint32_t i,j,mode,found=0,primary=0;int status=RF_OK,control;
#define CONTACT_AUDIT_CALL(x) do {status=(x);if(status)goto done;} while(0)
#define CONTACT_AUDIT_TEST(x) do {if(!(x)){status=RF_FORMAT;goto done;}} while(0)
    memset(rf_scene_fragment_contact_audit,0,sizeof(rf_scene_fragment_contact_audit));rf_scene_fragment_contact_audit[0]=2;
    scene=calloc(1,sizeof(*scene));if(!scene){status=RF_IO;goto done;}
    face.vertices=patch;face.count=4;face.plane[1]=1;
    for(i=0;i<3;i++){face.minimum[i]=-.0201f;face.maximum[i]=.0201f;body.orientation[i*4]=body.next_orientation[i*4]=1;}
    body.position[1]=1;body.next_position[1]=-1;body.bounds.radius=.001f;
    for(i=0;i<8;i++) {
        float start[3];rf_collision_sweep_hit sphere;
        for(j=0;j<3;j++)start[j]=((i>>j)&1)?.25f:-.25f;start[1]+=1;
        CONTACT_AUDIT_CALL(rf_collision_sweep_face(&face,start,delta,delta,.25f,1,&sphere,&found));
        CONTACT_AUDIT_TEST(!found);rf_scene_fragment_contact_audit[5]++;
    }
    for(i=0;i<4;i++) {
        float start[3];rf_collision_ray_hit corner;
        for(j=0;j<3;j++)start[j]=body.position[j]+vertices[i].position[j];
        CONTACT_AUDIT_CALL(rf_collision_thin_face(&face,start,delta,1,&corner,&found));
        CONTACT_AUDIT_TEST(!found);rf_scene_fragment_contact_audit[6]++;
    }
    CONTACT_AUDIT_CALL(rf_collision_tree_open(&face,1,65536,&room.tree));room.tree.source_indices[0]=17;
    view.tree=&room.tree;for(i=0;i<3;i++){view.minimum[i]=-3;view.maximum[i]=3;}
    world.rooms=&room;world.views=&view;world.room_count=1;world.primary=&primary;world.primary_count=1;
    scene->collision=&world;query.scene=scene;query.mesh=&mesh;
    CONTACT_AUDIT_CALL(scene_detached_world_vertex_sweep(&query,&body,&hit,&found,scene_fragment_audit_material,NULL));
    CONTACT_AUDIT_TEST(found && hit.contact.fraction==.25f && hit.contact.normal[1]==1 && hit.face==17 && hit.room==0);
    CONTACT_AUDIT_TEST(hit.contact.texture==9 && hit.contact.material==3);
    scene_fragment_audit_hit(1,found,&hit);
    flat.faces=&face;flat.count=1;movers.count=1;movers.owned=&flat;movers.views=&mover_view;movers.poses=&pose;
    mover_view.object_id=77;pose.position[0]=3;pose.position[1]=2;
    for(i=0;i<3;i++){pose.minimum[i]=-10;pose.maximum[i]=10;pose.velocity[i]=(float)(i+1);pose.public_position[i]=99;pose.output_matrix[i*4]=-1;}
    for(mode=0;mode<2;mode++) {
        memset(pose.input_matrix,0,36);
        if(!mode)for(i=0;i<3;i++)pose.input_matrix[i*4]=1;
        else {pose.input_matrix[1]=1;pose.input_matrix[3]=-1;pose.input_matrix[8]=1;}
        memcpy(body.orientation,pose.input_matrix,36);memcpy(body.next_orientation,pose.input_matrix,36);
        for(i=0;i<3;i++){body.position[i]=pose.position[i]+pose.input_matrix[3+i];body.next_position[i]=pose.position[i]-pose.input_matrix[3+i];}
        found=0;
        CONTACT_AUDIT_CALL(scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&hit,&found,scene_fragment_audit_material,NULL));
        CONTACT_AUDIT_TEST(found && hit.contact.fraction==.25f && hit.contact.object_id==77 && hit.contact.texture==11 && hit.contact.material==7);
        CONTACT_AUDIT_TEST(!memcmp(hit.contact.velocity,pose.velocity,12));
        CONTACT_AUDIT_TEST(mode?hit.contact.normal[0]==-1:hit.contact.normal[1]==1);
        scene_fragment_audit_hit(2+mode,found,&hit);
    }
    for(i=0;i<3;i++)pose.position[i]+=.25f*pose.input_matrix[3+i];found=0;
    CONTACT_AUDIT_CALL(scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&hit,&found,scene_fragment_audit_material,NULL));
    CONTACT_AUDIT_TEST(found && hit.contact.fraction==.125f);scene_fragment_audit_hit(4,found,&hit);
    saved=hit;found=0;pose.flags=0x40000;
    CONTACT_AUDIT_CALL(scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&hit,&found,scene_fragment_audit_material,NULL));
    CONTACT_AUDIT_TEST(!found && !memcmp(&hit,&saved,sizeof(hit)));
    rf_scene_fragment_contact_audit[44]=5;rf_scene_fragment_contact_audit[45]=found;
    rf_scene_fragment_contact_audit[50]=hit.contact.object_id;pose.flags=0;
    control=scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&hit,&found,scene_fragment_audit_material,&found);
    CONTACT_AUDIT_TEST(control==RF_IO && !found && !memcmp(&hit,&saved,sizeof(hit)));
    rf_scene_fragment_contact_audit[52]=6;rf_scene_fragment_contact_audit[53]=(uint32_t)control;
    rf_scene_fragment_contact_audit[54]=!memcmp(&hit,&saved,sizeof(hit));rf_scene_fragment_contact_audit[55]=found;
    {
        float a0[3]={-1,.5f,-.25f},b0[3]={1,.5f,-.25f},a1[3]={-1,-1.5f,-.25f},b1[3]={1,-1.5f,-.25f};
        float c[3]={-.25f,0,-1},d[3]={-.25f,0,1};rf_collision_ray_hit edge;
        CONTACT_AUDIT_CALL(rf_collision_swept_edge(a0,b0,a1,b1,c,d,1,&edge,&found));
        CONTACT_AUDIT_TEST(found && edge.fraction==.25f && edge.point[0]==-.25f && edge.point[1]==0 && edge.point[2]==-.25f);
        CONTACT_AUDIT_TEST(edge.normal[0]==0 && edge.normal[1]==1 && edge.normal[2]==0);
        rf_scene_fragment_contact_audit[60]=7;rf_scene_fragment_contact_audit[61]=found;
        memcpy(rf_scene_fragment_contact_audit+62,&edge.fraction,4);memcpy(rf_scene_fragment_contact_audit+63,edge.normal+1,4);
    }
    rf_scene_fragment_contact_audit[1]=8;rf_scene_fragment_contact_audit[3]=1;
done:
    rf_collision_tree_close(&room.tree);free(scene);if(!status)status=scene_fragment_edge_check();rf_scene_fragment_contact_audit[2]=(uint32_t)status;
#undef CONTACT_AUDIT_CALL
#undef CONTACT_AUDIT_TEST
    return status;
}
