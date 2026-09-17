/* New adapter test uses the retained cube builder, not the cavity as the
 * reset candidate. The full room comprises outer shell plus actual solid. */
#define main checkpoint_synthetic_unused_main
#include "scene_checkpoint_placement_probe.c"
#undef main
#include "rf/authored_reset_placement.h"
#include "rf/geomod_authored_post.h"

static int installed(const char *archive_path,const char *body_path)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geometry_collision_world world={0};
    rf_geomod_authored_post *asset=NULL;rf_geomod_authored_post_view a;
    rf_checkpoint_placement p={0};rf_checkpoint_placement_result result;
    rf_physics_sphere body[8];float eyes[6];uint32_t header[4];FILE *file=fopen(body_path,"rb");
    CHECK(file && fread(header,4,4,file)==4 && header[0]==0x31534652 && header[1]>0 && header[1]<=8);
    CHECK(fread(eyes,4,6,file)==6 && fread(body,sizeof(*body),header[1],file)==header[1] && fgetc(file)==EOF);fclose(file);
    CHECK(!rf_vpp_open(&archive,archive_path));CHECK(!rf_level_open(&level,&archive,"ctf06.rfl"));
    CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(!rf_geometry_collision_world_open(&geometry,16*1024*1024,&world));
    CHECK(!rf_geomod_authored_post_open(&level,&geometry,2*1024*1024,&asset));CHECK(!rf_geomod_authored_post_get(asset,&a));
    p.world=&world;p.replaced_room=a.room;p.query_flags=4;p.spheres=body;p.count=header[1];
    p.basis[2]=1;p.basis[4]=1;p.basis[6]=-1;
    p.position[0]=-2.75f;p.position[1]=-.4013611376285553f;p.position[2]=2.5f;
    CHECK(!rf_authored_reset_standing_check(&world.rooms[a.room].tree,a.source_planes,a.source.face_count,&p,1.f/60,5,&result));
    p.position[0]=-5;
    CHECK(rf_authored_reset_standing_check(&world.rooms[a.room].tree,a.source_planes,a.source.face_count,&p,1.f/60,5,&result)==RF_NOT_FOUND);
    printf("INSTALLED post_inside rejected sphere%u reason%u\n",result.sphere,result.reason);
    p.position[0]=4.446455955505371f;
    CHECK(!rf_authored_reset_standing_check(&world.rooms[a.room].tree,a.source_planes,a.source.face_count,&p,1.f/60,5,&result));
    p.position[1]+=.5f;
    CHECK(rf_authored_reset_standing_check(&world.rooms[a.room].tree,a.source_planes,a.source.face_count,&p,1.f/60,5,&result)==RF_NOT_FOUND);
    {
        const uint32_t uids[3]={93,96,97};uint32_t i;
        for(i=0;i<3;i++) {
            rf_geomod_authored_post_close(&asset);
            CHECK(!rf_geomod_authored_post_open_source(&level,&geometry,uids[i],2*1024*1024,&asset));
            CHECK(!rf_geomod_authored_post_get(asset,&a));
            p.position[0]=uids[i]>=96?8.25f:-2.75f;p.position[1]=-.4013611376285553f;
            p.position[2]=uids[i]==97?2.5f:-2.5f;
            CHECK(!rf_authored_reset_standing_check(&world.rooms[a.room].tree,a.source_planes,a.source.face_count,&p,1.f/60,5,&result));
            p.position[0]=uids[i]>=96?6.f:-5.f;
            CHECK(rf_authored_reset_standing_check(&world.rooms[a.room].tree,a.source_planes,a.source.face_count,&p,1.f/60,5,&result)==RF_NOT_FOUND);
        }
        puts("PASS all four source reset bounds: safe floor admitted, restored solid occupancy rejected");
    }
    rf_geomod_authored_post_close(&asset);rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);rf_vpp_close(&archive);
    puts("PASS installed post reset actual body union, initial/retreated floor support, inside rejection");return 0;
}
int main(int argc,char **argv)
{
    rf_geomod_mesh_view outer,solid;rf_collision_face all[12];rf_collision_tree original={0},cut={0};
    rf_collision_room_view room={0};rf_geometry_collision_world world={0};uint32_t root=0,i;
    rf_checkpoint_placement p={0};rf_checkpoint_placement_result result,sentinel;
    rf_physics_sphere body[2]={{{0,0,0},.5f,0,0},{{0,1,0},.5f,0,0}};
    float zero[3]={0,0,0},post[3]={-4,-8,0},planes[6][4],outer_positions[24][3];
    cube(vertices,faces,zero,10,1);outer=(rf_geomod_mesh_view){vertices,faces,24,6,0};
    CHECK(!rf_geomod_collision_faces(&outer,filters,outer_positions,24,all,6));
    cube(obstacle_vertices,obstacle_faces,post,1,0);solid=(rf_geomod_mesh_view){obstacle_vertices,obstacle_faces,24,6,0};
    CHECK(!rf_geomod_collision_faces(&solid,filters,obstacle_positions,24,all+6,6));
    for(i=0;i<6;i++)memcpy(planes[i],all[i+6].plane,16);
    original.faces=all;original.face_count=12;cut.faces=all;cut.face_count=6;
    room.tree=&cut;world.views=&room;world.primary=&root;world.primary_count=world.room_count=1;
    p.world=&world;p.query_flags=4;p.spheres=body;p.count=2;p.basis[0]=p.basis[4]=p.basis[8]=1;p.position[1]=-9.5f;
    CHECK(!rf_authored_reset_standing_check(&original,planes,6,&p,1.f/60,5,&result));
    p.position[0]=-4;p.position[1]=-8;
    CHECK(rf_authored_reset_standing_check(&original,planes,6,&p,1.f/60,5,&result)==RF_NOT_FOUND);
    p.position[0]=-2.49f;p.position[1]=-9.5f;
    CHECK(!rf_authored_reset_standing_check(&original,planes,6,&p,1.f/60,5,&result));
    p.position[0]=-2.6f;
    CHECK(rf_authored_reset_standing_check(&original,planes,6,&p,1.f/60,5,&result)==RF_NOT_FOUND);
    p.position[0]=0;p.position[1]=-9.45f;
    CHECK(!rf_authored_reset_standing_check(&original,planes,6,&p,1.f/60,5,&result));
    memset(&result,0xa5,sizeof(result));sentinel=result;planes[0][0]=NAN;
    CHECK(rf_authored_reset_standing_check(&original,planes,6,&p,1.f/60,5,&result)==RF_FORMAT && !memcmp(&result,&sentinel,sizeof(result)));
    planes[0][0]=all[6].plane[0];
    CHECK(rf_authored_reset_standing_check(&original,planes,0,&p,1.f/60,5,&result)==RF_RANGE && !memcmp(&result,&sentinel,sizeof(result)));
    p.basis[4]=2;
    CHECK(rf_authored_reset_standing_check(&original,planes,6,&p,1.f/60,5,&result)==RF_FORMAT && !memcmp(&result,&sentinel,sizeof(result)));
    p.basis[4]=1;p.count=9;
    CHECK(rf_authored_reset_standing_check(&original,planes,6,&p,1.f/60,5,&result)==RF_RANGE && !memcmp(&result,&sentinel,sizeof(result)));
    p.count=2;
    CHECK(room.tree==&cut && cut.face_count==6 && original.face_count==12);
    puts("PASS authored reset full-room substitution, union overlap, floor contact, rejection rollback");
    if(argc==3)return installed(argv[1],argv[2]);return argc==1?0:2;
}
