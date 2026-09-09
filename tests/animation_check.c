#include "rf/animation_check.h"
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
