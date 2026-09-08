#include "rf/animation_check.h"
#include "rf/model.h"
#include "rf/model_file.h"
#include <stdlib.h>
#include <string.h>

static uint32_t hash_bytes(uint32_t hash, const void *bytes, size_t count)
{
    const unsigned char *p=bytes;
    while (count--) hash=(hash ^ *p++)*16777619u;
    return hash;
}
int rf_animation_check(const char *meshes_path, const char *motions_path, uint32_t out[8])
{
    rf_vpp meshes, archive; rf_model_file model; rf_motion_file files[2];
    const rf_motion_file *handles[2]={&files[0],&files[1]};
    rf_motion_playback_resource resources[2]={0}; rf_motion_playback_state state={0};
    rf_motion_controller controller={0,-1,0,0,0,0}; int32_t motions[23];
    rf_model_bone bones[256]; rf_model_attachment eye;
    float matrices[256][12], local[12], tag[12], displacement[3]={.125f,-.25f,.5f};
    uint16_t generations[256]={0}; uint32_t count=0,i,frame; int status,opened=0,found=0;
    void *payload=NULL;
    if (!out) return RF_RANGE;
    memset(out,0,8*4); out[0]=1;
    status=rf_vpp_open(&meshes,meshes_path); if (status!=RF_OK) goto done;
    opened=1; status=rf_vpp_open(&archive,motions_path); if (status!=RF_OK) goto done;
    opened=2; status=rf_model_file_open(&model,&meshes,"miner.v3c"); if (status!=RF_OK) goto done;
    for (i=0;i<model.section_count;++i) if (model.sections[i].type==0x424f4e45) {
        if (model.sections[i].size>4+256*56) { status=RF_RANGE; goto done; }
        out[7]=model.sections[i].size; payload=malloc(out[7]);
        if (!payload) { status=RF_RANGE; goto done; }
        status=rf_vpp_read(&meshes,&model.entry,model.sections[i].offset,payload,out[7]);
        if (status==RF_OK) status=rf_model_decode_bones(payload,out[7],bones,256,&count);
        free(payload); payload=NULL; if (status!=RF_OK) goto done;
    }
    if (!count || !model.lod_count) { status=RF_FORMAT; goto done; }
    out[1]=count;
    for (i=0;i<model.lods[0].attachment_count;++i) {
        status=rf_model_file_attachment(&model,0,i,&eye); if (status!=RF_OK) goto done;
        if (!strcmp(eye.name,"eye")) { found=1; break; }
    }
    if (!found || eye.parent<0 || (uint32_t)eye.parent>=count) { status=RF_FORMAT; goto done; }
    status=rf_model_attachment_transform(eye.rotation,eye.position,local); if (status!=RF_OK) goto done;
    state.completion.active.freeze_slot=-1;
    state.completion.active.primary_slot=state.completion.active.dominant_slot=-1;
    state.phase=.25f; state.generation=1;
    for (i=0;i<2;++i) {
        rf_motion_track track;
        status=rf_motion_file_open(&files[i],&archive,i ? "ult2_crouch.rfa" : "ult2_stand.rfa"); if (status!=RF_OK) goto done;
        status=rf_motion_file_track(&files[i],0,&track); if (status!=RF_OK) goto done;
        resources[i].comparison=track.envelope; resources[i].looping=1;
        resources[i].markers[0]=3200; resources[i].markers[1]=6400;
    }
    for (i=0;i<23;++i) motions[i]=-1;
    motions[0]=0; motions[8]=1;
    for (i=3;i<=6;++i) out[i]=2166136261u;
    for (frame=0;frame<64;++frame) {
        /* Scripted diagnostic requests, not the unrecovered locomotion selector. */
        if (frame==4 || frame==7 || frame==20 || frame==40) {
            status=rf_motion_request_state(&controller,motions,(frame==4 || frame==20) ? 8 : 0,.25f);
            if (status!=RF_OK) goto done;
        }
        controller.override_enabled=frame>=22 && frame<26;
        status=rf_motion_apply_controller(&controller,motions,1.0f/30.0f,&state,resources,2); if (status!=RF_OK) goto done;
        status=rf_motion_update(&state,resources,2,1.0f/30.0f); if (status!=RF_OK) goto done;
        status=rf_model_evaluate_playback(bones,count,&state,handles,resources,2,displacement,matrices,generations,256); if (status!=RF_OK) goto done;
        status=rf_model_compose_transform(local,matrices[eye.parent],tag); if (status!=RF_OK) goto done;
        out[3]=hash_bytes(out[3],matrices,count*48); out[4]=hash_bytes(out[4],&state,sizeof(state)); out[6]=hash_bytes(out[6],tag,48);
        out[4]=hash_bytes(out[4],&controller,sizeof(controller));
        for (i=0;i<2;++i) out[4]=hash_bytes(out[4],&resources[i].references,4);
        displacement[0]=1;
        status=rf_model_evaluate_playback(bones,count,&state,handles,resources,2,displacement,matrices,generations,256); if (status!=RF_OK) goto done;
        if (displacement[0]!=1) { status=RF_FORMAT; goto done; }
        out[5]=hash_bytes(out[5],matrices,count*48); out[5]=hash_bytes(out[5],displacement,12);
        out[5]=hash_bytes(out[5],generations,count*2); displacement[0]=0;
        out[2]=frame+1;
    }
done:
    free(payload);
    if (opened==2) rf_vpp_close(&archive);
    if (opened) rf_vpp_close(&meshes);
    out[0]=status==RF_OK ? 2u : 0x80000000u | (uint32_t)(-status);
    return status;
}
