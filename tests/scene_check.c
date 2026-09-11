#include "rf/eye.h"
#include "rf/scene_preview.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
extern uint32_t rf_scene_player_input_frames[64][7];
extern uint32_t rf_scene_actor_room_frames[64][9],rf_scene_actor_room_summary[8];
extern uint32_t rf_scene_actor_physics_diagnostic[8];
extern uint32_t rf_scene_actor_live_enabled,rf_scene_actor_frame_count,rf_scene_actor_live_summary[8],rf_scene_actor_ring_frames[64];
extern uint32_t rf_scene_actor_route_enabled,rf_scene_actor_routes[8][16];
extern float rf_scene_actor_contact_time[4];
extern rf_physics_body scene_actor_body;
extern rf_physics_body_state rf_scene_actor_fall_state;
extern uint32_t rf_scene_actor_initial_world[8],rf_scene_actor_initial_fall[8];
extern uint32_t rf_scene_actor_tick_stats[8];
extern uint32_t rf_scene_actor_ground_stats[8];
extern uint32_t rf_scene_actor_landing[8];
extern rf_movement_descriptor rf_scene_actor_movement[2];
extern rf_entity_movement_values rf_scene_actor_movement_values;
extern uint32_t rf_scene_actor_ground_modes[64];
extern uint32_t rf_scene_actor_drive_enabled;
extern float rf_scene_actor_input_frames[64][3];
extern uint32_t rf_scene_actor_contact_count,rf_scene_actor_contacts[64][25];
extern rf_physics_stance_cache rf_scene_actor_stance_cache;
extern uint32_t rf_scene_actor_stance_frames[64][4];
extern uint32_t rf_scene_actor_locomotion_frames[64][12];
extern uint32_t rf_scene_actor_animation_timing[64][3];
extern uint32_t rf_scene_actor_stance_support[64][9];
extern unsigned char rf_scene_actor_stance_ground[],rf_scene_actor_ground_records[];
extern uint32_t rf_scene_actor_initial_animation[12];
extern float rf_scene_actor_initial_eye_offsets[6];
extern uint32_t rf_scene_actor_selector_frames[64][8];
extern uint32_t rf_scene_actor_clearance_diagnostic[8];
extern float rf_scene_actor_clearance_queries[2][12];
extern uint32_t rf_scene_actor_surface_frames[64][2];
extern uint32_t rf_scene_actor_movement_frames[64][3];
extern uint32_t rf_scene_actor_render_frames[64][5];
extern uint32_t rf_scene_actor_follow_frames[64][14],rf_scene_actor_follow_summary[5];
static int follow_camera;
extern uint32_t rf_scene_actor_eye_frames[64][46],rf_scene_actor_look_frames[64][33];
static rf_scene_world_geometry follow_world;
static int build_check_world(const rf_level *level,const rf_geometry *geometry,rf_vpp *maps,
    rf_preview_mesh *mesh,rf_materials *materials)
{
    if(rf_scene_actor_live_enabled) {
        rf_scene_world_geometry owned={0};
        rf_scene_world_geometry *owner=follow_camera && !follow_world.world?&follow_world:&owned;
        int status=rf_scene_world_open_retained(level,geometry,maps,5,mesh,materials,8*1024*1024,4*1024*1024,owner);
        rf_scene_world_geometry_close(&owned);return status;
    }
    {int status=rf_preview_build(mesh,geometry,level,8*1024*1024);return status?status:rf_materials_open(materials,geometry,maps,5,4*1024*1024);}
}
typedef struct check {
    const char *meshes,*motions;rf_animation_placement placement;
    rf_preview_mesh world;rf_model_materials bundle;uint32_t base,next,changed,last,stop,authored,body_mode;
    const void *address,*material_address;
    float particle_camera[3];
} check;
static int frame_check(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world)
{
    check *c=context;uint32_t i,hash=2166136261u;const uint8_t *bytes;
    {int status=rf_scene_draw_particles(NULL,NULL);if(status)return status;}
    {int status=rf_scene_draw_player_flash(NULL,NULL);if(status)return status;}
    if(frame!=c->next++ || (!follow_camera && world!=c->world.count) || mesh->count<world || mesh->count%3 ||
       mesh->bytes!=(uint64_t)mesh->count*sizeof(rf_preview_vertex) ||
       mesh->bytes>(follow_camera?RF_SCENE_FOLLOW_CAPACITY:c->world.bytes+1024*1024) || (!follow_camera && memcmp(mesh->vertices,c->world.vertices,c->world.bytes)))return RF_FORMAT;
    if(c->address && (c->address!=mesh->vertices || c->material_address!=materials->items))return RF_FORMAT;
    c->address=mesh->vertices;c->material_address=materials->items;
    if(follow_camera) {
        const uint32_t *r=rf_scene_actor_follow_frames[frame%64];float camera[3],expected[3];
        memcpy(camera,r+2,12);memcpy(expected,scene_actor_body.state.position,12);if(rf_scene_actor_eye_enabled) {
            rf_eye_input input;float calculated[3];
            const uint32_t *e=rf_scene_actor_eye_frames[frame%64];
            memcpy(&input,e+1,sizeof(input));
            if(e[0]!=frame || memcmp(input.position,expected,12) || rf_eye_position(&input,calculated) ||
               memcmp(calculated,e+25,12) || memcmp(e+37,rf_scene_actor_look_enabled?(const void*)(rf_scene_actor_look_frames[frame%64]+24):(const void*)input.orientation,36))return RF_FORMAT;
            memcpy(expected,calculated,12);
            if(rf_scene_actor_turn_enabled){printf("TURN_TENSOR %u",frame);for(i=0;i<9;++i){uint32_t w;memcpy(&w,scene_actor_body.state.local_tensor+i,4);printf(" %u",w);}for(i=0;i<9;++i){uint32_t w;memcpy(&w,scene_actor_body.state.world_tensor+i,4);printf(" %u",w);}puts("");}
            if(rf_scene_actor_look_enabled){printf("LOOK_FRAME");for(i=0;i<33;++i)printf(" %u",rf_scene_actor_look_frames[frame%64][i]);puts("");}
            printf("EYE_FRAME");for(i=0;i<46;++i)printf(" %u",e[i]);puts("");
            if(frame && memcmp(e+25,rf_scene_actor_eye_frames[(frame-1)%64]+25,12))++c->changed;
        } else {expected[1]+=.7f;expected[2]+=2.4f;}
        if(rf_scene_particle_view_enabled && frame<400)memcpy(expected,c->particle_camera,12);
        if(r[0]!=frame || r[1]!=world || memcmp(camera,expected,12) || (!rf_scene_actor_eye_enabled && mesh->count==world) || (rf_scene_actor_eye_enabled && mesh->count!=world))return RF_FORMAT;
        for(i=0;i<world;++i)if(mesh->vertices[i].material>=c->base)return RF_FORMAT;
    }
    if(c->body_mode && !rf_scene_actor_drive_enabled && frame>22 && (rf_scene_actor_landing[1]!=1 ||
       scene_actor_body.state.velocity[0]!=0 || scene_actor_body.state.velocity[1]!=0 ||
       scene_actor_body.state.velocity[2]!=0))return RF_FORMAT;
    bytes=(const uint8_t*)(mesh->vertices+world);
    for(i=0;i<mesh->bytes-world*sizeof(rf_preview_vertex);++i)hash=(hash^bytes[i])*16777619u;
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
        status=expected.bytes!=mesh->bytes-world*sizeof(rf_preview_vertex) || memcmp(expected.vertices,bytes,expected.bytes);
        rf_preview_close(&expected);if(status)return RF_FORMAT;
    }
    printf("Frame %u actor triangles %u hash %08x\n",frame,(mesh->count-world)/3,hash);
    return c->stop && frame==2?RF_NOT_FOUND:RF_OK;
}
typedef struct animation_span_check {uint32_t frames,hash,prefix;const void *vertices;} animation_span_check;
static int animation_span_frame(void *context,uint32_t frame,rf_preview_mesh *mesh)
{
    animation_span_check *c=context;uint32_t i;
    if(frame!=c->frames++ || (c->vertices && c->vertices!=mesh->vertices))return RF_FORMAT;
    c->vertices=mesh->vertices;
    for(i=0;i<mesh->bytes;++i)c->hash=(c->hash^((const unsigned char*)mesh->vertices)[i])*16777619u;
    if(frame==63)c->prefix=c->hash;
    return RF_OK;
}
static int replay_player(void *context,uint32_t frame,rf_scene_input *input)
{
    memset(input,0,sizeof(*input));if(context==(void*)2 && frame==3)return RF_NOT_FOUND;if(context)return RF_OK;
    if(frame>=24 && frame<48)input->move[0]=.25f;
    if(frame>=63)input->move[0]=1;
    input->look[0]=frame?((frame%180)<90?.25f:-.25f):0;
    input->look[1]=frame?((frame%240)<120?.2f:-.2f):0;
    input->crouch=frame>=32 && frame<56;return RF_OK;
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
    if(argc==4 && (!strcmp(argv[1],"--retained-world") || !strcmp(argv[1],"--moving-camera"))) {
        int moving_camera=!strcmp(argv[1],"--moving-camera");
        rf_vpp archive;rf_level source,camera;rf_geometry world={0};rf_scene_world_geometry owned={0};
        rf_materials images={0};rf_preview_mesh mesh={0},expected={0},staged={0};rf_geometry_materials mapping={0};
        rf_preview_vertex *scratch,*staged_address;
        rf_group_attached_pose *poses;uint32_t i,j,frame,capacity,hash=2166136261u;void *address;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&source,&archive,argv[3]) ||
            rf_geometry_open(&world,&source,8*1024*1024))return 1;
        camera=source;
        if(rf_scene_world_open_retained(&source,&world,NULL,0,&mesh,&images,8*1024*1024,4*1024*1024,&owned))return 1;
        rf_vpp_close(&archive);memset(&source,0xa5,sizeof(source));rf_materials_close(&images);
        capacity=mesh.bytes+1024*1024;address=realloc(mesh.vertices,capacity);if(!address)return 1;mesh.vertices=address;
        staged_address=malloc(capacity);scratch=malloc(capacity);if(!staged_address || !scratch)return 1;
        staged.vertices=staged_address;
        poses=malloc(owned.movers.count*sizeof(*poses));if(owned.movers.count && !poses)return 1;
        mapping.offsets=owned.offsets;mapping.slots=owned.slots;mapping.count=owned.geometry_count;mapping.textures.count=owned.material_count;
        for(frame=0;frame<(moving_camera?32u:3u);++frame) {
            if(moving_camera) {
                float angle=frame*.01f,cs=cosf(angle),sn=sinf(angle);
                for(j=0;j<3;++j) {
                    camera.player_position[j]=owned.camera_position[j]+frame*(j==0?.1f:j==1?.03f:.05f);
                    camera.player_orientation[0][j]=owned.camera_orientation[0][j]*cs-owned.camera_orientation[2][j]*sn;
                    camera.player_orientation[1][j]=owned.camera_orientation[1][j];
                    camera.player_orientation[2][j]=owned.camera_orientation[0][j]*sn+owned.camera_orientation[2][j]*cs;
                }
            }
            for(i=0;i<owned.movers.count;++i) {
                memset(poses+i,0xa5,sizeof(*poses));
                for(j=0;j<3;++j)poses[i].position[j]=owned.movers.items[i].position[j]+frame*(float)(j+1);
                memcpy(poses[i].output_matrix,owned.movers.items[i].orientation,36);
            }
            if((moving_camera?rf_scene_world_update_camera(&owned,poses,owned.movers.count,camera.player_position,camera.player_orientation,&mesh,capacity):
                rf_scene_world_update(&owned,poses,owned.movers.count,&mesh,capacity)) || mesh.vertices!=address ||
                rf_preview_build_world(&expected,&world,&owned.movers,poses,&mapping,&camera,capacity) ||
                mesh.bytes!=expected.bytes || (mesh.bytes && memcmp(mesh.vertices,expected.vertices,mesh.bytes)))return 3;
            if(rf_scene_world_update_camera_staged(&owned,poses,owned.movers.count,
                camera.player_position,camera.player_orientation,&staged,capacity,scratch,capacity) ||
                staged.vertices!=staged_address || staged.bytes!=expected.bytes ||
                staged.count!=expected.count || memcmp(staged.vertices,expected.vertices,expected.bytes))return 3;
            for(i=0;i<mesh.bytes;++i)hash=(hash^((const unsigned char *)mesh.vertices)[i])*16777619u;
            rf_preview_close(&expected);
        }
        /* Staging failures must preserve every destination byte, including
         * capacity failures after part of the scratch output has been written. */
        {
            rf_preview_mesh before;uint32_t original=staged.bytes;
            memset(staged.vertices,0xa5,capacity);staged.count=staged.bytes=0;before=staged;
            if(rf_preview_update_world_staged(&staged,capacity,scratch,capacity-1,&world,&owned.movers,poses,&mapping,&camera)!=RF_RANGE ||
               rf_preview_update_world_staged(&staged,capacity,staged.vertices,capacity,&world,&owned.movers,poses,&mapping,&camera)!=RF_RANGE ||
               original<sizeof(rf_preview_vertex) ||
               rf_preview_update_world_staged(&staged,original-sizeof(rf_preview_vertex),scratch,capacity,&world,&owned.movers,poses,&mapping,&camera)!=RF_RANGE)return 3;
            if(owned.movers.count) {
                float saved=poses[owned.movers.count-1].output_matrix[0];
                poses[owned.movers.count-1].output_matrix[0]=NAN;
                if(rf_preview_update_world_staged(&staged,capacity,scratch,capacity,&world,&owned.movers,poses,&mapping,&camera)==RF_OK)return 3;
                poses[owned.movers.count-1].output_matrix[0]=saved;
            }
            if(memcmp(&before,&staged,sizeof(before)))return 3;
            for(i=0;i<capacity;++i)if(((unsigned char*)staged.vertices)[i]!=0xa5)return 3;
        }
        if(moving_camera) {
            rf_preview_mesh before=mesh;float bad[3]={NAN,0,0};uint32_t before_hash=2166136261u,after_hash=2166136261u;
            for(i=0;i<mesh.bytes;++i)before_hash=(before_hash^((const unsigned char*)mesh.vertices)[i])*16777619u;
            if(rf_scene_world_update_camera(&owned,poses,owned.movers.count,bad,camera.player_orientation,&mesh,capacity)!=RF_RANGE || memcmp(&before,&mesh,sizeof(mesh)))return 3;
            for(i=0;i<mesh.bytes;++i)after_hash=(after_hash^((const unsigned char*)mesh.vertices)[i])*16777619u;
            if(before_hash!=after_hash)return 3;
            puts("PASS: 32 moving-camera projections match fresh builds after archive closure; fixed mesh allocation and invalid-camera guard");
        }
        printf("%u %u %u %u\n",owned.movers.count,owned.allocated_bytes,capacity,hash);
        free(scratch);rf_preview_close(&staged);free(poses);rf_preview_close(&mesh);rf_scene_world_geometry_close(&owned);rf_scene_world_geometry_close(&owned);
        rf_geometry_close(&world);return 0;
    }
    rf_vpp levels,meshes,maps[5];rf_level level;rf_geometry geometry={0};
    rf_level_actor_assets binding;rf_model_file model;const char *names[64];
    check c={0};uint32_t i,mode;int status,drive=argc==13 && !strcmp(argv[12],"--contact")?2:argc==13 && (!strcmp(argv[12],"--drive") || !strcmp(argv[12],"--traverse") || !strcmp(argv[12],"--live") || !strcmp(argv[12],"--follow") || !strcmp(argv[12],"--eye") || !strcmp(argv[12],"--look") || !strcmp(argv[12],"--turn") || !strcmp(argv[12],"--input") || !strcmp(argv[12],"--neutral") || !strcmp(argv[12],"--input-stop")),body_mode=argc==13 && (!strcmp(argv[12],"--body") || drive);rf_geometry_collision_world body_world={0};
    if(argc!=12 && (argc!=13 || (strcmp(argv[12],"--states") && strcmp(argv[12],"--long-animation") && !body_mode)))return 2;
    c.authored=argc==13;
    rf_scene_particle_view_enabled=getenv("RF_PARTICLE_VIEW")!=NULL;
    c.body_mode=body_mode;
    rf_scene_actor_turn_enabled=argc==13 && (!strcmp(argv[12],"--turn") || !strcmp(argv[12],"--input") || !strcmp(argv[12],"--neutral") || !strcmp(argv[12],"--input-stop"));
    if(argc==13 && (!strcmp(argv[12],"--input") || !strcmp(argv[12],"--neutral") || !strcmp(argv[12],"--input-stop")))
        rf_scene_set_input(replay_player,!strcmp(argv[12],"--input-stop")?(void*)2:!strcmp(argv[12],"--neutral")?(void*)1:NULL,!strcmp(argv[12],"--input-stop")?0:664);
    rf_scene_actor_look_enabled=rf_scene_actor_turn_enabled || (argc==13 && !strcmp(argv[12],"--look"));
    rf_scene_actor_eye_enabled=rf_scene_actor_look_enabled || (argc==13 && !strcmp(argv[12],"--eye"));
    follow_camera=rf_scene_actor_eye_enabled || (argc==13 && !strcmp(argv[12],"--follow"));
    rf_scene_actor_live_enabled=follow_camera || (argc==13 && !strcmp(argv[12],"--live"));
    rf_scene_actor_drive(drive);rf_scene_actor_route_enabled=argc==13 && !strcmp(argv[12],"--traverse");
    c.meshes=argv[4];c.motions=argv[5];
    if(rf_vpp_open(&levels,argv[1]) || rf_level_open(&level,&levels,argv[2]) ||
       (rf_scene_actor_live_enabled?rf_scene_preview_route_camera(&level,(int32_t)strtol(argv[3],NULL,10)):rf_scene_preview_camera(&level,(int32_t)strtol(argv[3],NULL,10))) ||
       rf_geometry_open(&geometry,&level,8*1024*1024) ||
       rf_preview_build(&c.world,&geometry,&level,8*1024*1024) || rf_vpp_open(&meshes,argv[4]))return 3;
    for(i=0;i<5;++i)if(rf_vpp_open(maps+i,argv[7+i]))return 3;
    if(rf_scene_particle_view_enabled) {
        rf_level_emitter_reader reader;rf_level_emitter emitter;
        if(rf_level_emitters_begin(&level,&reader) || rf_level_emitter_next(&reader,&emitter))return 3;
        memcpy(c.particle_camera,emitter.position,12);
    }
    if(rf_scene_actor_live_enabled) {
        rf_materials images={0};rf_preview_close(&c.world);
        if(build_check_world(&level,&geometry,maps,&c.world,&images))return 3;
        rf_materials_close(&images);
        if(follow_camera)rf_scene_actor_follow(&follow_world);
    }
    if(rf_level_actor_assets_load(&level,(int32_t)strtol(argv[3],NULL,10),argv[6],&meshes,512*1024,&binding) ||
       rf_animation_placement_from_level(&level,&binding.entity,&c.placement) ||
       rf_model_file_open(&model,&meshes,binding.mesh.name))return 3;
    for(i=0;i<binding.assets.texture_count;++i)names[i]=binding.assets.textures[i];
    if(rf_model_materials_open_skin(&c.bundle,&model,names,binding.assets.texture_count,maps,5,4*1024*1024))return 3;
    if(body_mode && rf_geometry_collision_world_open(&geometry,8*1024*1024,&body_world))return 3;
    if(argc==13 && !strcmp(argv[12],"--long-animation")) {
        rf_entity_state_set *states=malloc(sizeof(*states));rf_vpp motions;
        uint32_t (*timing)[3]=calloc(600,sizeof(*timing));animation_span_check short_run={0,2166136261u,0,NULL},long_run=short_run;
        if(!states || !timing || rf_vpp_open(&motions,argv[5]) ||
            rf_entity_state_set_open(argv[6],binding.entity.class_name,"",&motions,512*1024,states))return 3;
        c.placement.step_seconds=1.0f/60;c.placement.animation_timing=timing;c.placement.animation_timing_capacity=600;
        if(rf_animation_stream_states(argv[4],argv[5],1024*1024,&c.placement,states,animation_span_frame,&short_run))return 3;
        c.placement.frame_count=600;
        if(rf_animation_stream_states(argv[4],argv[5],1024*1024,&c.placement,states,animation_span_frame,&long_run))return 3;
        if(short_run.frames!=64 || long_run.frames!=600 || short_run.hash!=long_run.prefix || !memcmp(timing[0]+1,timing[64]+1,8))return 3;
        c.placement.animation_timing_capacity=64;
        if(rf_animation_stream_states(argv[4],argv[5],1024*1024,&c.placement,states,animation_span_frame,&long_run)!=RF_RANGE || long_run.frames!=600)return 3;
        printf("PASS: 600 continuous animation frames, matching 64-frame prefix, fixed mesh allocation, persistent clock, timing capacity guard; hash %08x\n",long_run.hash);
        free(timing);free(states);rf_vpp_close(&motions);
        rf_model_materials_close(&c.bundle);rf_preview_close(&c.world);rf_geometry_close(&geometry);
        for(i=0;i<5;++i)rf_vpp_close(maps+i);rf_vpp_close(&meshes);rf_vpp_close(&levels);return 0;
    }
    for(mode=0;mode<3;++mode) {
        rf_preview_mesh mesh={0},before;rf_materials materials={0},saved;
        if(build_check_world(&level,&geometry,maps,&mesh,&materials))return 3;
        c.base=materials.count;c.next=c.changed=c.last=0;c.address=c.material_address=NULL;c.stop=mode==1;
        before=mesh;saved=materials;
        if(body_mode)status=rf_scene_stream_miner_body(&level,binding.entity.uid,argv[4],argv[5],argv[6],maps,5,
            &mesh,&materials,mode==2?mesh.bytes+1024*1024-1:8*1024*1024,4*1024*1024,frame_check,&c,&body_world,&geometry);
        else if(c.authored)status=rf_scene_stream_miner_states(&level,binding.entity.uid,argv[4],argv[5],argv[6],maps,5,
            &mesh,&materials,mode==2?mesh.bytes+1024*1024-1:8*1024*1024,4*1024*1024,frame_check,&c);
        else status=rf_scene_stream_miner(&level,binding.entity.uid,argv[4],argv[5],argv[6],maps,5,
            &mesh,&materials,mode==2?mesh.bytes+1024*1024-1:8*1024*1024,4*1024*1024,frame_check,&c);
        if(mode==0 && body_mode && !status) {
            if(!(rf_scene_actor_initial_eye_offsets[1]>rf_scene_actor_initial_eye_offsets[4]) ||
               !(rf_scene_actor_initial_eye_offsets[4]>0))return 3;
            printf("ACTOR_EYE_OFFSETS");for(i=0;i<6;++i) {uint32_t word;memcpy(&word,rf_scene_actor_initial_eye_offsets+i,4);printf(" %u",word);}puts("");
        }
        if(mode==0 && argc==13 && !strcmp(argv[12],"--input-stop")) {
            if(status || c.next!=3 || !mesh.vertices || !materials.items)return 3;
            rf_preview_close(&mesh);rf_materials_close(&materials);puts("PASS: input provider clean stop after three frames in unbounded stream");break;
        }
        if(mode==0 && rf_scene_actor_live_enabled) {
            if(status || c.next!=664 || !c.changed || rf_scene_actor_tick_stats[1]!=663 || rf_scene_actor_tick_stats[4] ||
               (!rf_scene_actor_turn_enabled && (!rf_scene_actor_landing[7] || rf_scene_actor_landing[1]!=1)) || !rf_scene_actor_landing[3] || rf_scene_actor_landing[3]!=rf_scene_actor_landing[7]+(rf_scene_actor_landing[1]==1?1u:0u))return 3;
            for(i=0;i<64;++i) {
                if(rf_scene_actor_ring_frames[i]<600 || rf_scene_actor_ring_frames[i]>=664 || rf_scene_actor_ring_frames[i]%64!=i ||
                   memcmp(rf_scene_actor_locomotion_frames[i]+3,rf_scene_actor_input_frames[i],12))return 3;
            }
            if(memcmp(rf_scene_actor_render_frames[663%64]+2,scene_actor_body.state.position,12))return 3;
            for(i=0;i<64;++i) {
                const uint32_t *r=rf_scene_actor_room_frames[i];
                if(r[0]!=rf_scene_actor_ring_frames[i] || (r[2]&0x04000000u))return 3;
                if(r[7] && r[6]!=UINT32_MAX && memcmp(r+3,rf_scene_actor_render_frames[i]+2,12))return 3;
            }
            printf("ACTOR_PLAYER_INPUT");for(i=0;i<448;++i)printf(" %u",((uint32_t*)rf_scene_player_input_frames)[i]);puts("");
            if(rf_scene_actor_look_enabled){printf("ACTOR_LOOK_FRAMES");for(i=0;i<64*33;++i)printf(" %u",((uint32_t*)rf_scene_actor_look_frames)[i]);puts("");}
            if(rf_scene_actor_eye_enabled) {printf("ACTOR_EYE_FRAMES");for(i=0;i<64*46;++i)printf(" %u",((uint32_t*)rf_scene_actor_eye_frames)[i]);puts("");}
            printf("ACTOR_ROOMS");for(i=0;i<576;++i)printf(" %u",((uint32_t*)rf_scene_actor_room_frames)[i]);puts("");
            printf("ACTOR_ROOM_SUMMARY");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_room_summary[i]);puts("");
            printf("ACTOR_LIVE_WORLD %u\n",follow_camera?rf_scene_actor_follow_frames[663%64][1]:c.world.count);
            if(follow_camera) {
                printf("ACTOR_FOLLOW");for(i=0;i<896;++i)printf(" %u",((uint32_t*)rf_scene_actor_follow_frames)[i]);puts("");
                printf("ACTOR_FOLLOW_SUMMARY");for(i=0;i<5;++i)printf(" %u",rf_scene_actor_follow_summary[i]);puts("");
                printf("SCENE_VISIBILITY");for(i=0;i<6;++i)printf(" %u",rf_scene_visibility_summary[i]);puts("");
                printf("SCENE_VISIBILITY_FRAMES");for(i=0;i<64*17;++i)printf(" %u",((uint32_t*)rf_scene_visibility_frames)[i]);puts("");
                printf("SCENE_PARTICLE_DRAW");for(i=0;i<7;++i)printf(" %u",rf_scene_particle_draw_summary[i]);puts("");
        printf("SCENE_PARTICLE_DRAW_FRAMES");for(i=0;i<64*6;++i)printf(" %u",((uint32_t*)rf_scene_particle_draw_frames)[i]);puts("");
        printf("SCENE_PARTICLES");for(i=0;i<8;++i)printf(" %u",rf_scene_particles_summary[i]);puts("");
                printf("SCENE_PARTICLE_FRAMES");for(i=0;i<64*12;++i)printf(" %u",((uint32_t*)rf_scene_particles_frames)[i]);puts("");
            }
            printf("ACTOR_LIVE");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_live_summary[i]);puts("");
            printf("ACTOR_LIVE_TICKS");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_tick_stats[i]);puts("");
            {const void *arrays[]={rf_scene_actor_ring_frames,rf_scene_actor_render_frames,rf_scene_actor_animation_timing,
                rf_scene_actor_input_frames,rf_scene_actor_locomotion_frames,rf_scene_actor_selector_frames,
                rf_scene_actor_ground_records,rf_scene_actor_ground_modes,rf_scene_actor_surface_frames,rf_scene_actor_stance_frames};
             const uint32_t counts[]={64,320,192,192,768,512,2112,64,128,256};uint32_t a,j;
             for(a=0;a<10;++a) {printf("ACTOR_LIVE_RING_%u",a);for(j=0;j<counts[a];++j) {uint32_t word;memcpy(&word,(const unsigned char*)arrays[a]+j*4,4);printf(" %u",word);}puts("");}}
            printf("ACTOR_LIVE_BODY");for(i=0;i<77;++i) {uint32_t word;memcpy(&word,(const unsigned char*)&scene_actor_body.state+4*i,4);printf(" %u",word);}puts("");
            rf_preview_close(&mesh);rf_materials_close(&materials);break;
        }
        if(mode==0 && (status || c.next!=64 || !c.changed))return 3;
        if(mode==0) {
            if(body_mode) {if(rf_scene_actor_tick_stats[1]!=63)return 3;
                if(rf_scene_actor_route_enabled) {
                    printf("ACTOR_ROUTES");for(i=0;i<128;++i)printf(" %u",((uint32_t*)rf_scene_actor_routes)[i]);puts("");
                    for(i=0;i<8;++i)if(rf_scene_actor_routes[i][0] || rf_scene_actor_routes[i][1]!=600 || rf_scene_actor_routes[i][5])return 3;
                    if(!rf_scene_actor_routes[0][3] || rf_scene_actor_routes[0][2]!=rf_scene_actor_routes[0][3] || rf_scene_actor_routes[0][6]!=1)return 3;
                }
                printf("ACTOR_TICKS");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_tick_stats[i]);puts("");
                if(rf_scene_actor_ground_stats[1]!=64 || !rf_scene_actor_ground_stats[3])return 3;
                printf("ACTOR_GROUND");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_ground_stats[i]);puts("");}
            if(body_mode) {
                if(!drive && (rf_scene_actor_landing[2]!=22 || rf_scene_actor_landing[3]!=1 || rf_scene_actor_landing[4]!=41))return 3;
                if(drive && rf_scene_actor_landing[6]<3)return 3;
                if(drive==1 && scene_actor_body.state.position[0]<=c.placement.position[0]+.2f)return 3;
                if(drive==2 && (scene_actor_body.state.position[0]>=c.placement.position[0]-.2f || rf_scene_actor_tick_stats[4]))return 3;
                if(drive==2) {
                    float x[6],standing,crouched,restored;uint32_t first=64,last=0,frames[6];
                    for(i=0;i<64;++i)if(rf_scene_actor_stance_frames[i][1]&0x400) {if(first==64)first=i;last=i;}
                    if(first<26 || first==64 || last<=first || last>=62)return 3;
                    frames[0]=first-2;frames[1]=first-1;frames[2]=last-1;frames[3]=last;frames[4]=62;frames[5]=63;
                    for(i=0;i<6;++i)memcpy(x+i,rf_scene_actor_render_frames[frames[i]]+2,4);
                    standing=(x[0]-x[1])*60;crouched=(x[2]-x[3])*60;restored=(x[4]-x[5])*60;
                    if(!(standing>crouched && restored>crouched && crouched>0))return 3;
                    printf("RUN_SPEEDS %.8f %.8f %.8f\n",standing,crouched,restored);
                }
                if(rf_scene_actor_contact_count!=rf_scene_actor_tick_stats[3])return 3;
                printf("ACTOR_CONTACTS %u",rf_scene_actor_contact_count);for(i=0;i<rf_scene_actor_contact_count*25;++i)printf(" %u",((uint32_t*)rf_scene_actor_contacts)[i]);puts("");
                {uint32_t crouch=0,stand=0,blocked=0,first=64;
                 for(i=0;i<64;++i) {if(rf_scene_actor_stance_frames[i][1]&0x400) {++crouch;if(first==64)first=i;}else if(first<64)++stand;blocked+=rf_scene_actor_stance_frames[i][3];}
                 if(!crouch || !stand || rf_scene_actor_stance_cache.count!=3 || rf_scene_actor_stance_cache.height_difference<=0 || rf_scene_actor_stance_frames[first][2]==rf_scene_actor_stance_frames[0][2])return 3;
                 printf("STANCE_SUMMARY %u %u %u\n",crouch,stand,blocked);}
                for(i=0;i<64;++i) {
                    float speed;uint32_t crouched=(rf_scene_actor_stance_frames[i][1]&0x400)!=0;
                    memcpy(&speed,rf_scene_actor_movement_frames[i]+1,4);
                    if(rf_scene_actor_movement_frames[i][2]!=(crouched?0u:1u) || speed!=(crouched?3.0f:6.0f))return 3;
                }
                if(rf_scene_actor_clearance_diagnostic[1]!=2 || rf_scene_actor_clearance_diagnostic[2]!=1 ||
                   rf_scene_actor_clearance_diagnostic[3]!=1 || rf_scene_actor_clearance_diagnostic[7]!=1)return 3;
                {
                    uint32_t crouch_effects=0,stand_effects=0;
                    for(i=0;i<64;++i) {
                        const uint32_t *r=rf_scene_actor_selector_frames[i];
                        if(r[2]==RF_MOTION_STANCE_CROUCH) {
                            ++crouch_effects;if(r[0]!=8 || r[3]!=1 || (r[4]&0x400) || !(r[5]&0x400))return 3;
                        }
                        if(r[2]==RF_MOTION_STANCE_STAND) {
                            ++stand_effects;if(r[3] || !(r[4]&0x400) || (r[5]&0x400))return 3;
                        }
                        if(r[5]!=rf_scene_actor_stance_frames[i][1])return 3;
                    }
                    if(crouch_effects!=1 || stand_effects!=1)return 3;
                }
                if(rf_scene_actor_initial_animation[0]!=0 || rf_scene_actor_initial_animation[1]!=1 ||
                   rf_scene_actor_initial_animation[2]!=0 || rf_scene_actor_initial_animation[3]!=UINT32_MAX ||
                   rf_scene_actor_initial_animation[4]!=0 || rf_scene_actor_initial_animation[9]!=1 ||
                   rf_scene_actor_initial_animation[11]!=0x3f800000)return 3;
                for(i=0;i<64;++i) {
                    const uint32_t *r=rf_scene_actor_locomotion_frames[i];
                    if(rf_scene_actor_selector_frames[i][3]) {if(r[0])return 3;continue;}
                    if(r[0]!=1 || r[1]!=rf_scene_actor_ground_modes[i] ||
                       memcmp(r+3,rf_scene_actor_input_frames[i],12))return 3;
                    if(r[3]==0 && r[4]==0 && r[5]==0) {if(r[9]!=0 && r[10]!=0)return 3;}
                    else if(r[1]==1 && r[2]==1) {if(r[9]!=4 && r[10]!=4)return 3;}
                }
                if(rf_scene_actor_locomotion_frames[16][9]!=0 || rf_scene_actor_locomotion_frames[16][10]!=UINT32_MAX)return 3;
                printf("ACTOR_LOCOMOTION");for(i=0;i<768;++i)printf(" %u",((uint32_t*)rf_scene_actor_locomotion_frames)[i]);puts("");
                for(i=0;i<64;++i) {
                    float dt;memcpy(&dt,rf_scene_actor_animation_timing[i],4);
                    if(dt!=(i?1.0f/60.0f:1.0f/30.0f))return 3;
                }
                {uint32_t queries=0;
                 for(i=0;i<64;++i) {
                    const uint32_t *r=rf_scene_actor_stance_support[i];
                    int changed=rf_scene_actor_selector_frames[i][4]!=rf_scene_actor_selector_frames[i][5];
                    if((r[0]!=0)!=changed)return 3;
                    if(r[0]) {++queries;if(memcmp(r+6,rf_scene_actor_render_frames[i]+2,12))return 3;}
                 }
                 if(queries!=2)return 3;}
                printf("ACTOR_STANCE_SUPPORT");for(i=0;i<576;++i)printf(" %u",((uint32_t*)rf_scene_actor_stance_support)[i]);puts("");
                printf("ACTOR_STANCE_GROUND");for(i=0;i<2112;++i) {uint32_t word;memcpy(&word,rf_scene_actor_stance_ground+i*4,4);printf(" %u",word);}puts("");
                printf("ACTOR_CLOCK");for(i=0;i<192;++i)printf(" %u",((uint32_t*)rf_scene_actor_animation_timing)[i]);puts("");
                printf("ACTOR_INITIAL_ANIMATION");for(i=0;i<12;++i)printf(" %u",rf_scene_actor_initial_animation[i]);puts("");
                printf("ACTOR_SELECTOR");for(i=0;i<512;++i)printf(" %u",((uint32_t*)rf_scene_actor_selector_frames)[i]);puts("");
                printf("ACTOR_CLEARANCE");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_clearance_diagnostic[i]);
                for(i=0;i<24;++i) {uint32_t word;memcpy(&word,((float*)rf_scene_actor_clearance_queries)+i,4);printf(" %u",word);}puts("");
                printf("ACTOR_SURFACES");for(i=0;i<128;++i)printf(" %u",((uint32_t*)rf_scene_actor_surface_frames)[i]);puts("");
                printf("ACTOR_SPEED_MODES");for(i=0;i<192;++i)printf(" %u",((uint32_t*)rf_scene_actor_movement_frames)[i]);puts("");
                printf("ACTOR_STANCE");for(i=0;i<256;++i)printf(" %u",((uint32_t*)rf_scene_actor_stance_frames)[i]);puts("");
                printf("ACTOR_STANCE_CACHE");for(i=0;i<50;++i) {uint32_t word;memcpy(&word,(const unsigned char*)&rf_scene_actor_stance_cache+i*4,4);printf(" %u",word);}puts("");
                printf("ACTOR_LANDING");for(i=0;i<8;++i)printf(" %u",rf_scene_actor_landing[i]);puts("");
                printf("ACTOR_MOVEMENT");for(i=0;i<16;++i) {uint32_t word;memcpy(&word,(const unsigned char*)rf_scene_actor_movement+i*4,4);printf(" %u",word);}puts("");
                printf("ACTOR_SPEED");for(i=0;i<4;++i) {uint32_t word;memcpy(&word,(const unsigned char*)&rf_scene_actor_movement_values+i*4,4);printf(" %u",word);}puts("");
                printf("ACTOR_GROUND_MODES");for(i=0;i<64;++i)printf(" %u",rf_scene_actor_ground_modes[i]);puts("");
                printf("ACTOR_INPUT");for(i=0;i<192;++i) {uint32_t word;memcpy(&word,(const unsigned char*)rf_scene_actor_input_frames+i*4,4);printf(" %u",word);}puts("");
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
    rf_scene_actor_follow(NULL);rf_scene_world_geometry_close(&follow_world);
    rf_geometry_collision_world_close(&body_world);
    rf_model_materials_close(&c.bundle);rf_preview_close(&c.world);rf_geometry_close(&geometry);
    for(i=0;i<5;++i)rf_vpp_close(maps+i);rf_vpp_close(&meshes);rf_vpp_close(&levels);
    if(argc==13 && !strcmp(argv[12],"--input-stop"))return 0;
    puts(rf_scene_actor_live_enabled?"PASS: 664 continuous animated body frames, support bookkeeping, fixed scene allocations":c.authored?"PASS: 64 authored-state scene frames, fixed world and allocations, sink cancellation, capacity guard":
        "PASS: 64 scene frames, fixed world and allocations, four independent pose snapshots, sink cancellation, capacity guard");
    return 0;
}
