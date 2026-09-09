#include "rf/animation_check.h"
#include <fcntl.h>
#include <io.h>
#include <string.h>
typedef struct stream_check {const char *meshes,*motions;void *address;uint32_t next,last_hash,changed;} stream_check;
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
