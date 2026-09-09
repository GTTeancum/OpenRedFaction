#include "rf/scene_preview.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include <stdlib.h>
#include <string.h>
extern uint32_t rf_scene_actor_physics_diagnostic[8];
extern float rf_scene_actor_contact_time[4];
extern rf_physics_body scene_actor_body;
extern rf_physics_body_state rf_scene_actor_fall_state;
extern uint32_t rf_scene_actor_initial_world[8],rf_scene_actor_initial_fall[8];
extern uint32_t rf_scene_actor_tick_stats[8];
extern uint32_t rf_scene_actor_ground_stats[8];
extern uint32_t rf_scene_actor_landing[8];
extern rf_movement_descriptor rf_scene_actor_movement[2];
extern rf_entity_movement_values rf_scene_actor_movement_values;
typedef struct check {
    const char *meshes,*motions;rf_animation_placement placement;
    rf_preview_mesh world;rf_model_materials bundle;uint32_t base,next,changed,last,stop,authored,body_mode;
    const void *address,*material_address;
} check;
static int frame_check(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world)
{
    check *c=context;uint32_t i,hash=2166136261u;const uint8_t *bytes;
    if(frame!=c->next++ || world!=c->world.count || mesh->count<world || mesh->count%3 ||
       mesh->bytes!=(uint64_t)mesh->count*sizeof(rf_preview_vertex) ||
       mesh->bytes>c->world.bytes+1024*1024 || memcmp(mesh->vertices,c->world.vertices,c->world.bytes))return RF_FORMAT;
    if(c->address && (c->address!=mesh->vertices || c->material_address!=materials->items))return RF_FORMAT;
    c->address=mesh->vertices;c->material_address=materials->items;
    if(c->body_mode && frame>22 && (rf_scene_actor_landing[1]!=1 ||
       scene_actor_body.state.velocity[0]!=0 || scene_actor_body.state.velocity[1]!=0 ||
       scene_actor_body.state.velocity[2]!=0))return RF_FORMAT;
    bytes=(const uint8_t*)(mesh->vertices+world);
    for(i=0;i<mesh->bytes-c->world.bytes;++i)hash=(hash^bytes[i])*16777619u;
    if(frame && hash!=c->last)++c->changed;c->last=hash;
    for(i=world;i<mesh->count;++i)if(mesh->vertices[i].material<c->base ||
        mesh->vertices[i].material>=materials->count || mesh->vertices[i].lightmap!=UINT32_MAX)return RF_FORMAT;
    if(!c->authored && (frame==0 || frame==4 || frame==32 || frame==63)) {
        rf_preview_mesh expected={0};int status=rf_animation_preview_placed(c->meshes,c->motions,
            &c->placement,frame,&expected,1024*1024);
        if(status)return status;
        for(i=0;i<expected.count;++i) {
            uint32_t slot;
            if(expected.vertices[i].material>=c->bundle.count) {rf_preview_close(&expected);return RF_FORMAT;}
            memcpy(&slot,c->bundle.items[expected.vertices[i].material].record.bytes+0x10,4);
            expected.vertices[i].material=c->base+slot;
        }
        status=expected.bytes!=mesh->bytes-c->world.bytes || memcmp(expected.vertices,bytes,expected.bytes);
        rf_preview_close(&expected);if(status)return RF_FORMAT;
    }
    printf("Frame %u actor triangles %u hash %08x\n",frame,(mesh->count-world)/3,hash);
    return c->stop && frame==2?RF_NOT_FOUND:RF_OK;
}
int main(int argc,char **argv)
{
    if((argc==5 || argc==6) && !strcmp(argv[1],"--pose-world")) {
        rf_vpp archive;rf_level level;rf_geometry world={0};rf_scene_world_geometry owned={0};
        rf_materials images={0};rf_preview_mesh mesh={0};rf_group_attached_pose *poses;
        FILE *input;uint32_t capacity,i,hash;void *buffer;size_t size;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) ||
            rf_scene_preview_mover_camera(&level,8544,6.0f) || rf_geometry_open(&world,&level,8*1024*1024) ||
            rf_scene_world_open_retained(&level,&world,NULL,0,&mesh,&images,8*1024*1024,4*1024*1024,&owned))return 1;
        rf_vpp_close(&archive);memset(&level,0xa5,sizeof(level));rf_materials_close(&images);
        capacity=mesh.bytes+1024*1024;buffer=realloc(mesh.vertices,capacity);if(!buffer)return 1;mesh.vertices=buffer;
        size=owned.movers.count*sizeof(*poses);poses=malloc(size);if(!size || !poses)return 1;
        input=fopen(argv[4],"rb");if(!input)return 1;
        for(;;) {
            size_t got=fread(poses,1,size,input);if(!got && feof(input))break;if(got!=size)return 3;
            if(rf_scene_world_update(&owned,poses,owned.movers.count,&mesh,capacity) || mesh.vertices!=buffer)return 3;
            hash=2166136261u;for(i=0;i<mesh.bytes;++i)hash=(hash^((const unsigned char *)mesh.vertices)[i])*16777619u;
            printf("%u %u\n",mesh.count,hash);
        }
        if(argc==6) {FILE *output=fopen(argv[5],"wb");if(!output || fwrite(mesh.vertices,1,mesh.bytes,output)!=mesh.bytes || fclose(output))return 3;}
        fclose(input);free(poses);rf_preview_close(&mesh);rf_scene_world_geometry_close(&owned);rf_geometry_close(&world);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--retained-world")) {
        rf_vpp archive;rf_level source,camera;rf_geometry world={0};rf_scene_world_geometry owned={0};
        rf_materials images={0};rf_preview_mesh mesh={0},expected={0};rf_geometry_materials mapping={0};
        rf_group_attached_pose *poses;uint32_t i,j,frame,capacity,hash=2166136261u;void *address;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&source,&archive,argv[3]) ||
            rf_geometry_open(&world,&source,8*1024*1024))return 1;
        camera=source;
        if(rf_scene_world_open_retained(&source,&world,NULL,0,&mesh,&images,8*1024*1024,4*1024*1024,&owned))return 1;
        rf_vpp_close(&archive);memset(&source,0xa5,sizeof(source));rf_materials_close(&images);
        capacity=mesh.bytes+1024*1024;address=realloc(mesh.vertices,capacity);if(!address)return 1;mesh.vertices=address;
        poses=malloc(owned.movers.count*sizeof(*poses));if(owned.movers.count && !poses)return 1;
        mapping.offsets=owned.offsets;mapping.slots=owned.slots;mapping.count=owned.geometry_count;mapping.textures.count=owned.material_count;
        for(frame=0;frame<3;++frame) {
            for(i=0;i<owned.movers.count;++i) {
                memset(poses+i,0xa5,sizeof(*poses));
                for(j=0;j<3;++j)poses[i].position[j]=owned.movers.items[i].position[j]+frame*(float)(j+1);
                memcpy(poses[i].output_matrix,owned.movers.items[i].orientation,36);
            }
            if(rf_scene_world_update(&owned,poses,owned.movers.count,&mesh,capacity) || mesh.vertices!=address ||
                rf_preview_build_world(&expected,&world,&owned.movers,poses,&mapping,&camera,capacity) ||
                mesh.bytes!=expected.bytes || (mesh.bytes && memcmp(mesh.vertices,expected.vertices,mesh.bytes)))return 3;
            for(i=0;i<mesh.bytes;++i)hash=(hash^((const unsigned char *)mesh.vertices)[i])*16777619u;
            rf_preview_close(&expected);
        }
        printf("%u %u %u %u\n",owned.movers.count,owned.allocated_bytes,capacity,hash);
        free(poses);rf_preview_close(&mesh);rf_scene_world_geometry_close(&owned);rf_scene_world_geometry_close(&owned);
        rf_geometry_close(&world);return 0;
    }
    rf_vpp levels,meshes,maps[5];rf_level level;rf_geometry geometry={0};
    rf_level_actor_assets binding;rf_model_file model;const char *names[64];
    check c={0};uint32_t i,mode;int status,body_mode=argc==13 && !strcmp(argv[12],"--body");rf_geometry_collision_world body_world={0};
    if(argc!=12 && (argc!=13 || (strcmp(argv[12],"--states") && !body_mode)))return 2;
    c.authored=argc==13;
    c.body_mode=body_mode;
    c.meshes=argv[4];c.motions=argv[5];
    if(rf_vpp_open(&levels,argv[1]) || rf_level_open(&level,&levels,argv[2]) ||
       rf_scene_preview_camera(&level,(int32_t)strtol(argv[3],NULL,10)) ||
       rf_geometry_open(&geometry,&level,8*1024*1024) ||
       rf_preview_build(&c.world,&geometry,&level,8*1024*1024) || rf_vpp_open(&meshes,argv[4]))return 3;
    for(i=0;i<5;++i)if(rf_vpp_open(maps+i,argv[7+i]))return 3;
    if(rf_level_actor_assets_load(&level,(int32_t)strtol(argv[3],NULL,10),argv[6],&meshes,512*1024,&binding) ||
       rf_animation_placement_from_level(&level,&binding.entity,&c.placement) ||
       rf_model_file_open(&model,&meshes,binding.mesh.name))return 3;
    for(i=0;i<binding.assets.texture_count;++i)names[i]=binding.assets.textures[i];
    if(rf_model_materials_open_skin(&c.bundle,&model,names,binding.assets.texture_count,maps,5,4*1024*1024))return 3;
    if(body_mode && rf_geometry_collision_world_open(&geometry,8*1024*1024,&body_world))return 3;
    for(mode=0;mode<3;++mode) {
        rf_preview_mesh mesh={0},before;rf_materials materials={0},saved;
        if(rf_preview_build(&mesh,&geometry,&level,8*1024*1024) ||
           rf_materials_open(&materials,&geometry,maps,5,4*1024*1024))return 3;
        c.base=materials.count;c.next=c.changed=c.last=0;c.address=c.material_address=NULL;c.stop=mode==1;
        before=mesh;saved=materials;
        if(body_mode)status=rf_scene_stream_miner_body(&level,binding.entity.uid,argv[4],argv[5],argv[6],maps,5,
            &mesh,&materials,mode==2?mesh.bytes+1024*1024-1:8*1024*1024,4*1024*1024,frame_check,&c,&body_world);
        else if(c.authored)status=rf_scene_stream_miner_states(&level,binding.entity.uid,argv[4],argv[5],argv[6],maps,5,
            &mesh,&materials,mode==2?mesh.bytes+1024*1024-1:8*1024*1024,4*1024*1024,frame_check,&c);
        else status=rf_scene_stream_miner(&level,binding.entity.uid,argv[4],argv[5],argv[6],maps,5,
            &mesh,&materials,mode==2?mesh.bytes+1024*1024-1:8*1024*1024,4*1024*1024,frame_check,&c);
        if(mode==0 && (status || c.next!=64 || !c.changed))return 3;
        if(mode==0) {
            if(body_mode) {if(rf_scene_actor_tick_stats[1]!=63)return 3;
                printf("ACTOR_TICKS");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_tick_stats[i]);puts("");
                if(rf_scene_actor_ground_stats[1]!=64 || !rf_scene_actor_ground_stats[3])return 3;
                printf("ACTOR_GROUND");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_ground_stats[i]);puts("");}
            if(body_mode) {
                if(rf_scene_actor_landing[2]!=22 || rf_scene_actor_landing[3]!=1 || rf_scene_actor_landing[4]!=41)return 3;
                printf("ACTOR_LANDING");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_landing[i]);puts("");
                printf("ACTOR_MOVEMENT");for(i=0;i<16;++i) {uint32_t word;memcpy(&word,(const unsigned char*)rf_scene_actor_movement+i*4,4);printf(" %u",word);}puts("");
                printf("ACTOR_SPEED");for(i=0;i<4;++i) {uint32_t word;memcpy(&word,(const unsigned char*)&rf_scene_actor_movement_values+i*4,4);printf(" %u",word);}puts("");
            }
            if(rf_scene_actor_physics_diagnostic[1]!=1 || rf_scene_actor_physics_diagnostic[2]!=64 || rf_scene_actor_physics_diagnostic[3]!=3)return 3;
            printf("PHYSICS");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_physics_diagnostic[i]);puts("");
            {rf_geometry_collision_world collision={0};uint32_t sweep[8];
             if(rf_geometry_collision_world_open(&geometry,8*1024*1024,&collision))return 3;
             if(body_mode)memcpy(sweep,rf_scene_actor_initial_world,sizeof(sweep));
             else {status=rf_scene_actor_world_check(&collision,sweep);if(status)return 3;}
             printf("ACTOR_WORLD");for(i=0;i<8;++i)printf(" %u",sweep[i]);puts("");
             if(body_mode)memcpy(sweep,rf_scene_actor_initial_fall,sizeof(sweep));
             else status=rf_scene_actor_fall_check(&collision,sweep);
             rf_geometry_collision_world_close(&collision);if(status)return 3;
             if(sweep[2]>=120 || sweep[3]>=3)return 3;
             printf("ACTOR_FALL");for(i=0;i<8;++i)printf(" %u",sweep[i]);puts("");
             if(rf_scene_actor_contact_time[2]!=0 || rf_scene_actor_contact_time[3]<2)return 3;
             memcpy(sweep,rf_scene_actor_contact_time,16);printf("ACTOR_TIME %u %u %u %u\n",sweep[0],sweep[1],sweep[2],sweep[3]);}
            {
                rf_physics_body moved=scene_actor_body;rf_animation_placement explicit_pose=c.placement,body_pose=c.placement;
                rf_preview_mesh expected={0},actual={0};rf_physics_body_state saved;
                moved.state=rf_scene_actor_fall_state;saved=moved.state;
                memcpy(explicit_pose.position,moved.state.position,12);memcpy(explicit_pose.orientation,moved.state.orientation,36);
                body_pose.physics_body=&moved;
                if(!memcmp(body_pose.position,moved.state.position,12))return 3;
                if(rf_animation_preview_placed(c.meshes,c.motions,&explicit_pose,63,&expected,1024*1024) ||
                   rf_animation_preview_placed(c.meshes,c.motions,&body_pose,63,&actual,1024*1024))return 3;
                if(!actual.count || actual.bytes!=expected.bytes || memcmp(actual.vertices,expected.vertices,actual.bytes) ||
                   memcmp(&moved.state,&saved,sizeof(saved)))return 3;
                printf("BODY_RENDER %u vertices match explicit physics pose\n",actual.count);
                rf_preview_close(&expected);rf_preview_close(&actual);
            }
        }
        if(mode==1 && (status!=RF_NOT_FOUND || c.next!=3))return 3;
        if(mode==2 && (status!=RF_RANGE || c.next || memcmp(&before,&mesh,sizeof(mesh)) ||
            memcmp(&saved,&materials,sizeof(materials))))return 3;
        rf_preview_close(&mesh);rf_materials_close(&materials);
    }
    rf_geometry_collision_world_close(&body_world);
    rf_model_materials_close(&c.bundle);rf_preview_close(&c.world);rf_geometry_close(&geometry);
    for(i=0;i<5;++i)rf_vpp_close(maps+i);rf_vpp_close(&meshes);rf_vpp_close(&levels);
    puts(c.authored?"PASS: 64 authored-state scene frames, fixed world and allocations, sink cancellation, capacity guard":
        "PASS: 64 scene frames, fixed world and allocations, four independent pose snapshots, sink cancellation, capacity guard");
    return 0;
}
