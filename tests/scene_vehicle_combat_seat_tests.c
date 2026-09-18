#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_vehicle_combat_seat.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"combat seat line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    static scene_stream stream;static scene_driller_resources resource;rf_geometry_collision_world world={0};
    rf_vpp meshes={0};rf_model_file *file=malloc(sizeof(*file));rf_apc_checkpoint apc={0};rf_jeep_checkpoint jeep={0};
    rf_player_checkpoint player={0};rf_checkpoint_placement placement,kept;rf_physics_sphere scratch,saved;
    rf_physics_sphere spheres[3]={{{0,.2f,0},.9f,0,0},{{.1f,.8f,.2f},.35f,0,0},{{0,-.5f,0},.3f,0,0}};
    float basis[9]={0,0,-1,0,1,0,1,0,0},reference[9]={1,0,0,0,1,0,0,0,1},tag[12];uint32_t v,role,i,j;
    CHECK(file);CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    stream.driller=&resource;stream.collision=&world;stream.terrain_collision.room=7;
    scene_actor_body.spheres.items=spheres;scene_actor_body.spheres.count=3;scene_actor_body.state.state_124=0x1000;
    rf_scene_actor_initial_eye_offsets[0]=.1f;rf_scene_actor_initial_eye_offsets[1]=.8f;rf_scene_actor_initial_eye_offsets[2]=.2f;
    for(v=0;v<2;v++){
        rf_vehicle_checkpoint *host=v?&jeep.vehicle:&apc.vehicle;
        strcpy(resource.model,v?"Jeep01.v3m":"APC.v3m");CHECK(!rf_model_file_open(file,&meshes,resource.model));
        CHECK(!rf_static_model_tags_open(file,65536,&resource.tags));
        host->health=v?400:5000;host->alive=host->player_occupied=1;host->position[0]=30;host->position[1]=10;host->position[2]=-167;
        memcpy(host->orientation,basis,36);memcpy(jeep.aim_reference,reference,36);
        for(role=0;role<(v?2u:1u);role++){
            const float *body;int32_t seat=-1;jeep.role=role;
            for(i=0;i<resource.tags.count;i++)if(!strcmp(resource.tags.items[i].name,role?"interface_2":"interface_1"))seat=(int32_t)i;
            CHECK(seat>=0);CHECK(!rf_static_model_tag_place(&resource.tags,seat,basis,host->position,tag));body=role?reference:tag;
            for(i=0;i<3;i++){player.position[i]=tag[9+i];for(j=0;j<3;j++)player.position[i]-=rf_scene_actor_initial_eye_offsets[j]*body[j*3+i];}
            memset(&scratch,0xa5,sizeof(scratch));saved=scratch;
            CHECK(!(v?scene_jeep_checkpoint_seat_prepare(&stream,&jeep,&player,&placement,&scratch):
                scene_apc_checkpoint_seat_prepare(&stream,&apc,&player,&placement,&scratch)));
            CHECK(!memcmp(placement.basis,body,36) && placement.query_flags==4 && placement.replaced_room==7);
            if(v){CHECK(placement.count==1 && placement.spheres==&scratch && !memcmp(&scratch,spheres+1,sizeof(scratch)));}
            else{CHECK(placement.count==3 && placement.spheres==spheres && !memcmp(&scratch,&saved,sizeof(scratch)));}
            kept=placement;saved=scratch;player.position[0]+=.02f;
            CHECK((v?scene_jeep_checkpoint_seat_prepare(&stream,&jeep,&player,&placement,&scratch):
                scene_apc_checkpoint_seat_prepare(&stream,&apc,&player,&placement,&scratch))==RF_FORMAT);
            CHECK(!memcmp(&kept,&placement,sizeof(kept)) && !memcmp(&saved,&scratch,sizeof(saved)));
        }
        rf_static_model_tags_close(&resource.tags);
    }
    scene_actor_body.spheres.items=NULL;scene_actor_body.spheres.count=0;free(file);rf_vpp_close(&meshes);
    puts("PASS actual APC/Jeep seat tags, independent gunner basis, caller-owned head sphere, atomic rejection");return 0;
}
