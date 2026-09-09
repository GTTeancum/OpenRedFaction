#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
typedef struct stream_check {const char *meshes,*motions;void *address;uint32_t next,last_hash,changed;} stream_check;
typedef struct placed_check {uint32_t hashes[64],mode,next,changed;rf_animation_placement *placement;} placed_check;
static int placed_frame(void *context,uint32_t frame,rf_preview_mesh *mesh)
{
    placed_check *check=context;uint32_t i,hash=2166136261u;const uint8_t *bytes=(const uint8_t*)mesh->vertices;
    if(frame!=check->next++ || mesh->bytes!=mesh->count*sizeof(rf_preview_vertex))return RF_FORMAT;
    for(i=0;i<mesh->bytes;++i)hash=(hash^bytes[i])*16777619u;
    if(!check->mode)check->hashes[frame]=hash;
    else if(check->mode==1 && hash!=check->hashes[frame])return RF_FORMAT;
    else if(check->mode==2) {
        if(!frame && hash!=check->hashes[frame])return RF_FORMAT;
        if(hash!=check->hashes[frame])++check->changed;
        check->placement->position[0]+=0.015625f;
    } else if(check->mode==3 && mesh->count)return RF_FORMAT;
    return RF_OK;
}
static int frame_check(void *context,uint32_t frame,rf_preview_mesh *mesh)
{
    stream_check *check=context;uint32_t hash=2166136261u,i;uint8_t *bytes=(uint8_t*)mesh->vertices;
    if(frame!=check->next++ || !mesh->count || mesh->count%3 || mesh->bytes!=mesh->count*sizeof(rf_preview_vertex) || mesh->bytes>1024*1024)return RF_FORMAT;
    if(check->address && check->address!=mesh->vertices)return RF_FORMAT;check->address=mesh->vertices;
    for(i=0;i<mesh->bytes;++i)hash=(hash^bytes[i])*16777619u;
    if(frame && hash!=check->last_hash)++check->changed;check->last_hash=hash;
    if(frame==0 || frame==4 || frame==32 || frame==63) {
        rf_preview_mesh reference={0};int status=rf_animation_preview(check->meshes,check->motions,frame,&reference,1024*1024);
        if(!status && (reference.bytes!=mesh->bytes || memcmp(reference.vertices,mesh->vertices,mesh->bytes)))status=RF_FORMAT;
        rf_preview_close(&reference);if(status)return status;
    }
    printf("%u %u %08x\n",frame,mesh->count,hash);
    for(i=0;i<mesh->count;++i)mesh->vertices[i].material=UINT32_MAX;
    return RF_OK;
}
int main(int argc, char **argv)
{
    uint32_t output[8]; int status;
    if(argc==8 && !strcmp(argv[1],"--level-placement")) {
        rf_vpp archive,meshes;rf_level level;rf_level_actor_assets actor;
        rf_animation_placement placement;rf_model_projection local;rf_preview_mesh mesh={0};uint32_t i,j,n;
        if(rf_vpp_open(&archive,argv[2]))return 3;
        status=rf_level_open(&level,&archive,argv[3]);if(status)return 3;
        if(rf_vpp_open(&meshes,argv[5]))return 3;
        status=rf_level_actor_assets_load(&level,(int32_t)strtol(argv[4],NULL,10),argv[7],&meshes,512*1024,&actor);
        if(status || strcmp(actor.mesh.name,"miner.v3c"))return 3;
        if(rf_animation_placement_from_level(&level,&actor.entity,&placement) ||
           rf_model_local_view(&placement.world_view,placement.position,placement.orientation,&local))return 3;
        for(n=0;n<100;++n) {
            float camera_point[3]={(float)(n%10)-5,(float)(n/10)-5,20+(float)n};
            float world[3],point[3],delta[3],view[3]={0},clip[3];uint8_t vertex[40]={0};uint32_t visible;
            rf_model_render_cache cache={0};
            for(i=0;i<3;++i) {
                world[i]=level.player_position[i];
                for(j=0;j<3;++j)world[i]+=level.player_orientation[j][i]*camera_point[j];
                delta[i]=world[i]-level.player_position[i];
            }
            for(i=0;i<3;++i) {
                point[i]=0;
                for(j=0;j<3;++j) {
                    view[i]+=level.player_orientation[i][j]*delta[j];
                    point[i]+=actor.entity.orientation[i][j]*(world[j]-actor.entity.position[j]);
                }
            }
            if(rf_model_project_vertex(point,&local,&cache,clip,vertex,&visible))return 3;
            if(fabsf(cache.projected[0]-(320+320*view[0]/view[2]))>.02f ||
               fabsf(cache.projected[1]-(240-320*view[1]/view[2]))>.02f)return 3;
        }
        for(n=0;n<2;++n) {
            status=rf_animation_preview_placed(argv[5],argv[6],&placement,n?63:0,&mesh,1024*1024);
            if(status || mesh.count%3)return 3;
            for(i=0;i<mesh.count;++i)if(!isfinite(mesh.vertices[i].position[0]) || !isfinite(mesh.vertices[i].position[1]))return 3;
            printf("UID %d frame %u triangles %u\n",actor.entity.uid,n?63:0,mesh.count/3);rf_preview_close(&mesh);
        }
        {rf_animation_placement before=placement;float saved=level.player_position[0];
         level.player_position[0]=NAN;
         if(rf_animation_placement_from_level(&level,&actor.entity,&placement)!=RF_FORMAT || memcmp(&before,&placement,sizeof(before)))return 3;
         level.player_position[0]=saved;}
        if(rf_animation_preview_placed(argv[5],argv[6],&placement,64,&mesh,1024*1024)!=RF_RANGE || mesh.vertices)return 3;
        for(i=0;i<3;++i)placement.position[i]=level.player_position[i]-1000*level.player_orientation[2][i];
        if(rf_animation_preview_placed(argv[5],argv[6],&placement,0,&mesh,1024*1024) || mesh.count || mesh.bytes)return 3;
        rf_preview_close(&mesh);
        if(rf_animation_placement_from_level(&level,&actor.entity,&placement))return 3;
        {uint32_t visible_cases=0,near_vertices=0;
        for(n=0;n<8;++n) {
            static const float distances[8]={-.2f,0,.1f,.2f,.3f,.4f,.5f,1.0f};
            for(i=0;i<3;++i) {
                placement.world_view.camera[i]=actor.entity.position[i]+distances[n]*actor.entity.orientation[2][i];
                for(j=0;j<3;++j)placement.world_view.rotation[i*3+j]=actor.entity.orientation[i][j]*(i==1?4.0f/3.0f:-1.0f);
            }
            if(rf_animation_preview_placed(argv[5],argv[6],&placement,0,&mesh,1024*1024))return 3;
            if(mesh.count)++visible_cases;
            for(i=0;i<mesh.count;++i) {
                float q=mesh.vertices[i].texture[2];
                if(!isfinite(q) || q<=0 || q>10.001f || !isfinite(mesh.vertices[i].position[2]))return 3;
                if(fabsf(q-10.0f)<.001f)++near_vertices;
            }
            printf("Near camera %.3g triangles %u\n",distances[n],mesh.count/3);rf_preview_close(&mesh);
        }
        if(!visible_cases || !near_vertices)return 3;
        printf("PASS: %u visible near-camera cases, %u vertices on the near plane\n",visible_cases,near_vertices);}
        rf_vpp_close(&meshes);rf_vpp_close(&archive);
        puts("PASS: 100 world/model projection comparisons and two authored-placement snapshots");return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--placed-stream")) {
        rf_animation_placement placement={0};placed_check check={0};uint32_t mode;
        placement.world_view.camera[0]=2.2f;placement.world_view.camera[1]=8;placement.world_view.camera[2]=16;
        placement.world_view.rotation[2]=1;placement.world_view.rotation[4]=1;placement.world_view.rotation[6]=-1;
        placement.world_view.perspective=placement.world_view.compute_clip=placement.world_view.clipping=1;
        placement.world_view.screen[0]=320;placement.world_view.screen[1]=-240;placement.world_view.screen[2]=320;placement.world_view.screen[3]=240;
        placement.position[1]=8;placement.position[2]=16;placement.orientation[2]=-1;placement.orientation[4]=1;placement.orientation[6]=1;
        placement.clip_projection.scale[0]=320;placement.clip_projection.scale[1]=240;placement.clip_projection.clamp=1;check.placement=&placement;
        if(rf_animation_stream(argv[2],argv[3],1024*1024,placed_frame,&check) || check.next!=64)return 3;
        for(mode=1;mode<=3;++mode) {
            check.mode=mode;check.next=0;
            if(mode==3) {placement.position[0]=0;placement.position[2]=-1000;}
            if(rf_animation_stream_placed(argv[2],argv[3],1024*1024,&placement,placed_frame,&check) || check.next!=64)return 3;
        }
        if(check.changed!=63)return 3;
        puts("PASS: 64 translated/rotated equivalent frames, 63 moving-placement differences, 64 culled frames");return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--stream")) {
        stream_check check={argv[2],argv[3],NULL,0,0,0};
        status=rf_animation_stream(argv[2],argv[3],1024*1024,frame_check,&check);
        if(status || check.next!=64 || !check.changed)return 3;
        printf("PASS: %u frames, %u changed transitions, reused mesh, four snapshot comparisons\n",check.next,check.changed);return 0;
    }
    if (argc!=3) return 1;
    _setmode(_fileno(stdout),_O_BINARY);
    status=rf_animation_check(argv[1],argv[2],output);
    if (fwrite(output,sizeof(output),1,stdout)!=1) return 2;
    return status==RF_OK ? 0 : 3;
}
